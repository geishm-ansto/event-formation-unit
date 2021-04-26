// Copyright (C) 2016-2020 European Spallation Source, see LICENSE file
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief flatbuffer serialization for the 64bit variant of senv schema
///
/// See https://github.com/ess-dmsc/streaming-data-types
//===----------------------------------------------------------------------===//

#pragma once

#include "Producer.h"
#include "flatbuffers/flatbuffers.h"
#include "senv_data_generated.h"

template <typename DataType, typename ValueType, typename ValueBuilder>
class SENVSerializer {
public:
  /// \brief creates senv flat buffer serializer
  /// \param max_array_length maximum number of events
  /// \param source_name value for source_name field

  SENVSerializer(size_t MaxArrayLength, std::string SourceName,
                 ProducerCallback Callback = {});

  /// \brief sets producer callback
  /// \param cb function to be called to send buffer to Kafka
  void setProducerCallback(ProducerCallback Callback);

  /// \brief adds event, if maximum count is exceeded, sends data using the
  /// producer callback
  /// \param time time of event in relation to pulse time
  /// \param pixl id of pixel as defined by logical geometry mapping
  /// \returns bytes transmitted, if any
  size_t addEvent(uint64_t Time, DataType Value);

  /// \brief returns event count
  size_t eventCount() const { return EventCount; }

  /// \returns current message counter
  uint64_t getMessageId() const {
    return MessageId;
  }

  /// \returns current timestamp location
  Location getTimestampLocation() const {
    return SampleEnvironmentData_->TimestampLocation();
  }

  /// \brief sets the current timestamp location
  /// \param Value location value
  void setTimestampLocation(Location Value) {
    SampleEnvironmentData_->mutate_TimestampLocation(Value);
  }

  /// \returns returns the current channel
  int32_t getChannel() const { return SampleEnvironmentData_->Channel(); }

  /// \brief sets the current channel Id
  /// \param ChannelId new channel id
  void setChannel(int32_t ChannelId) {
    SampleEnvironmentData_->mutate_Channel(ChannelId);
  }

  /// \returns returns the current time delta
  double getTimeDelta() const { return SampleEnvironmentData_->TimeDelta(); }

  /// \brief sets the current time delta
  /// \param Value new time delta value
  void setTimeDelta(double Value) {
    SampleEnvironmentData_->mutate_TimeDelta(Value);
  }

  /// \returns returns the current packet timestamp
  uint64_t getPacketTimestamp() const {
    return SampleEnvironmentData_->PacketTimestamp();
  }

  /// \brief sets the current packet timestamp
  /// \param Value new packet timestamp value
  void setPacketTimestamp(uint64_t Value) {
    SampleEnvironmentData_->mutate_PacketTimestamp(Value);
  }

  /// \brief serializes and sends to producer
  /// \returns bytes transmitted
  size_t produce();

  // \todo make private?
  /// \brief serializes buffer
  /// \returns reference to internally stored buffer
  nonstd::span<const uint8_t> serialize();

private:
  // \todo should this not be predefined in terms of jumbo frame?
  size_t MaxEvents{0};
  size_t EventCount{0};

  // \todo maybe should be mutated directly in buffer? Start at 0?
  uint64_t MessageId{1};

  // All of this is the flatbuffer
  flatbuffers::FlatBufferBuilder Builder_;

  ProducerCallback ProduceFunctor;

  uint8_t *TimePtr{nullptr};
  uint8_t *DataPtr{nullptr};

  SampleEnvironmentData *SampleEnvironmentData_;

  nonstd::span<const uint8_t> Buffer_;
  flatbuffers::uoffset_t *TimeLengthPtr;
  flatbuffers::uoffset_t *DataLengthPtr;
};

using SENVSerializerU8 = SENVSerializer<uint8_t, UInt8Array, UInt8ArrayBuilder>;
using SENVSerializerU16 =
    SENVSerializer<uint16_t, UInt16Array, UInt16ArrayBuilder>;
using SENVSerializerU32 =
    SENVSerializer<uint32_t, UInt32Array, UInt32ArrayBuilder>;
using SENVSerializerU64 =
    SENVSerializer<uint64_t, UInt64Array, UInt64ArrayBuilder>;