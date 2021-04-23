// Copyright (C) 2016-2020 European Spallation Source, see LICENSE file
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief Implementation of senv schema serialiser
///
/// See https://github.com/ess-dmsc/streaming-data-types
//===----------------------------------------------------------------------===//
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include <common/SENVSerializer.h>
#include <common/gccintel.h>

#include <common/Trace.h>
//#undef TRC_LEVEL
//#define TRC_LEVEL TRC_L_DEB

namespace fb = flatbuffers;

template <typename T> ValueUnion MapType();
template <> ValueUnion MapType<uint8_t>() { return ValueUnion::UInt8Array; }
template <> ValueUnion MapType<uint16_t>() { return ValueUnion::UInt16Array; }
template <> ValueUnion MapType<uint32_t>() { return ValueUnion::UInt32Array; }
template <> ValueUnion MapType<uint64_t>() { return ValueUnion::UInt64Array; }

template <typename ValueType, typename ValueBuilder>
fb::Offset<ValueType> CreateValue(fb::FlatBufferBuilder &Fbb,
                                  fb::uoffset_t Ofs) {
  ValueBuilder builder_(Fbb);
  builder_.add_value(Ofs);
  return builder_.Finish();
}

template <typename ValueType>
const void *GetValueDataPtr(const SampleEnvironmentData *SenvData_) {
  return static_cast<const ValueType *>(SenvData_->Values())->value()->Data();
}

static_assert(FLATBUFFERS_LITTLEENDIAN,
              "Flatbuffers only tested on little endian systems");

template <typename DataType, typename ValueType, typename ValueBuilder>
SENVSerializer<DataType, ValueType, ValueBuilder>::SENVSerializer(
    size_t MaxArrayLength, std::string SourceName, ProducerCallback Callback)
    : MaxEvents(MaxArrayLength), ProduceFunctor(Callback) {

  auto SourceNameOffset = Builder_.CreateString(SourceName);
  auto TimeOffset =
      Builder_.CreateUninitializedVector(MaxEvents, sizeof(uint64_t), &TimePtr);
  auto DataOffset =
      Builder_.CreateUninitializedVector(MaxEvents, sizeof(DataType), &DataPtr);
  // auto ValueType = TypeMap[typeid(DataType)];
  auto BaseOffset = CreateValue<ValueType, ValueBuilder>(Builder_, DataOffset);

  SampleEnvironmentDataBuilder MessageBuilder(Builder_);
  MessageBuilder.add_Name(SourceNameOffset);
  MessageBuilder.add_Timestamps(TimeOffset);
  MessageBuilder.add_Values(BaseOffset.Union());
  MessageBuilder.add_Values_type(MapType<DataType>());
  MessageBuilder.add_Channel(1);
  MessageBuilder.add_PacketTimestamp(1);
  MessageBuilder.add_TimeDelta(1.0);
  MessageBuilder.add_MessageCounter(1);
  MessageBuilder.add_TimestampLocation(Location::End);
  Builder_.Finish(MessageBuilder.Finish(), SampleEnvironmentDataIdentifier());

  Buffer_ = nonstd::span<const uint8_t>(Builder_.GetBufferPointer(),
                                        Builder_.GetSize());

  SampleEnvironmentData_ = const_cast<SampleEnvironmentData *>(
      GetSampleEnvironmentData(Builder_.GetBufferPointer()));
  TimeLengthPtr =
      reinterpret_cast<flatbuffers::uoffset_t *>(const_cast<std::uint8_t *>(
          SampleEnvironmentData_->Timestamps()->Data())) -
      1;
  DataLengthPtr = reinterpret_cast<flatbuffers::uoffset_t *>(const_cast<void *>(
                      GetValueDataPtr<ValueType>(SampleEnvironmentData_))) -
                  1;

  SampleEnvironmentData_->mutate_MessageCounter(0);
  SampleEnvironmentData_->mutate_Channel(0);
  SampleEnvironmentData_->mutate_TimeDelta(0.0);
}

template <typename DataType, typename ValueType, typename ValueBuilder>
void SENVSerializer<DataType, ValueType, ValueBuilder>::setProducerCallback(
    ProducerCallback Callback) {
  ProduceFunctor = Callback;
}

template <typename DataType, typename ValueType, typename ValueBuilder>
nonstd::span<const uint8_t>
SENVSerializer<DataType, ValueType, ValueBuilder>::serialize() {
  if (EventCount > MaxEvents) {
    // \todo this should probably throw instead?
    return {};
  }
  SampleEnvironmentData_->mutate_MessageCounter(MessageId);
  *TimeLengthPtr = EventCount;
  *DataLengthPtr = EventCount;

  // reset counter and increment message counter
  EventCount = 0;
  MessageId++;

  return Buffer_;
}

template <typename DataType, typename ValueType, typename ValueBuilder>
size_t SENVSerializer<DataType, ValueType, ValueBuilder>::produce() {
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

template <typename DataType, typename ValueType, typename ValueBuilder>
size_t SENVSerializer<DataType, ValueType, ValueBuilder>::eventCount() const {
  return EventCount;
}

template <typename DataType, typename ValueType, typename ValueBuilder>
uint64_t
SENVSerializer<DataType, ValueType, ValueBuilder>::currentMessageId() const {
  return MessageId;
}

template <typename DataType, typename ValueType, typename ValueBuilder>
size_t
SENVSerializer<DataType, ValueType, ValueBuilder>::addEvent(uint64_t Time,
                                                            DataType Value) {
  XTRACE(OUTPUT, DEB, "Add event: %d %u\n", Time, Value);
  reinterpret_cast<uint64_t *>(TimePtr)[EventCount] = Time;
  reinterpret_cast<DataType *>(DataPtr)[EventCount] = Value;
  EventCount++;

  if (EventCount >= MaxEvents) {
    return produce();
  }
  return 0;
}

// Instantiate the particular variants
// TODO need the newer version of streaming data type
// that supports other types
template class SENVSerializer<uint8_t, UInt8Array, UInt8ArrayBuilder>;
template class SENVSerializer<uint16_t, UInt16Array, UInt16ArrayBuilder>;
template class SENVSerializer<uint32_t, UInt32Array, UInt32ArrayBuilder>;
template class SENVSerializer<uint64_t, UInt64Array, UInt64ArrayBuilder>;
