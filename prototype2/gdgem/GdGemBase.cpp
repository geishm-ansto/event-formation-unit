/* Copyright (C) 2016-2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// plugin for gdgem detector data reception, parsing and event formation
///
//===----------------------------------------------------------------------===//

#include "GdGemBase.h"

#include <gdgem/clustering/ClusterMatcher.h>
#include <gdgem/clustering/DoroClusterer.h>
#include <gdgem/nmx/TrackSerializer.h>
#include <gdgem/vmm3/BuilderVMM3.h>
#include <common/EV42Serializer.h>
#include <common/HistSerializer.h>
#include <common/Log.h>
#include <common/Producer.h>
#include <common/Trace.h>
#include <efu/Server.h>
#include <libs/include/Socket.h>
#include <libs/include/TSCTimer.h>

const int TSC_MHZ = 2900; // MJC's workstation - not reliable

//#undef TRC_LEVEL
//#define TRC_LEVEL TRC_L_DEB

/** ----------------------------------------------------- */

int GdGemBase::getCalibration(std::vector<std::string> cmdargs,
                        char *output,
                        unsigned int *obytes) {
  std::string cmd = "NMX_GET_CALIB";
  LOG(CMD, Sev::Info, "{}", cmd);
  if (cmdargs.size() != 4) {
    LOG(CMD, Sev::Warning, "{}: wrong number of arguments", cmd);
    return -Parser::EBADARGS;
  }

  int fec = atoi(cmdargs.at(1).c_str());
  int asic = atoi(cmdargs.at(2).c_str());
  int channel = atoi(cmdargs.at(3).c_str());
  auto calib = nmx_opts.calfile->getCalibration(fec, asic, channel);
  if ((abs(calib.offset) <= 1e-6) and (abs(calib.slope) <= 1e-6)) {
    *obytes =
        snprintf(output, SERVER_BUFFER_SIZE, "<error> no calibration exist");
    return -Parser::EBADARGS;
  }

  *obytes = snprintf(output, SERVER_BUFFER_SIZE, "%s offset: %f slope: %f",
                     cmd.c_str(), calib.offset, calib.slope);

  return Parser::OK;
}

GdGemBase::GdGemBase(BaseSettings const &settings, struct NMXSettings &LocalSettings) :
       Detector("NMX", settings), NMXSettings(LocalSettings) {
  Stats.setPrefix("efu.nmx");

  XTRACE(PROCESS, ALW, "NMX Config file: %s", NMXSettings.ConfigFile.c_str());
  nmx_opts = NMXConfig(NMXSettings.ConfigFile, NMXSettings.CalibrationFile);

  XTRACE(INIT, ALW, "Adding stats");
  // clang-format off
  Stats.create("rx_packets", mystats.rx_packets);
  Stats.create("rx_bytes", mystats.rx_bytes);
  Stats.create("i2pfifo_dropped", mystats.fifo_push_errors);
  Stats.create("readouts", mystats.readouts);
  Stats.create("readouts_error_bytes", mystats.readouts_error_bytes);
  Stats.create("readouts_discarded", mystats.readouts_discarded);
  Stats.create("clusters_discarded", mystats.clusters_discarded);
  Stats.create("clusters_events", mystats.clusters_events);
  Stats.create("clusters_x", mystats.clusters_x);
  Stats.create("clusters_y", mystats.clusters_y);
  Stats.create("clusters_xy", mystats.clusters_xy);
  Stats.create("processing_idle", mystats.processing_idle);
  Stats.create("fifo_seq_errors", mystats.fifo_seq_errors);
  Stats.create("unclustered", mystats.unclustered);
  Stats.create("geom_errors", mystats.geom_errors);
  Stats.create("lost_frames", mystats.lost_frames);
  Stats.create("bad_frames", mystats.bad_frames);
  Stats.create("good_frames", mystats.good_frames);
  Stats.create("tx_bytes", mystats.tx_bytes);
  /// \todo below stats are common to all detectors and could/should be moved
  Stats.create("kafka_produce_fails", mystats.kafka_produce_fails);
  Stats.create("kafka_ev_errors", mystats.kafka_ev_errors);
  Stats.create("kafka_ev_others", mystats.kafka_ev_others);
  Stats.create("kafka_dr_errors", mystats.kafka_dr_errors);
  Stats.create("kafka_dr_others", mystats.kafka_dr_noerrors);
  // clang-format on

  std::function<void()> inputFunc = [this]() { GdGemBase::input_thread(); };
  Detector::AddThreadFunction(inputFunc, "input");

  std::function<void()> processingFunc = [this]() { GdGemBase::processing_thread(); };
  Detector::AddThreadFunction(processingFunc, "processing");

  AddCommandFunction("NMX_GET_CALIB",
                     [this](std::vector<std::string> cmdargs, char *output,
                            unsigned int *obytes) {
                       return GdGemBase::getCalibration(cmdargs, output, obytes);
                     });

  XTRACE(INIT, ALW, "Creating %d NMX Rx ringbuffers of size %d",
         eth_buffer_max_entries, eth_buffer_size);
  eth_ringbuf = new RingBuffer<eth_buffer_size>(
      eth_buffer_max_entries + 11); /**< \todo testing workaround */
  assert(eth_ringbuf != 0);
}

