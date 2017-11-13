/** Copyright (C) 2016, 2017 European Spallation Source ERIC */

/** @file
 *
 *  @brief CSPEC Detector implementation
 */

#include <common/Detector.h>
#include <common/EFUArgs.h>
#include <common/FBSerializer.h>
#include <common/NewStats.h>
#include <common/Producer.h>
#include <common/RingBuffer.h>
#include <common/Trace.h>
#include <multigrid/mgcncs/ChanConv.h>
#include <multigrid/mgcncs/Data.h>
#include <multigrid/mgcncs/MultigridGeometry.h>
//#include <cspec/CSPECEvent.h>
#include <cstring>
#include <iostream>
#include <libs/include/SPSCFifo.h>
#include <libs/include/Socket.h>
#include <libs/include/TSCTimer.h>
#include <libs/include/Timer.h>
#include <memory>
#include <mutex>
#include <queue>
#include <stdio.h>
#include <unistd.h>

//#undef TRC_LEVEL
//#define TRC_LEVEL TRC_L_CRI

using namespace memory_sequential_consistent; // Lock free fifo

const int TSC_MHZ = 2900; // Not accurate, do not rely solely on this

/** ----------------------------------------------------- */
const char *classname = "CSPEC Detector (2 thread pipeline)";

class CSPEC : public Detector {
public:
  CSPEC(void *args);
  void input_thread();
  void processing_thread();

  int statsize();
  int64_t statvalue(size_t index);
  std::string &statname(size_t index);
  const char *detectorname();

  /** @todo figure out the right size  of the .._max_entries  */
  static const int eth_buffer_max_entries = 1000;
  static const int eth_buffer_size = 9000;
  static const int kafka_buffer_size = 1000000;

private:
  /** Shared between input_thread and processing_thread*/
  CircularFifo<unsigned int, eth_buffer_max_entries> input2proc_fifo;
  RingBuffer<eth_buffer_size> *eth_ringbuf;

  std::mutex eventq_mutex, cout_mutex;

  NewStats ns{"efu2.cspec2."};

  struct {
    // Input Counters
    int64_t rx_packets;
    int64_t rx_bytes;
    int64_t fifo_push_errors;
    int64_t fifo_free;
    int64_t pad_a[4]; /**< @todo check alignment*/

    // Processing Counters
    int64_t rx_readouts;
    int64_t rx_error_bytes;
    int64_t rx_discards;
    int64_t rx_idle1;
    int64_t geometry_errors;
    int64_t fifo_seq_errors;
    // Output Counters
    int64_t rx_events;
    int64_t tx_bytes;
  } ALIGN(64) mystats;

  EFUArgs *opts;
};

CSPEC::CSPEC(void *args) {
  opts = (EFUArgs *)args;

  XTRACE(INIT, ALW, "Adding stats\n");
  // clang-format off
  ns.create("input.rx_packets",                &mystats.rx_packets);
  ns.create("input.rx_bytes",                  &mystats.rx_bytes);
  ns.create("input.i2pfifo_dropped",           &mystats.fifo_push_errors);
  ns.create("input.i2pfifo_free",              &mystats.fifo_free);
  ns.create("processing.rx_readouts",          &mystats.rx_readouts);
  ns.create("processing.rx_error_bytes",       &mystats.rx_error_bytes);
  ns.create("processing.rx_discards",          &mystats.rx_discards);
  ns.create("processing.rx_idle",              &mystats.rx_idle1);
  ns.create("processing.rx_geometry_errors",   &mystats.geometry_errors);
  ns.create("processing.fifo_seq_errors",      &mystats.fifo_seq_errors);
  ns.create("output.rx_events",                &mystats.rx_events);
  ns.create("output.tx_bytes",                 &mystats.tx_bytes);
  // clang-format on

  XTRACE(INIT, ALW, "Creating %d Ethernet ringbuffers of size %d\n",
         eth_buffer_max_entries, eth_buffer_size);
  eth_ringbuf = new RingBuffer<eth_buffer_size>(eth_buffer_max_entries + 11);
}

int CSPEC::statsize() { return ns.size(); }

int64_t CSPEC::statvalue(size_t index) { return ns.value(index); }

std::string &CSPEC::statname(size_t index) { return ns.name(index); }

const char *CSPEC::detectorname() { return classname; }

