/** Copyright (C) 2017-2018 European Spallation Source */
//===----------------------------------------------------------------------===//
///
/// \file
/// Class to parse detector readout for ANSTO instruments using MCPD-8 UDP
/// Data format is described in ...
///
//===----------------------------------------------------------------------===//

#include "BnlpzParser.h"
#include <ansto/common/Config.h>
#include <chrono>
#include <common/Log.h>
#include <common/Trace.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <endian.h>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>

//#undef TRC_LEVEL
//#define TRC_LEVEL TRC_L_DEB

namespace Ansto {
namespace Bnlpz {

using namespace std::chrono;

DataParser::DataParser(const std::string &jsonfile) : LastPacketNumber(16, 0) {
  Timestamp = 0;
  if (!jsonfile.empty()) {
    Ansto::Config config(jsonfile);
    auto root = config.getRoot("Detector");
    BasePixel = root["BasePixel"];
    YPixels = root["YPixels"];
    MaxMessageGap =
        (uint64_t)(root["MaxMessageGap"]) * 1'000'000UL; // msecs to nsec
  }
  LastTimestamp = std::numeric_limits<uint64_t>::max();
}

int DataParser::parse(const char *buffer, uint size, uint64_t &tsmin,
                      uint64_t &tsmax) {

  Readouts.clear();

  Stats.ErrorBytes = 0;
  Stats.SeqErrors = 0;

  auto headerSize = sizeof(struct PacketHeader) + sizeof(struct EventHeaderExt);
  if (size < headerSize) {
    XTRACE(PROCESS, WAR, "Invalid data size: received %d, min. expected: %lu",
           size, headerSize);
    Stats.ErrorBytes += size;
    return -error::ESIZE;
  }
  PHeader = (const PacketHeader *)(buffer);
  EHeader = (const EventHeaderExt *)(buffer + sizeof(struct PacketHeader));
  Data = (const NeutronData *)(buffer + headerSize);

  // Check for out of sequence or lost message but continue
  auto packetCnt = (uint32_t)(EHeader->header.packetCount);
  if (packetCnt != (uint32_t)(LastPacketNumber[EHeader->header.boardID] + 1)) {
    XTRACE(PROCESS, WAR,
           "Sequence number inconsistency: board=%d, current=%lu, previous=%lu",
           EHeader->header.boardID, EHeader->header.packetCount,
           LastPacketNumber[EHeader->header.boardID]);
    Stats.SeqErrors++;
  }
  LastPacketNumber[EHeader->header.boardID] = packetCnt;

  if (PHeader->PacketType != PacketType::EVENT) {
    XTRACE(PROCESS, WAR, "Data buffer is not an event type");
    Stats.ErrorBytes += size;
    return -error::EHEADER;
  }

  // Check the num of events agrees with the event size
  size_t numEvents = EHeader->EventCount;
  auto elementSize = sizeof(struct NeutronData);
  if (headerSize + numEvents * elementSize > size) {
    XTRACE(PROCESS, WAR, "Reported num events exceeds buffer size");
    auto dataSize = size - headerSize;
    Stats.ErrorBytes += dataSize % elementSize;
    numEvents = dataSize / elementSize;
  }

  if (numEvents == 0) {
    // TODO this is not checking the timestamp - so if there is
    // a period of no neutron events the baetime may be reset
    // the data structures and process flow needs to be review for 'bnlpz'
    Readouts.resize(0);
    return 0;
  }

  // Find the earliest time in the data
  uint8_t p[12];
  uint64_t minimumTS = std::numeric_limits<uint64_t>::max();
  for (size_t i = 0; i < numEvents; i++) {
    memcpy(p, &Data[i], 12);
    if (*p != 0) {
    }
    minimumTS = std::min(Data[i].ts64, minimumTS);
  }

  // If the difference between the current and last timestamp is more tha N secs
  // then consider it to be a restart and reset the BaseTime and Timestamp.
  // minimum timestamp is in 100nsec units.
  auto timeStamp = minimumTS * 100;
  if (timeStamp + MaxMessageGap < LastTimestamp ||
      LastTimestamp + MaxMessageGap < timeStamp) {
    nanoseconds nsecs =
        duration_cast<nanoseconds>(system_clock::now().time_since_epoch());
    BaseTime = static_cast<uint64_t>(nsecs.count()) - timeStamp;
    LOG(INIT, Sev::Info, "New BaseTime:  {} ms, TS: {} ms",
        BaseTime / 1000000UL, timeStamp / 1000000UL);
  }
  LastTimestamp = timeStamp;

  // Now fill the reader buffer
  prototype.event_type = Ansto::EventType::Blank;
  prototype.source = PHeader->BoardID;
  Readouts.resize(numEvents, prototype);

  for (size_t i = 0; i < numEvents; i++) {
    auto &neutron = Data[i];
    auto &readout = Readouts[i];
    // neutron events are filtered on the w term (expect less 64, 6 bits)
    if (neutron.w < 64) {
      readout.event_type = Ansto::EventType::Neutron;
      readout.x_posn = neutron.x;
      readout.y_posn = neutron.y;
      readout.timestamp = neutron.ts64 * 100 + BaseTime;
      tsmin = std::min(readout.timestamp, tsmin);
      tsmax = std::max(readout.timestamp, tsmax);
      readout.data = (neutron.v << 16 | neutron.w);
    }
  }

  return numEvents;
}

size_t DataParser::pixelID(uint16_t xpos, uint16_t ypos) {
  return xpos * YPixels + ypos + BasePixel;
}

// Finally register the parser
RegisteredParser<DataParser> registered("bnlpz");

} // namespace Bnlpz
} // namespace Ansto
