/** Copyright (C) 2017-2018 European Spallation Source */
//===----------------------------------------------------------------------===//
///
/// \file
/// Class to parse detector readout for ANSTO instruments using MCPD-8 UDP
/// Data format is described in ...
///
//===----------------------------------------------------------------------===//

#include "DataParser.h"
#include <ansto/common/Config.h>
#include <chrono>
#include <common/Log.h>
#include <common/Trace.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>

//#undef TRC_LEVEL
//#define TRC_LEVEL TRC_L_DEB

namespace Ansto {
namespace Mcpd8 {

//
constexpr size_t NumEvents = 256;
constexpr uint64_t MaxTimestamp = -1;

using namespace std::chrono;

union TriggerKey {
  struct __attribute__((__packed__)) Triggers {
    uint8_t dataID : 4;
    uint8_t triggerID : 4;
  } Source;
  uint8_t Value;
};

DataParser::DataParser(const std::string &jsonfile)
    : PreviousSeqNum(256, 0), EventMap(NumEvents, EventType::OtherEvent) {
  // TODO open the configuration file
  if (!jsonfile.empty()) {
    Ansto::Config config(jsonfile);
    auto root = config.getRoot("Detector");
    BasePixel = root["BasePixel"];
    XPixels = root["XPixels"];
    YPixels = root["YPixels"];
    auto evmap = root["TriggerSource"];
    for (const auto &x : evmap.items()) {
      try {
        TriggerKey Key;
        auto kvec = x.value().get<std::vector<int>>();
        Key.Source.triggerID = kvec[0];
        Key.Source.dataID = kvec[1];
        EventMap[Key.Value] = Ansto::StringToEventType.at(x.key());
      } catch (std::out_of_range &) {
        LOG(INIT, Sev::Error, "JSON MCPD8 - error: invalid event type: {}",
            x.key());
      }
    }
  }
  LastTimestamp = MaxTimestamp;
}

int DataParser::parse(const char *buffer, uint size, uint64_t &tsmin,
                      uint64_t &tsmax) {

  Readouts.clear();
  Header = nullptr;
  Data = nullptr;
  Stats.ErrorBytes = 0;
  Stats.SeqErrors = 0;

  auto headerLength = sizeof(struct Header);
  if (size < headerLength) {
    XTRACE(PROCESS, WAR, "Invalid data size: received %d, min. expected: %lu",
           size, headerLength);
    Stats.ErrorBytes += size;
    return -error::ESIZE;
  }

  Header = (struct Header *)buffer;

  // If it is a command check for {Reset, StartDAQ} to resynch the
  // timestamp. This achieved by setting LastTimestamp to a max value
  // which will force the BaseTime to be reset
  if (Header->bufferType & TypeMask) {
    if (Header->runCmdID == Reset || Header->runCmdID == StartDAQ) {
      LastTimestamp = MaxTimestamp;
      XTRACE(PROCESS, WAR, "Synchronize DAE with system clock: command=%d",
             Header->runCmdID);
    }
    return error::OK;
  }

  // Check the buffer length in the header matches the actual buffer
  // Note the minimum ethernet payload size is 42 bytes
  uint expectedSz = std::max(2 * Header->bufferLength, 42);
  if (expectedSz != size || Header->bufferLength < 21) {
    XTRACE(PROCESS, WAR,
           "Message size inconsistency: expected=%lu, actual=%lu, buffer "
           "length=%lu",
           expectedSz, size, 2 * Header->bufferLength);
    Stats.ErrorBytes += size;
    return -error::ESIZE;
  }

  // Only process neutron events and a limited number trigger events
  // from the EventType list

  auto paramLength = sizeof(struct Parameters);
  Params = (struct Parameters *)(buffer + headerLength);
  Data = (struct EventElement *)(buffer + headerLength + paramLength);

  // Check for out of sequence or lost message but continue note that
  // the counter cycles over 8 bits not 16 bits as suggested in documentation
  auto bufferNumber = (uint8_t)(Header->bufferNumber);
  if (bufferNumber != (uint8_t)(PreviousSeqNum[Header->mcpdID] + 1)) {
    XTRACE(PROCESS, WAR,
           "Sequence number inconsistency: current=%lu, previous=%lu",
           Header->bufferNumber, PreviousSeqNum[Header->mcpdID]);
    Stats.SeqErrors++;
  }
  PreviousSeqNum[Header->mcpdID] = bufferNumber;

  // check the data space is a multiple of 3 words (6 bytes)
  // log the error but continue processing the message
  auto numBytes = 2 * Header->bufferLength - headerLength - paramLength;
  if (numBytes % 6 != 0) {
    XTRACE(PROCESS, WAR, "Data buffer is not a multiple of 3 words");
    Stats.ErrorBytes += size;
  }
  size_t numEvents = numBytes / 6;

  // get the parameters?

  // if the last timestamp is less than 1 sec from the last time stamp assume
  // that it was reset/changed and that the base time should be reset
  auto timeStamp = Header->nanosecs();
  if (timeStamp + 1000000000UL < LastTimestamp) {
    nanoseconds nsecs =
        duration_cast<nanoseconds>(system_clock::now().time_since_epoch());
    BaseTime = static_cast<uint64_t>(nsecs.count()) - timeStamp;
    LOG(INIT, Sev::Info, "New BaseTime:  {} ms, TS: {} ms",
        BaseTime / 1000000UL, timeStamp / 1000000UL);
  }
  LastTimestamp = timeStamp;
  uint64_t ReadoutBaseTime = BaseTime + timeStamp;
  prototype.source = Header->mcpdID;
  Readouts.resize(numEvents, prototype);

  // when the dae hardware is configured for a new test there may be
  // inconsistent values that lead to a future base time that has caused
  // problems with the kafka retention processing - trap base time more than 10
  // sec into the future, log the error and continue
  nanoseconds nsecs =
      duration_cast<nanoseconds>(system_clock::now().time_since_epoch());
  if (ReadoutBaseTime > static_cast<uint64_t>(nsecs.count()) + 10000000000UL) {
    LOG(INIT, Sev::Warning, "Warning Future BaseTime:  {} ms, TS: {} ms",
        BaseTime / 1000000UL, timeStamp / 1000000UL);
    Stats.SeqErrors += 1;
    LastTimestamp = MaxTimestamp;
    return -error::ETIMESTAMP;
  }

  // lambda functions that map the trigger, tube and mdll neutron events
  // generate the x posn value from the parameters
  auto mapTube = [&](LinearEvent *ev, Readout &r) {
    r.event_type = Ansto::Neutron;
    r.timestamp = ev->nanosecs() + ReadoutBaseTime;
    tsmin = std::min(r.timestamp, tsmin);
    tsmax = std::max(r.timestamp, tsmax);
    r.x_posn = Header->mcpdID << 6 | ev->modID << 3 | ev->slotID;
    r.y_posn = ev->position;
    r.weight = ev->amplitude;
    r.data = 0;
  };

  auto mapMDLL = [&](MDLLEvent *ev, Readout &r) {
    r.event_type = Ansto::Neutron;
    r.timestamp = ev->nanosecs() + ReadoutBaseTime;
    tsmin = std::min(r.timestamp, tsmin);
    tsmax = std::max(r.timestamp, tsmax);
    r.x_posn = ev->xPosition;
    r.y_posn = ev->yPosition;
    r.weight = ev->amplitude;
    r.data = 0;
  };

  struct TriggerData *trData;
  TriggerKey evKey;
  auto mapTrig = [&](TriggerEvent *ev, Readout &r) {
    evKey.Source.triggerID = ev->triggerID;
    evKey.Source.dataID = ev->dataID;
    r.event_type = EventMap[evKey.Value];
    r.timestamp = ev->nanosecs() + ReadoutBaseTime;
    tsmin = std::min(r.timestamp, tsmin);
    tsmax = std::max(r.timestamp, tsmax);
    r.x_posn = 0;
    r.y_posn = 0;
    trData = (struct TriggerData *)&r.data;
    trData->data = ev->data;
    trData->source = ev->dataID;
  };

  // finally map all the events to the Readout format
  if (Header->bufferType == MdllType) {
    struct MDLLEvent *event;
    for (size_t i = 0; i < numEvents; ++i) {
      event = (struct MDLLEvent *)&Data[i];
      if (!event->id)
        mapMDLL(event, Readouts[i]);
      else
        mapTrig((struct TriggerEvent *)event, Readouts[i]);
    }
  } else {
    struct LinearEvent *event;
    for (size_t i = 0; i < numEvents; ++i) {
      event = (struct LinearEvent *)&Data[i];
      if (!event->id)
        mapTube(event, Readouts[i]);
      else
        mapTrig((struct TriggerEvent *)event, Readouts[i]);
    }
  }

  return numEvents;
}

size_t DataParser::pixelID(uint16_t xpos, uint16_t ypos) {
  return xpos * YPixels + ypos + BasePixel;
}

// Finally register the parser
RegisteredParser<DataParser> registered("mcpd8");

} // namespace Mcpd8
} // namespace Ansto