void CSPEC::input_thread() {
  /** Connection setup */
  Socket::Endpoint local(opts->ip_addr.c_str(), opts->port);
  UDPServer cspecdata(local);
  cspecdata.buflen(opts->buflen);
  cspecdata.setbuffers(0, opts->rcvbuf);
  cspecdata.printbuffers();
  cspecdata.settimeout(0, 100000); // One tenth of a second

  int rdsize;
  TSCTimer report_timer;
  for (;;) {
    unsigned int eth_index = eth_ringbuf->getindex();

    /** this is the processing step */
    eth_ringbuf->setdatalength(eth_index, 0);
    if ((rdsize = cspecdata.receive(eth_ringbuf->getdatabuffer(eth_index),
                                    eth_ringbuf->getmaxbufsize())) > 0) {
      eth_ringbuf->setdatalength(eth_index, rdsize);
      mystats.rx_packets++;
      mystats.rx_bytes += rdsize;
      XTRACE(INPUT, DEB, "rdsize: %u\n", rdsize);

      mystats.fifo_free = input2proc_fifo.free();
      if (input2proc_fifo.push(eth_index) == false) {
        mystats.fifo_push_errors++;
      } else {
        eth_ringbuf->nextbuffer();
      }
    }

    // Checking for exit
    if (report_timer.timetsc() >= opts->updint * 1000000 * TSC_MHZ) {

      if (opts->proc_cmd == opts->thread_cmd::THREAD_TERMINATE) {
        XTRACE(INPUT, ALW, "Stopping input thread - stopcmd: %d\n", opts->proc_cmd);
        return;
      }

      report_timer.now();
    }
  }
}

void CSPEC::processing_thread() {
  CSPECChanConv conv;
  Producer producer(opts->broker, "C-SPEC_detector");
  FBSerializer flatbuffer(kafka_buffer_size, producer);

  MultiGridGeometry geom(1, 2, 48, 4, 16);

  CSPECData dat(250, &conv, &geom); // Default signal thresholds

  TSCTimer report_timer;
  TSCTimer timestamp;

  unsigned int data_index;
  while (1) {
    // Check for control from mothership (main)
    if (opts->proc_cmd == opts->thread_cmd::THREAD_LOADCAL) {
      opts->proc_cmd = opts->thread_cmd::NOCMD; /** @todo other means of ipc? */
      XTRACE(PROCESS, INF, "processing_thread loading new calibrations\n");
      conv.load_calibration(opts->wirecal, opts->gridcal);
    }

    if ((input2proc_fifo.pop(data_index)) == false) {
      mystats.rx_idle1++;
      mystats.fifo_free = input2proc_fifo.free();
      usleep(1);
    } else {
      auto len = eth_ringbuf->getdatalength(data_index);
      if (len == 0) {
        mystats.fifo_seq_errors++;
      } else {
        dat.receive(eth_ringbuf->getdatabuffer(data_index),
                    eth_ringbuf->getdatalength(data_index));
        mystats.rx_readouts += dat.elems;
        mystats.rx_error_bytes += dat.error;
        mystats.rx_discards += dat.input_filter();

        for (unsigned int id = 0; id < dat.elems; id++) {
          auto d = dat.data[id];
          if (d.valid) {
            uint32_t tmptime;
            uint32_t tmppixel;
            if (dat.createevent(d, &tmptime, &tmppixel) < 0) {
              mystats.geometry_errors++;
              assert(mystats.geometry_errors <= mystats.rx_readouts);
            } else {
              mystats.tx_bytes += flatbuffer.addevent(tmptime, tmppixel);
              mystats.rx_events++;
            }
          }
        }
      }
    }

    // Checking for exit
    if (report_timer.timetsc() >= opts->updint * 1000000 * TSC_MHZ) {

      mystats.tx_bytes += flatbuffer.produce();

      if (opts->proc_cmd == opts->thread_cmd::THREAD_TERMINATE) {
        XTRACE(INPUT, ALW, "Stopping processing thread - stopcmd: %d\n", opts->proc_cmd);
        return;
      }

      report_timer.now();
    }
  }
}

/** ----------------------------------------------------- */

class CSPECFactory : public DetectorFactory {
public:
  std::shared_ptr<Detector> create(void *args) {
    return std::shared_ptr<Detector>(new CSPEC(args));
  }
};

CSPECFactory Factory;