void GdGemBase::input_thread() {
  /** Connection setup */
  int rxBuffer, txBuffer;
  Socket::Endpoint local(EFUSettings.DetectorAddress.c_str(),
                         EFUSettings.DetectorPort);
  UDPReceiver nmxdata(local);

  nmxdata.setBufferSizes(0 /*use default */, EFUSettings.DetectorRxBufferSize);
  nmxdata.getBufferSizes(txBuffer, rxBuffer);
  if (rxBuffer < EFUSettings.DetectorRxBufferSize) {
    XTRACE(INIT, ERR, "Receive buffer sizes too small, wanted %d, got %d",
           EFUSettings.DetectorRxBufferSize, rxBuffer);
    return;
  }
  nmxdata.printBufferSizes();
  nmxdata.setRecvTimeout(0, 100000); /// secs, usecs

  TSCTimer report_timer;
  for (;;) {
    int rdsize;
    unsigned int eth_index = eth_ringbuf->getDataIndex();

    /** this is the processing step */
    eth_ringbuf->setDataLength(eth_index,
                               0); /**\todo \todo buffer corruption can occur */
    if ((rdsize = nmxdata.receive(eth_ringbuf->getDataBuffer(eth_index),
                                  eth_ringbuf->getMaxBufSize())) > 0) {
      eth_ringbuf->setDataLength(eth_index, rdsize);
      XTRACE(INPUT, DEB, "rdsize: %d", rdsize);
      mystats.rx_packets++;
      mystats.rx_bytes += rdsize;

      // mystats.fifo_free = input2proc_fifo.free();
      if (input2proc_fifo.push(eth_index) == false) {
        mystats.fifo_push_errors++;
      } else {
        eth_ringbuf->getNextBuffer();
      }
    }

    // Checking for exit
    if (not runThreads) {
      XTRACE(INPUT, ALW, "Stopping input thread.");
      return;
    }
  }
}

void bin(Hists& hists, const Event &e)
{
  uint32_t sum = e.x.adc_sum + e.y.adc_sum;
  hists.bincluster(sum);
}

void bin(Hists& hists, const Hit &e)
{
  if (e.plane_id == 0) {
    hists.binstrips(e.strip, e.adc, 0, 0);
  } else if (e.plane_id == 1) {
    hists.binstrips(0, 0, e.strip, e.adc);
  }
}

void bin_hists(Hists& hists, const std::list<Cluster>& cl)
{
  for (const auto& cluster : cl)
    for (const auto& e : cluster.entries)
      bin(hists, e);
}


