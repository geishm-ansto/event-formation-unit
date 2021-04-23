/** Copyright (C) 2016, 2017 European Spallation Source ERIC */

#include "tdct_timestamps_generated.h"
#include <common/TDCTSerializer.h>
#include <common/gccintel.h>

#include <common/Trace.h>
//#undef TRC_LEVEL
//#define TRC_LEVEL TRC_L_DEB

static constexpr size_t TimeSize = sizeof(uint64_t);

// These must be non-0 because of Flatbuffers being stupid
// If they are initially set to 0, they will not be mutable
static constexpr uint64_t FBMutablePlaceholder = 1;

static_assert(FLATBUFFERS_LITTLEENDIAN,
              "Flatbuffers only tested on little endian systems");

TDCTSerializer::TDCTSerializer(size_t MaxArrayLength, std::string Name,
                               ProducerCallback Callback)
    : MaxEvents(MaxArrayLength), Builder_(MaxEvents * 8 + 256),
      ProduceFunctor(Callback) {

  auto NameOffset = Builder_.CreateString(Name);
  auto TimeOffset =
      Builder_.CreateUninitializedVector(MaxEvents, TimeSize, &TimePtr);

  auto HeaderOffset =
      Createtimestamp(Builder_, NameOffset, TimeOffset, FBMutablePlaceholder);
  FinishtimestampBuffer(Builder_, HeaderOffset);

  Buffer_ = nonstd::span<const uint8_t>(Builder_.GetBufferPointer(),
                                        Builder_.GetSize());

  timestamp_ =
      const_cast<timestamp *>(Gettimestamp(Builder_.GetBufferPointer()));
  TimeLengthPtr =
      reinterpret_cast<flatbuffers::uoffset_t *>(
          const_cast<std::uint8_t *>(timestamp_->timestamps()->Data())) -
      1;

  timestamp_->mutate_sequence_counter(0);
}

void TDCTSerializer::setProducerCallback(ProducerCallback Callback) {
  ProduceFunctor = Callback;
}

nonstd::span<const uint8_t> TDCTSerializer::serialize() {
  if (EventCount > MaxEvents) {
    // \todo this should probably throw instead?
    return {};
  }
  timestamp_->mutate_sequence_counter(MessageId);
  *TimeLengthPtr = EventCount;

  // reset counter and increment message counter
  EventCount = 0;
  MessageId++;

  return Buffer_;
}

size_t TDCTSerializer::produce() {
  if (EventCount != 0) {
    XTRACE(OUTPUT, DEB, "autoproduce %zu EventCount_ \n", EventCount);
    serialize();
    if (ProduceFunctor) {
      auto time = reinterpret_cast<uint64_t *>(TimePtr)[0];
      // pulse_time is currently ns since 1970, produce time should be ms.
      ProduceFunctor(Buffer_, time / 1000000);
    }
    return Buffer_.size_bytes();
  }
  return 0;
}

size_t TDCTSerializer::eventCount() const { return EventCount; }

uint64_t TDCTSerializer::currentMessageId() const { return MessageId; }

size_t TDCTSerializer::addEvent(uint64_t Time) {
  XTRACE(OUTPUT, DEB, "Add event: %u\n", Time);
  reinterpret_cast<uint64_t *>(TimePtr)[EventCount] = Time;
  EventCount++;

  if (EventCount >= MaxEvents) {
    return produce();
  }
  return 0;
}
