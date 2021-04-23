/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */

#include "bnlpz.h"
#include <ansto/common/Config.h>
#include <chrono>
#include <stdlib.h>
#include <time.h>

using std::string;

namespace Ansto {
namespace Generator {

using namespace Ansto::Bnlpz;
using namespace std::chrono;

BnlpzSource::BnlpzSource(const string &filename, const string &cfgfile)
    : SequenceCount(16, 0) {
  if (!filename.empty()) {
    printf("** Reading a file is not support for bnlpz, running simulator.\n");
  }

  // TODO: add primary trigger events based on the frame period to the udp data

  Ansto::Config config(cfgfile);
  Param = BnlpzParam(config.getRoot("Simulation"));

  // repeatable random seed
  srand(42);

  // The base and last frame time is reset at the start of each test for bnlpz
  RunTimer.now();
  TimerReference = Param.BaseTime;
  LastFrame = Param.BaseTime;

  // fill remaining header parameters noting that header length is in words
  PHeader.PacketType = DataParser::EVENT;
  PHeader.BoardID = 1;

  EHeader.header.adcPeak1 = 0x1234;
  EHeader.header.adcPeak2 = 0x5678;
  EHeader.header.timingSystem = 0;
  EHeader.header.userReg = 1;
  EHeader.header.dataSelect = 0;
  EHeader.header.rateSelect = 0;
  EHeader.header.rateOut = 0x4321;

  uint64_t FramePeriodUnits = Param.FramePeriod / 100;
  Distribution = std::uniform_int_distribution<uint64_t>(0, FramePeriodUnits);
}

int BnlpzSource::Next(char *buffer, size_t max_size) {

  // use the data parser structure to populate buffer
  // as it is covered by unit testing

  // Setup the EventHeader
  //PHeader.PacketNumber++;

  // add the maximum ethernet frame size constraint
  size_t eventSz = sizeof(DataParser::NeutronData);
  size_t PHeaderSz = sizeof(DataParser::PacketHeader);
  size_t EHeaderSz = sizeof(DataParser::EventHeaderExt);
  size_t NeutronSz = sizeof(DataParser::NeutronData);
  uint32_t MaxLength = std::min(max_size, (size_t)Param.MaxPacketSize);
  size_t MaxEvents = (MaxLength - PHeaderSz - EHeaderSz) / NeutronSz;

  // Set the global timestamp - even though not used by parser
  microseconds usecs =
      duration_cast<microseconds>(system_clock::now().time_since_epoch());
  PHeader.Timestamp = usecs.count();

  uint64_t TimeNow = TimerReference + 1000 * RunTimer.timeus();

  char *p = buffer + PHeaderSz + EHeaderSz;
  uint32_t NewEvents{0};

  // update the last frame period
  auto periods = (TimeNow - LastFrame) / Param.FramePeriod;
  LastFrame += periods * Param.FramePeriod;

  // insert neutron events
  for (size_t i = 0; i < Param.EventsPerFrame && NewEvents < MaxEvents; i++) {
    auto Value = Distribution(Generator);
    auto PulseTime = LastFrame + Value * 100;
    if (PulseTime >= Param.BaseTime && PulseTime < TimeNow) {
      NeutronEvent.x = rand() % 192;
      NeutronEvent.y = rand() % 192;
      NeutronEvent.v = rand() % 256;
      NeutronEvent.w = rand() % 32 + 32;
      NeutronEvent.ts64 = PulseTime / 100;
      NewEvents++;
      memcpy(p, &NeutronEvent, eventSz);
      p += eventSz;
    }
  }

  // Change the BaseTime to the current time, update the header noting that
  // buffer length should not include the first word according to the
  // documentation but the implementation appears to be the complete message
  // length (makes more sense)
  Param.BaseTime = TimeNow;
  auto BoardID = rand() % 16;
  PHeader.BoardID = BoardID;
  EHeader.header.boardID = BoardID;
  EHeader.header.packetCount = ++SequenceCount[BoardID];
  EHeader.EventCount = NewEvents;
  memset(buffer, 0, PHeaderSz + EHeaderSz);
  memcpy(buffer, &PHeader, PHeaderSz);
  memcpy(buffer + PHeaderSz, &EHeader, EHeaderSz);
  return (p - buffer);
}

} // namespace Generator
} // namespace Ansto