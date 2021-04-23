/** Copyright (C) 2017-2018 European Spallation Source */
//===----------------------------------------------------------------------===//
///
/// \file
/// Class to parse detector readout for ANSTO instruments using MCPD-8 UDP
/// Data format is described in ...
///
//===----------------------------------------------------------------------===//

#include "BnlpParser.h"
#include <algorithm>
#include <ansto/common/Config.h>
#include <chrono>
#include <common/Log.h>
#include <common/Trace.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>

//#undef TRC_LEVEL
//#define TRC_LEVEL TRC_L_DEB

namespace Ansto {
namespace Bnlp {

using namespace std::chrono;

// The following code and structures are from the neutron pad detector
// user guide. The code is based on Fig 3. The elements are needed to translate
// (chip, channel) to a (48,48) image. In the translation (0,0) corresponds to
// the top-right, pve Y down, pve x left. The image is looking from the front.
constexpr int PIXEL_MAP[] = {59, 51, 43, 35, 34, 42, 50, 58, 25, 33, 41, 49, 57,
                             24, 32, 40, 48, 56, 16, 8,  0,  1,  9,  17, 2,  10,
                             18, 26, 3,  11, 19, 27, 4,  12, 20, 28, 5,  13, 21,
                             29, 6,  14, 22, 7,  15, 23, 63, 55, 47, 39, 31, 62,
                             54, 46, 38, 30, 61, 53, 45, 37, 36, 44, 52, 60};
constexpr int CHIP_MAP_Y[] = {0, 8, 16, 24, 32, 40, 40, 32, 24, 16, 8, 0,
                              0, 8, 16, 24, 32, 40, 40, 32, 24, 16, 8, 0,
                              0, 8, 16, 24, 32, 40, 40, 32, 24, 16, 8, 0};
constexpr int CHIP_MAP_X[] = {0,  0,  0,  0,  0,  0,  8,  8,  8,  8,  8,  8,
                              16, 16, 16, 16, 16, 16, 24, 24, 24, 24, 24, 24,
                              32, 32, 32, 32, 32, 32, 40, 40, 40, 40, 40, 40};
constexpr int CHIP_MAP_fl[] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
                               0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
                               0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1};

void MapToPad(uint chp, uint chn, int &x, int &y) {
  x = (PIXEL_MAP[chn] / 8);
  y = PIXEL_MAP[chn] % 8;
  if (CHIP_MAP_fl[chp] == 1) {
    y = (7 - y);
    x = (7 - x);
  }
  y += CHIP_MAP_Y[chp];
  x += CHIP_MAP_X[chp];
}

// The chip and channel define the (x,y) coordinate based on the board however
// each board is displaced and rotated. The documentation defines the board
// location viewed from the back along with the rotation. This is transformed to
// a view from front.

// The board coordinate position (16,16) is from the front with (0,0) at the
// bottom-left as well.

constexpr int CBoardPixels{48};
constexpr int CPanelPixels{192};

std::map<int, int> boardIdMap = {
    {0, 0},  {2, 1}, {3, 2},  {1, 3},   {7, 4},   {4, 5},   {5, 6},   {6, 7},
    {10, 8}, {9, 9}, {8, 10}, {11, 11}, {13, 12}, {15, 13}, {14, 14}, {12, 15}};

constexpr Coordinate boardOfs[]{{3, 0}, {2, 0}, {3, 1}, {2, 1}, {1, 0}, {0, 0},
                                {1, 1}, {0, 1}, {3, 2}, {2, 2}, {3, 3}, {2, 3},
                                {1, 2}, {0, 2}, {1, 3}, {0, 3}};

auto rot0 = [&](int, int &, int &) {};
auto rot90 = [&](int n, int &x, int &y) {
  auto t = y;
  y = n - 1 - x;
  x = t;
};
auto rot180 = [&](int n, int &x, int &y) {
  x = n - 1 - x;
  y = n - 1 - y;
};
auto rot270 = [&](int n, int &x, int &y) {
  auto t = x;
  x = n - 1 - y;
  y = t;
};
auto flip = [&](int n, int &x, int &) {
  x = n - 1 - x;
  // y = n - 1 - y;
};
const std::function<void(int, int &, int &)> boardRotation[] = {
    rot270, rot90, rot270, rot90,  rot0,   rot0,  rot180, rot180,
    rot0,   rot0,  rot180, rot180, rot270, rot90, rot270, rot90};

Coordinate DecodeToPixel(int boardId, int chip, int channel) {

  // The pad map converts it to (48,48) from front so the process
  // requires switching from front to back , mapping to the board
  // and flipping the full (192,192) to front.
  int x, y;
  int board = boardIdMap[boardId];
  MapToPad(chip, channel, x, y);
  flip(CBoardPixels, x, y); // front to back
  boardRotation[board](CBoardPixels, x, y);
  x += boardOfs[board].x * CBoardPixels;
  y += boardOfs[board].y * CBoardPixels;
  // thought it needed to be rotated by 180 but 270 is consistent
  // with the existing system???
  rot270(CPanelPixels, x, y);
  return Coordinate{x, y};
}

constexpr int NumBoards = 16;
constexpr int NumChips = 36;
constexpr int NumChannels = 64;

Coordinate DecoderMap[NumBoards][NumChips][NumChannels];
void FillDecoderMap() {
  for (int board = 0; board < NumBoards; board++) {
    for (int chip = 0; chip < NumChips; chip++) {
      for (int channel = 0; channel < NumChannels; channel++) {
        DecoderMap[board][chip][channel] = DecodeToPixel(board, chip, channel);
      }
    }
  }
}

DataParser::DataParser(const std::string &jsonfile)
    : PreviousResidual(16, 0), PartialResidual(16), LastPacketNumber(16, 0),
      Timestamps(16, {0}) {
  FillDecoderMap();
  if (!jsonfile.empty()) {
    Ansto::Config config(jsonfile);
    auto root = config.getRoot("Detector");
    BasePixel = root["BasePixel"];
    YPixels = root["YPixels"];
    MaxMessageGap =
        (uint64_t)(root["MaxMessageGap"]) * 10'000UL; // msecs to 100nsec clock
  }
  LastTimestamp = std::numeric_limits<uint64_t>::max();
}

int DataParser::parse(const char *buffer, uint size, uint64_t &tsmin,
                      uint64_t &tsmax) {

  Readouts.clear();

  Stats.ErrorBytes = 0;
  Stats.SeqErrors = 0;

  auto headerSize = sizeof(struct Header);
  if (size < headerSize) {
    XTRACE(PROCESS, WAR, "Invalid data size: received %d, min. expected: %lu",
           size, headerSize);
    Stats.ErrorBytes += 1;
    return -error::ESIZE;
  }

  Header = (struct Header *)buffer;
  auto boardID = Header->BoardID();

  // Check for out of sequence or lost message but continue
  auto packetCnt = (uint32_t)(Header->PacketCnt());
  if (packetCnt != (uint32_t)(LastPacketNumber[boardID] + 1)) {
    XTRACE(PROCESS, WAR,
           "Sequence number inconsistency: board=%d, current=%lu, previous=%lu",
           boardID, Header->PacketCnt(), LastPacketNumber[boardID]);
    Stats.SeqErrors++;
  }
  LastPacketNumber[boardID] = packetCnt;

  // Only processing sample events (neutron or timestamp events), if there
  // are any.
  if (Header->DataSelect() != SAMPLE_EVENT || (size - headerSize) == 0) {
    return error::OK;
  }

  auto elementSize = sizeof(struct SampleData);

  // Apparently, data events can be split across two packets. If the packet
  // sequence numbers are consistent then the trailing and leading residuals
  // will be merged and included in the normal processing. If there is a packet
  // sequence error the offset to the data will need to be determined from the
  // data content.
  auto validOffset = [&](size_t offset) -> bool {
    // Byte 0 is the chip number (< 36|0x3E|0x3F)
    auto p = (SampleEvent *)(buffer + headerSize + offset);
    size_t numEvents = (size - headerSize - offset) / elementSize;
    for (size_t i = 0; i < numEvents; i++) {
      auto x = p[i].sample.ChipNum();
      if (x >= 36 && x < 0x3E)
        return false;
    }
    return true;
  };

  auto findValidOffset = [&](size_t maxOffset) -> size_t {
    for (size_t i = 0; i < maxOffset; i++) {
      if (validOffset(i))
        return i;
    }
    // If it can't find it, go with 0
    return 0;
  };

  // For sequential processing where there is a partial residual the remaining
  // offset will be confirmed. If the test fails, it will determine the offset,
  // drop the partial event and continue. At the end of this block a non-zero
  // PreviousResidual value indicates the presence of a merged partial event.
  size_t dataOffset = 0;
  if (Stats.SeqErrors == 0) {
    auto residual = PreviousResidual[boardID];
    if (residual > 0) {
      // Confirm that the predicted offset is correct
      dataOffset = elementSize - residual;
      if (validOffset(dataOffset)) {
        memcpy(PartialResidual[boardID].sample.s + residual,
               buffer + headerSize, dataOffset);
        PreviousResidual[boardID] = elementSize;
      } else {
        dataOffset = findValidOffset(elementSize);
        PreviousResidual[boardID] = 0;
        Stats.ErrorBytes += 1;
        XTRACE(PROCESS, WAR,
               "Trailing partial event offset is invalid, ignoring partial "
               "event and resynching");
      }
    }
  } else {
    dataOffset = findValidOffset(elementSize);
    PreviousResidual[boardID] = 0;
  }

  // Processing the different message types if they exist
  // - but only handle sample for now
  if (Header->DataSelect() != SAMPLE_EVENT) {
    return 0;
  }
  auto data = (SampleEvent *)(buffer + headerSize + dataOffset);
  auto &Timestamp = Timestamps[boardID];

  // The sample timestamp is made up of a fineTS (18 bits) plus coarseTS (8
  // bits) where each tick is 100nsec. The sample timestamp range is ~7sec. For
  // each coarseTS click, a rollover TS event is sent as a 32 bit value (~7min
  // range). We need to manage a longer range by incrementing the upper bits in
  // a 64bit timestamp. This timestamp needs to be managed for each board as
  // each board sends timestamp rollover events. We can assume that the
  // synchronization of the boards is significantly better than the sample
  // timestamp range so the number of rollover cycles should never differ by
  // more than 1. The soln is too maintain an independent max upper 32 bit
  // counter. On a rollover event, increment the upper time as necessary and
  // compare the upper value against the max count. If it is greater than the
  // max, reset the max, else if it is less than max set the current upper to
  // the max. The assumption is that the 32 bit rollover timestamp is consistent
  // between the boards (to synch error).

  auto handleRolloverEvent = [&](const TimestampEvent &ev) {
    Timestamp.extended.lower32 = ev.Timestamp();
    if (Timestamp.extended.lower32 == 0) {
      Timestamp.extended.upper32++;

      if (Timestamp.extended.upper32 > TimestampExtn) {
        TimestampExtn = Timestamp.extended.upper32;
        LOG(INIT, Sev::Info, "Rollover timestamp:  boardID {}, ts {}", boardID,
            Timestamp.full64);
      } else if (Timestamp.extended.upper32 < TimestampExtn) {
        Timestamp.extended.upper32 = TimestampExtn;
        LOG(INIT, Sev::Info, "** Rollover ts fix:  boardID {}, ts {}", boardID,
            Timestamp.full64);
      }
    }
  };

  // Handling the DistirbutionBoxEvent is not well understood, for now
  // just copy the QBD data 
  auto handleDBoxEvent = [&](const TimestampEvent &ev, Readout &r) {
    Timestamp.extended.lower32 = ev.Timestamp();
    r.event_type = Ansto::OtherEvent;
    r.timestamp = Timestamp.full64 * 100 + BaseTime;
    tsmin = std::min(r.timestamp, tsmin);
    tsmax = std::max(r.timestamp, tsmax);
    r.data = ev.QBD();
  };

  // The code includes the following elements:
  //  - Timestamp   timing counter in 100ns units
  //  - BaseTime    a fixed reference that converts the timestamp to epoch
  //                time in nsec
  //  - LastTimestamp last recorded time stamp event in nsec since epoch

  // Check the timestamp. As there is no absolute time with
  // the timer counts, the BaseTime, which is the difference between
  // the timing counts and the system time, needs to be updated when there
  // is a significant difference between the two to account for restarting
  // a data acquisition.
  // If the difference between the current and last timestamp is more tha N secs
  // then consider it to be a restart and reset the BaseTime and Timestamp.
  // Timestamp is in 100nsec units.

  auto checkTimestamp = [&](SampleEvent *ev) {

    // handle rollover events before continuing
    if (ev->sample.ChipNum() == 0x3F) {
      handleRolloverEvent(ev->timestamp);
    } else if (ev->sample.ChipNum() == 0x3E) {
      Timestamp.extended.lower32 = ev->timestamp.Timestamp();
    } else {
      Timestamp.sample.coarseTS = ev->sample.CoarseTS();
      Timestamp.sample.fineTS = ev->sample.FineTS();
    }

    if (Timestamp.full64 + MaxMessageGap < LastTimestamp ||
        LastTimestamp + MaxMessageGap < Timestamp.full64) {
      nanoseconds nsecs =
          duration_cast<nanoseconds>(system_clock::now().time_since_epoch());
      BaseTime = static_cast<uint64_t>(nsecs.count()) - Timestamp.full64 * 100;
      LOG(INIT, Sev::Info, "New BaseTime: {} ms, Brd: {}, TS: {}",
          BaseTime / 1000000UL, boardID, Timestamp.full64 / 10000UL);
    }
    auto timestamp = Timestamp.full64 * 100 + BaseTime;
    tsmin = std::min(timestamp, tsmin);
    tsmax = std::max(timestamp, tsmax);
    LastTimestamp = Timestamp.full64;
  };

  // fill the dample data information, noting that the event time
  // was updated prior to this call
  auto handleSampleData = [&](const SampleData &ev, Readout &r) {
    Timestamp.sample.fineTS = ev.FineTS();
    r.event_type = Ansto::Neutron;
    r.timestamp = Timestamp.full64 * 100 + BaseTime;
    tsmin = std::min(r.timestamp, tsmin);
    tsmax = std::max(r.timestamp, tsmax);
    const Coordinate &c = DecoderMap[boardID][ev.ChipNum()][ev.ChannelNum()];
    r.x_posn = c.x;
    r.y_posn = c.y;
    r.weight = ev.AdcValue();
    r.data = 0;
  };

  // Process each of the events and add to the readout but
  // first check and update the readout packet timestamp.
  // This will update the LastTimestamp which will be the
  // base time for the readout group.

  // If there is a reconstructed partial event process it first
  // and then the data in the packet
  auto dataSize = size - headerSize - dataOffset;
  size_t numEvents = dataSize / elementSize;
  size_t extraEvent = PreviousResidual[boardID] > 0 ? 1 : 0;
  if (extraEvent == 1)
    checkTimestamp(&PartialResidual[boardID]);
  else if (numEvents > 0)
    checkTimestamp(data);

  Readout prototype{0};
  prototype.event_type = Ansto::Blank;
  prototype.source = boardID;
  Readouts.resize(numEvents + extraEvent, prototype);

  // If the extra event is a sample, process it. Note that a rollover event
  // is processed in the checkTimestamp() function
  if (extraEvent > 0 && PartialResidual[boardID].sample.ChipNum() < NumChips)
    handleSampleData(PartialResidual[boardID].sample, Readouts[0]);
  for (size_t ix = 0; ix < numEvents; ix++) {
    auto &event = data[ix];
    auto chipNum = event.sample.ChipNum();
    if (chipNum < NumChips) {
      handleSampleData(event.sample, Readouts[ix + extraEvent]);
      continue;
    }
    // This rollover was handled by checkTimestamp() if there was
    // no extra event (partial recovery) and it was the first element
    if ((extraEvent > 0 || ix > 0) && chipNum == 0x3F) {
      handleRolloverEvent(event.timestamp);
      continue;
    }
    if (chipNum == 0x3E) {
      handleDBoxEvent(event.timestamp, Readouts[ix + extraEvent]);
      continue;
    }
  }

  // Check the the data buffer space is a multiple of the event size and copy
  // the partial event to be processed with the next package.
  PreviousResidual[boardID] = dataSize % elementSize;
  if (PreviousResidual[boardID] != 0) {
    memcpy(PartialResidual[boardID].sample.s, data[numEvents].sample.s,
           PreviousResidual[boardID]);
  }

  return numEvents + extraEvent;
}

size_t DataParser::pixelID(uint16_t xpos, uint16_t ypos) {
  return xpos * YPixels + ypos + BasePixel;
}

// Finally register the parser
RegisteredParser<DataParser> registered("bnlp");

} // namespace Bnlp
} // namespace Ansto
