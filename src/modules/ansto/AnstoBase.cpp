/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// ansto event formation unit base
//===----------------------------------------------------------------------===//

#include "AnstoBase.h"

#include <cinttypes>
#include <common/EFUArgs.h>
#include <common/EV42Serializer.h>
#include <common/Log.h>
#include <common/Producer.h>
#include <common/RingBuffer.h>
#include <common/SENVSerializer.h>
#include <common/TDCTSerializer.h>
#include <common/TimeString.h>
#include <common/Trace.h>
#include <common/monitor/HistogramSerializer.h>

#include <functional>
#include <unistd.h>

#include <common/SPSCFifo.h>
#include <common/Socket.h>
#include <common/TSCTimer.h>
#include <common/Timer.h>
#include <common/ZmqInput.h>

#include <ansto/common/Clusterer.h>
#include <ansto/common/EventBinning.h>
#include <ansto/common/ParserFactory.h>
#include <ansto/common/Readout.h>

namespace Ansto {

using namespace memory_sequential_consistent; // Lock free fifo

static constexpr int KafkaNeutronBufferSize{124000}; /// entries ~ 1MB
static constexpr int KafkaChopperBufferSize{1240};   /// entries ~ 0.01MB
static constexpr int KafkaOtherBufferSize{1024};     /// entries < 0.016Mb

struct DetectorParams {
  DetectorParams(const nlohmann::json &root) {
    BasePixel = root["BasePixel"];
    XPixels = root["XPixels"];
    YPixels = root["YPixels"];
    EventLag = root["EventLag"].get<uint64_t>() * 1'000'000; // msec to nsec
    TOFMode = root.value("TOFMode", false);
    MinTOF = root.value("MinTOF", 3'000) * 1000; // usec to nsec
    KafkaNeutronEvents = root.value("KafkaNeutronEvents", 124000);
  }

  uint32_t BasePixel;
  uint16_t XPixels, YPixels;
  uint64_t EventLag;
  bool TOFMode;
  uint64_t MinTOF;
  int KafkaNeutronEvents;
};

struct ClusterParams {
  ClusterParams(const nlohmann::json &root) {
    Enabled = root["Enabled"];
    NumThreads = root["NumThreads"];
    SpanT = root["MaxSpan"];
    MaxDT = root["MaxDT"];
    MaxDX = root["MaxDX"];
    MaxDY = root["MaxDY"];
    ActiveClusters = root["ActiveClusters"];
  }
  bool Enabled;
  int NumThreads;
  size_t ActiveClusters;
  uint64_t SpanT, MaxDT, MaxDX, MaxDY;
};

// const char *classname = "Ansto EFU";
const int TSC_MHZ = 2900; // MJC's workstation - not reliable

AnstoBase::AnstoBase(BaseSettings const &settings,
                     struct AnstoSettings &localAnstoSettings)
    : Detector("Ansto", settings), Settings(localAnstoSettings),
      BaseConfig(localAnstoSettings.ConfigFile) {

  Stats.setPrefix(EFUSettings.GraphitePrefix, EFUSettings.GraphiteRegion);

  XTRACE(INIT, ALW, "Adding stats");
  Stats.create("receive.packets", Counters.RxPackets);
  Stats.create("receive.bytes", Counters.RxBytes);
  Stats.create("receive.dropped", Counters.FifoPushErrors);
  Stats.create("receive.fifo_seq_errors", Counters.FifoSeqErrors);
  Stats.create("readouts.error_bytes", Counters.ReadoutsErrorBytes);
  Stats.create("readouts.seq_errors", Counters.ReadoutsSeqErrors);
  Stats.create("transmit.bytes", Counters.TxBytes);
  Stats.create("events.count", Counters.Events);
  Stats.create("events.errors", Counters.EventErrors);
  Stats.create("events.dropped", Counters.DroppedEvents);
  Stats.create("frames.primary", Counters.Frames);
  Stats.create("frames.auxillary", Counters.AuxFrames);
  Stats.create("custom_events", Counters.CustomEvents);
  Stats.create("beam_monitor", Counters.BeamMonitorCount);

  XTRACE(INIT, ALW, "Creating %d Ansto Rx ringbuffers of size %d",
         EthernetBufferMaxEntries, EthernetBufferSize);
  /// \todo the number 11 is a workaround
  EthernetRingbuffer =
      new RingBuffer<EthernetBufferSize>(EthernetBufferMaxEntries + 11);
  assert(EthernetRingbuffer != 0);

  std::function<void()> inputFunc = [this]() { AnstoBase::InputThread(); };
  Detector::AddThreadFunction(inputFunc, "input");

  std::function<void()> processingFunc = [this]() {
    AnstoBase::ProcessingThread();
  };
  Detector::AddThreadFunction(processingFunc, "processing");
}

// The sole responsibility for the input thread to is capture
// the data from the communications channel to the ring buffer.
void AnstoBase::InputThread() {
  // All comms channels are over an IP and port.
  Socket::Endpoint local(EFUSettings.DetectorAddress.c_str(),
                         EFUSettings.DetectorPort);

  void *receiverObject;
  std::unique_ptr<ZMQSubscriber> zmqPtr;
  std::unique_ptr<UDPReceiver> udpPtr;
  std::function<ssize_t(void *, void *, int)> Receiver;

  auto comms = BaseConfig.getDAEComms();
  if (comms == "zmq") {
    zmqPtr = std::make_unique<ZMQSubscriber>(local);
    Receiver = InvokeReceive<ZMQSubscriber>;
    receiverObject = static_cast<void *>(&(*zmqPtr));
  } else {
    udpPtr = std::make_unique<UDPReceiver>(local);
    udpPtr->setBufferSizes(0, EFUSettings.RxSocketBufferSize);
    udpPtr->checkRxBufferSizes(EFUSettings.RxSocketBufferSize);
    udpPtr->printBufferSizes();
    udpPtr->setRecvTimeout(0, 100000); /// secs, usecs 1/10s
    Receiver = InvokeReceive<UDPReceiver>;
    receiverObject = static_cast<void *>(&(*udpPtr));
  }

  for (;;) {
    int rdsize;
    unsigned int eth_index = EthernetRingbuffer->getDataIndex();

    EthernetRingbuffer->setDataLength(eth_index, 0);
    if ((rdsize = Receiver(receiverObject,
                           EthernetRingbuffer->getDataBuffer(eth_index),
                           EthernetRingbuffer->getMaxBufSize())) > 0) {
      EthernetRingbuffer->setDataLength(eth_index, rdsize);
      XTRACE(INPUT, DEB, "Received an udp packet of length %d bytes", rdsize);
      Counters.RxPackets++;
      Counters.RxBytes += rdsize;

      if (InputFifo.push(eth_index) == false) {
        Counters.FifoPushErrors++;
      } else {
        EthernetRingbuffer->getNextBuffer();
      }
    } else if (rdsize < 0) {
      usleep(1000000);
    }

    // Checking for exit
    if (not runThreads) {
      XTRACE(INPUT, ALW, "Stopping input thread.");
      return;
    }
  }
}

// After creating the kafka topic producers, comms parser, event bins,
// and clusterer (if needed) it processes the messages in the ring buffer.
// Each message buffer is parsed and the events binned by time to
// partially sort the data. Events in separate bins are ordered but not
// events in a single bin. All the events in a single bin are clustered
// and forwarded to kafka.
void AnstoBase::ProcessingThread() {

  const struct DetectorParams detectorPM(BaseConfig.getRoot("Detector"));

  Producer eventProd(EFUSettings.KafkaBroker, "neutron_events");
  auto Produce = [&eventProd](auto dataBuffer, auto timestamp) {
    eventProd.produce(dataBuffer, timestamp);
  };
  EV42Serializer flatbuffer(detectorPM.KafkaNeutronEvents, "Ansto", Produce);

  Producer priChopper(EFUSettings.KafkaBroker, "primary_chopper");
  auto PXProduce = [&priChopper](auto dataBuffer, auto timestamp) {
    priChopper.produce(dataBuffer, timestamp);
  };
  TDCTSerializer pxbuffer(KafkaChopperBufferSize, "Ansto", PXProduce);

  Producer auxChopper(EFUSettings.KafkaBroker, "auxillary_chopper");
  auto AXProduce = [&auxChopper](auto dataBuffer, auto timestamp) {
    auxChopper.produce(dataBuffer, timestamp);
  };
  TDCTSerializer axbuffer(KafkaChopperBufferSize, "Ansto", AXProduce);

  // Should use uint32_t for the beam monitor when available
  Producer beamMonitor(EFUSettings.KafkaBroker, "beam_monitor");
  auto BMProduce = [&beamMonitor](auto dataBuffer, auto timestamp) {
    beamMonitor.produce(dataBuffer, timestamp);
  };
  SENVSerializerU32 bmbuffer(KafkaOtherBufferSize, "Ansto", BMProduce);

  // Should use uint64_t for the customer events when available
  Producer customEvents(EFUSettings.KafkaBroker, "custom_events");
  auto CEProduce = [&customEvents](auto dataBuffer, auto timestamp) {
    customEvents.produce(dataBuffer, timestamp);
  };
  SENVSerializerU64 cebuffer(KafkaOtherBufferSize, "Ansto", CEProduce);

  auto reader = BaseConfig.getReader();
  auto parser = ParserMethodFactory::create(reader, BaseConfig.getConfigFile());
  if (parser == nullptr) {
    XTRACE(PROCESS, ALW, "Cannot launch reader %s for instrument %s",
           reader.c_str(), BaseConfig.getInstrument().c_str());
    return;
  }

  std::shared_ptr<ReadoutFile> dumpfile;
  if (!Settings.FilePrefix.empty()) {
    dumpfile = ReadoutFile::create(Settings.FilePrefix + "-" + timeString());
  }

  // Setup the event binner and clustering algorithm if needed
  const struct ClusterParams clusterPM(BaseConfig.getRoot("Clusters"));
  std::unique_ptr<ParallelClusterer> clusterer;
  if (clusterPM.Enabled) {
    clusterer = std::make_unique<ParallelClusterer>(
        clusterPM.NumThreads, clusterPM.SpanT, clusterPM.MaxDT, clusterPM.MaxDX,
        clusterPM.MaxDY, clusterPM.ActiveClusters);
  }

  auto binningPM = BaseConfig.getRoot("EventBinning");
  const int fineWidthBits = binningPM["FineWidthBits"];
  Ansto::EventBinning EventDemux(binningPM["CoarseBinBits"],
                                 binningPM["FineWidthBits"],
                                 binningPM["FineBinBits"]);
  const uint64_t CCoarseBinWidth = EventDemux.getCoarseBinWidth();

  uint data_index;
  TSCTimer produce_timer;
  Timer h5flushtimer;
  constexpr uint MicroSleep{10};
  uint DemuxTimeout = BaseConfig.getRoot("Detector")["DemuxTimeout"];
  uint SleepLoops = DemuxTimeout * 1000 / MicroSleep;
  uint countdown = SleepLoops;
  std::vector<Readout> OutputEvents;

  // Frame events are used to manage the TOF conversion from the primary chopper
  std::deque<uint64_t> FrameEvents;
  uint64_t CurFrame{0};
  uint64_t EventBaseTime{0};
  const uint64_t TOFDelay = detectorPM.MinTOF + EventDemux.getFineBinWidth();

  // All timestamps are ordered up to the fine bin width so only check
  // if the chopper base should be changed on a transition. The chopper
  // timestamps should be popped until it finds the maximal chopper time
  // less than the event time. Hopefully it should only need 1 step.
  auto setPulseTime = [&flatbuffer](uint64_t ts) {
    if (flatbuffer.pulseTime() != ts) {
      flatbuffer.produce();
      flatbuffer.pulseTime(ts);
    }
  };
  auto checkChopperPulse = [&](uint64_t ts) {
    uint64_t tsFloor = ((ts >> fineWidthBits) << fineWidthBits);
    if (EventBaseTime < tsFloor) {
      EventBaseTime = tsFloor;
      int shift{0};
      while (!FrameEvents.empty() && (FrameEvents.front() + TOFDelay <= EventBaseTime)) {
        CurFrame = FrameEvents.front();
        FrameEvents.pop_front();
        shift++;
      }
      if (shift > 0)
        setPulseTime(CurFrame);
    }
    return CurFrame;
  };

  while (true) {
    if (InputFifo.pop(data_index)) {
      auto datalen = EthernetRingbuffer->getDataLength(data_index);
      if (datalen == 0) {
        Counters.FifoSeqErrors++;
        continue;
      }

      // Parse the data and update the errors to the stats.
      uint64_t TSmin = -1;
      uint64_t TSmax = 0;
      auto dataptr = EthernetRingbuffer->getDataBuffer(data_index);
      auto result = parser->parse(dataptr, datalen, TSmin, TSmax);
      Counters.ReadoutsErrorBytes += parser->byteErrors();
      Counters.ReadoutsSeqErrors += parser->seqErrors();
      if (result < 0) {
        continue;
      }

      // if there are events then reset the counter because it is
      // ticking over
      auto readouts = parser->getReadouts();
      if (readouts.size() == 0) {
        continue;
      }
      countdown = SleepLoops;

      // Get the base time for the readout timestamp using the coarse bin width
      // and reset the base time if necessary.
      uint64_t readoutBase = (TSmin / CCoarseBinWidth) * CCoarseBinWidth;
      if (!EventDemux.validEventTime(readoutBase)) {
        LOG(INIT, Sev::Warning, "Reset Event bins:  {} ms -> {} ms",
            EventDemux.getBaseTime() / 1'000'000UL, readoutBase / 1'000'000UL);
        int64_t dropped = EventDemux.resetBins(readoutBase);
        Counters.DroppedEvents += dropped;

        FrameEvents.clear();
        CurFrame = 0;
        EventBaseTime = 0;
      }

      // Insert the events to the coarse time bins and check for dropped events
      auto activeEvents = EventDemux.insert(readouts);
      if (readouts.size() > activeEvents)
        Counters.DroppedEvents += (int64_t)(readouts.size() - activeEvents);

    } else {
      // There is NO data in the FIFO - decrement the counter to
      // ensure the bins will be dumped eventually, and sleep a little
      if (countdown > 0)
        countdown--;
      usleep(MicroSleep);
    }

    // Determine the block size in nsecs that can be processed
    // having allowed for the lag in module comms. The minimum
    // blocksize is a coarse bin width.
    uint64_t BlockSpan{0};
    if (EventDemux.timeSpan() > (detectorPM.EventLag + CCoarseBinWidth)) {
      BlockSpan = EventDemux.timeSpan() - detectorPM.EventLag;
    } else if ((!EventDemux.empty() && countdown == 0)) {
      BlockSpan = MicroSleep * SleepLoops * 1000; // in nsecs
    }

    OutputEvents.clear();
    auto OutputBaseTime = EventDemux.getBaseTime();
    if (clusterPM.Enabled) {
      size_t NumCoarseBins = BlockSpan / CCoarseBinWidth;
      for (size_t ix = 0; ix < NumCoarseBins; ix++) {
        EventDemux.mapToFineBins();
        clusterer->ProcessEvents(EventDemux.getBaseTime(),
                                 EventDemux.getFineBinWidth(),
                                 EventDemux.getFineBins(), OutputEvents);
        EventDemux.clearFineBins();
        EventDemux.incrementBase();
      }
    } else {
      EventDemux.dumpBlock(BlockSpan, OutputEvents);
    }

    if (dumpfile && !OutputEvents.empty() > 0) {
      dumpfile->push(OutputEvents);
    }

    // If the output is not empty, set the basetime for neutron event
    // flatbuffer, fill and send the flatbuffers to kafka
    if (OutputEvents.size() > 0) {
      uint64_t pulseTime{0};
      if (!detectorPM.TOFMode) {
        pulseTime = CheckPulseTime(flatbuffer, OutputBaseTime);
      }
      uint32_t pixelId{0};
      for (auto &ev : OutputEvents) {
        switch (ev.event_type) {
        case Ansto::Neutron:
          // In TOF mode if there is no valid chopper time drop the event check
          // that pulse
          if (detectorPM.TOFMode) {
            pulseTime = checkChopperPulse(ev.timestamp);
          }
          if (pulseTime > 0) {
            pixelId = parser->pixelID(ev.x_posn, ev.y_posn);
            Counters.TxBytes +=
                flatbuffer.addEvent(ev.timestamp - pulseTime, pixelId);
            Counters.Events++;
          } else {
            Counters.EventErrors++;
          }
          break;
        case Ansto::FrameStart:
          pxbuffer.addEvent(ev.timestamp);
          if (detectorPM.TOFMode) {
            FrameEvents.push_back(ev.timestamp);
            if (CurFrame == 0) {
              CurFrame = FrameEvents.front();
              FrameEvents.pop_front();
              setPulseTime(CurFrame);
            }
          }
          Counters.Frames++;
          break;
        case Ansto::FrameAuxStart:
          axbuffer.addEvent(ev.timestamp);
          Counters.AuxFrames++;
          break;
        case Ansto::Blank:
          break;
        case Ansto::BeamMonitor:
          bmbuffer.addEvent(ev.timestamp, ev.data);
          Counters.BeamMonitorCount++;
          break;
        case Ansto::OtherEvent:
          cebuffer.addEvent(ev.timestamp, ev.data);
          Counters.CustomEvents++;
          break;
        default:
          // do something?
          Counters.EventErrors++;
        }
      }
      OutputEvents.clear();
    }

    // if filedumping and requesting time splitting, check for rotation.
    if (Settings.H5SplitTime != 0 and (dumpfile)) {
      if (h5flushtimer.timeus() >= Settings.H5SplitTime * 1000000) {

        /// \todo user should not need to call flush() - implicit in rotate() ?
        dumpfile->flush();
        dumpfile->rotate();
        h5flushtimer.now();
      }
    }

    if (produce_timer.timetsc() >=
        EFUSettings.UpdateIntervalSec * 1000000 * TSC_MHZ) {
      // Note that the pulseTime is only updated because of the regular timer
      // that dumps the data so that eventCount is zero
      Counters.TxBytes += flatbuffer.produce();
      pxbuffer.produce();
      axbuffer.produce();

      produce_timer.now();
    }

    if (not runThreads) {
      // \todo flush everything here
      XTRACE(INPUT, ALW, "Stopping processing thread.");
      return;
    }
  }
}

// The function returns the pulse time for the flatbuffer but there are cases
// that flush the flatbuffer and reset the pulse time:
//  - if the base time is less than the pulse time or,
//  - the base time is more than 1 second ahead of the pulse time
//
uint64_t AnstoBase::CheckPulseTime(EV42Serializer &flatbuffer,
                                   uint64_t baseTime) {

  if (flatbuffer.eventCount() == 0) {
    flatbuffer.pulseTime(baseTime);
  } else {
    constexpr uint64_t OneSecond = 1'000'000'000;
    uint64_t pulseTime = flatbuffer.pulseTime();
    if ((baseTime < pulseTime) || (baseTime > pulseTime + OneSecond)) {
      flatbuffer.produce();
      flatbuffer.pulseTime(baseTime);
    }
  }
  return flatbuffer.pulseTime();
}

} // namespace Ansto
