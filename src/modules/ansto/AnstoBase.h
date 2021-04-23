/* Copyright (C) 2019 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief Ansto detector base plugin interface definition
///
//===----------------------------------------------------------------------===//
#pragma once

#include <ansto/common/Config.h>
#include <cctype>
#include <common/Detector.h>
#include <common/EV42Serializer.h>
#include <common/RingBuffer.h>
#include <common/SPSCFifo.h>

namespace Ansto {

struct AnstoSettings {
  std::string FilePrefix{""};
  std::string ConfigFile{""};
  uint32_t H5SplitTime{
      0}; // split dumpfile file every N seconds (0 is inactive)
};

using namespace memory_sequential_consistent; // Lock free fifo

/// Main processing loop for the Ansto detectors.
class AnstoBase : public Detector {
public:
  /// \brief Create base from settings
  ///
  /// \param settings
  /// \param LocalAnstoSettings
  AnstoBase(BaseSettings const &settings,
            struct AnstoSettings &LocalAnstoSettings);
  ~AnstoBase() { delete EthernetRingbuffer; }

protected:
  /** @todo figure out the right size  of the .._max_entries  */
  static constexpr int EthernetBufferMaxEntries{1000};
  static constexpr int EthernetBufferSize{9000};       /// bytes

  void InputThread();
  void ProcessingThread();
  uint64_t CheckPulseTime(EV42Serializer &flatbuffer, uint64_t baseTime);

  /** Shared between input_thread and processing_thread*/
  CircularFifo<unsigned int, EthernetBufferMaxEntries> InputFifo;
  RingBuffer<EthernetBufferSize> *EthernetRingbuffer;

  struct {
    // Input Counters - accessed in input thread
    int64_t RxPackets;
    int64_t RxBytes;
    int64_t FifoPushErrors;
    int64_t PaddingFor64ByteAlignment[5];
    // cppcheck-suppress unusedStructMember

    // Processing Counters - accessed in processing thread
    int64_t TxBytes;
    int64_t Events;
    int64_t Frames;
    int64_t AuxFrames;
    int64_t FifoSeqErrors;
    int64_t ReadoutsErrorBytes;
    int64_t ReadoutsSeqErrors;
    int64_t EventErrors;
    int64_t DroppedEvents;
    int64_t BeamMonitorCount;
    int64_t CustomEvents;
  } __attribute__((aligned(64))) Counters ;

  AnstoSettings Settings;
  Ansto::Config BaseConfig;
};

} // namespace Ansto
