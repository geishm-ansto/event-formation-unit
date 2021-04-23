/// Copyright (C) 2016-2018 European Spallation Source, see LICENSE file
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief flatbuffer serialization
///
//===----------------------------------------------------------------------===//

#pragma once

#include "Producer.h"
#include "flatbuffers/flatbuffers.h"

struct timestamp;

class TDCTSerializer {
public:
  /// \brief creates TDCT flat buffer serializer
  /// \param max_array_length maximum number of events
  /// \param name value for name field
  TDCTSerializer(size_t maxArrayLength, std::string name, ProducerCallback callback = {});

  /// \brief sets producer callback
  /// \param cb function to be called to send buffer to Kafka
  void setProducerCallback(ProducerCallback callback);

  /// \brief adds event, if maximum count is exceeded, sends data using the producer callback
  /// \param time time of event in relation to pulse time
  /// \returns bytes transmitted, if any
  size_t addEvent(uint64_t time);

  /// \brief returns event count
  size_t eventCount() const;

  /// \returns current message counter
  uint64_t currentMessageId() const;

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

  timestamp *timestamp_;

  nonstd::span<const uint8_t> Buffer_;
  flatbuffers::uoffset_t *TimeLengthPtr;
};