void GdGemBase::processing_thread() {
  init_builder();
  if (!builder_) {
    XTRACE(PROCESS, ERR, "No builder specified, exiting thread");
    return;
  }

  Producer event_producer(EFUSettings.KafkaBroker, "NMX_detector");
  Producer monitor_producer(EFUSettings.KafkaBroker, "NMX_monitor");

  EV42Serializer ev42serializer(kafka_buffer_size, "nmx");
  ev42serializer.setProducerCallback(
      std::bind(&Producer::produce2<uint8_t>, &event_producer, std::placeholders::_1));


  TrackSerializer track_serializer(256, nmx_opts.track_sample_minhits,
                          nmx_opts.time_config.target_resolution_ns());
  track_serializer.set_callback(
      std::bind(&Producer::produce2<uint8_t>, &monitor_producer, std::placeholders::_1));

  Hists hists(Hit::strip_max_val, Hit::adc_max_val);
  HistSerializer hist_serializer(hists.needed_buffer_size());
  hist_serializer.set_callback(
      std::bind(&Producer::produce2<uint8_t>, &monitor_producer, std::placeholders::_1));
  hists.set_cluster_adc_downshift(nmx_opts.cluster_adc_downshift);

  ClusterMatcher matcher(nmx_opts.matcher_max_delta_time);

  TSCTimer global_time, report_timer;

  Event event;
  uint32_t time;
  uint32_t pixelid;

  unsigned int data_index;
  bool sample_next_track {nmx_opts.send_tracks};
  while (1) {
    // mystats.fifo_free = input2proc_fifo.free();
    if (!input2proc_fifo.pop(data_index)) {
      mystats.processing_idle++;
      usleep(1);
    } else {
      auto len = eth_ringbuf->getDataLength(data_index);
      if (len == 0) {
        mystats.fifo_seq_errors++;
      } else {
        // printf("received packet with length %d\n", len);
        auto stats = builder_->process_buffer(
            eth_ringbuf->getDataBuffer(data_index), len);

        mystats.readouts += stats.valid_hits;
        mystats.readouts_error_bytes +=
            stats.error_bytes; // From srs data parser
        mystats.lost_frames += stats.lost_frames;
        mystats.bad_frames += stats.bad_frames;
        mystats.good_frames += stats.good_frames;

        if (nmx_opts.hit_histograms) {
          bin_hists(hists, builder_->clusterer_x->clusters);
          bin_hists(hists, builder_->clusterer_y->clusters);
        }

        if (!builder_->clusterer_x->empty() &&
            !builder_->clusterer_y->empty()) {
          matcher.merge(0, builder_->clusterer_x->clusters);
          matcher.merge(1, builder_->clusterer_y->clusters);
        }
        matcher.match_end(false);

        while (!matcher.matched_clusters.empty()) {
          // printf("MATCHED_CLUSTERS\n");
          XTRACE(PROCESS, DEB, "event_ready()");
          event = matcher.matched_clusters.front();
          matcher.matched_clusters.pop_front();

          // mystats.unclustered = clusterer.unclustered();

          event.analyze(nmx_opts.analyze_weighted,
                        nmx_opts.analyze_max_timebins,
                        nmx_opts.analyze_max_timedif);

          if (nmx_opts.hit_histograms) {
            bin(hists, event);
          }

          if (event.valid()) {
            XTRACE(PROCESS, DEB, "event.good");

            mystats.clusters_xy++;
	   
            // TODO: Should it be here or outside of event.valid()?
            if (sample_next_track) {
              sample_next_track = !track_serializer.add_track(event);
            }
		
            XTRACE(PROCESS, DEB, "x.center: %d, y.center %d",
                   event.x.utpc_center_rounded(),
                   event.y.utpc_center_rounded());

            if (nmx_opts.filter.valid(event)) {
              pixelid = nmx_opts.geometry.pixel2D(
                  event.x.utpc_center_rounded(), event.y.utpc_center_rounded());
		
              if (!nmx_opts.geometry.valid_id(pixelid)) {
                mystats.geom_errors++;
              } else {
                time = static_cast<uint32_t>(event.utpc_time());
 		
                XTRACE(PROCESS, DEB, "time: %d, pixelid %d", time, pixelid);

                mystats.tx_bytes += ev42serializer.addEvent(time, pixelid);
                mystats.clusters_events++;
              }
            } else { // Does not meet criteria
              /** \todo increments counters when failing this */
            }
          } else { /// no valid event
            if (event.x.entries.size() != 0) {
              mystats.clusters_x++;
            } else {
              mystats.clusters_y++;
            }
            mystats.readouts_discarded +=
                event.x.entries.size() + event.y.entries.size();
            mystats.clusters_discarded++;
          }
        }
      }
    }

    // Flush on interval or exit
    if ((not runThreads) || (report_timer.timetsc() >=
        EFUSettings.UpdateIntervalSec * 1000000 * TSC_MHZ)) {

      /// \todo in case of exit, flush all clusters first

      sample_next_track = nmx_opts.send_tracks;

      mystats.tx_bytes += ev42serializer.produce();

      /// Kafka stats update - common to all detectors
      /// don't increment as producer keeps absolute count
      mystats.kafka_produce_fails = event_producer.stats.produce_fails;
      mystats.kafka_ev_errors = event_producer.stats.ev_errors;
      mystats.kafka_ev_others = event_producer.stats.ev_others;
      mystats.kafka_dr_errors = event_producer.stats.dr_errors;
      mystats.kafka_dr_noerrors = event_producer.stats.dr_noerrors;

      if (!hists.isEmpty()) {
        XTRACE(PROCESS, DEB, "Sending histogram for %zu hits and %zu clusters ",
               hists.hit_count(), hists.cluster_count());
        hist_serializer.produce(hists);
        hists.clear();
      }

      // checking for exit
      if (not runThreads) {
        XTRACE(INPUT, ALW, "Stopping input thread.");
        /// \todo this is a hack to force ~BuilderSRS() call
        builder_.reset();
        delete builder_.get(); /**< \todo see above */
        return;
      }

      report_timer.now();
    }
  }
}

void GdGemBase::init_builder() {
  XTRACE(INIT, ALW, "NMXConfig:\n%s", nmx_opts.debug().c_str());

  auto clusx = std::make_shared<DoroClusterer>(
      nmx_opts.clusterer_x.max_time_gap, nmx_opts.clusterer_x.max_strip_gap,
      nmx_opts.clusterer_x.min_cluster_size);
  auto clusy = std::make_shared<DoroClusterer>(
      nmx_opts.clusterer_y.max_time_gap, nmx_opts.clusterer_y.max_strip_gap,
      nmx_opts.clusterer_y.min_cluster_size);

if (nmx_opts.builder_type == "VMM3") {
    XTRACE(INIT, DEB, "Using BuilderVMM3");
    builder_ = std::make_shared<BuilderVMM3>(
        nmx_opts.time_config, nmx_opts.srs_mappings, clusx, clusy,
        nmx_opts.clusterer_x.hit_adc_threshold,
        nmx_opts.clusterer_x.max_time_gap,
        nmx_opts.clusterer_y.hit_adc_threshold,
        nmx_opts.clusterer_y.max_time_gap, nmx_opts.dump_directory,
        nmx_opts.dump_csv, nmx_opts.dump_h5, nmx_opts.calfile);
  } else {
    XTRACE(INIT, ALW, "Unrecognized builder type in config");
  }
}
