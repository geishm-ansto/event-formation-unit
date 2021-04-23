/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */

#include "mcpd8.h"
#include <ansto/common/Config.h>
#include <stdlib.h>
#include <time.h>

using std::string;

namespace Ansto {
namespace Generator {

using namespace Ansto::Mcpd8;

Mcpd8Source::Mcpd8Source(const string &filename, const string &cfgfile)
    : NeutronEvent() {
  if (!filename.empty()) {
    printf("** Reading a file is not support for mcpd8, running simulator.\n");
  }

  // TODO: add primary trigger events based on the frame period to the udp data

  Ansto::Config config(cfgfile);
  Param = Mcpd8Param(config.getRoot("Simulation"));

  // repeatable random seed
  srand(42);

  // The base and last frame time is reset at the start of each test for mcpd8

  if (Param.BaseTime == 0) {
    Param.BaseTime = 1000000000LU * (uint64_t)time(NULL);
  }
  RunTimer.now();
  TimerReference = Param.BaseTime;
  LastFrame = Param.BaseTime;
  Header.settime(Param.BaseTime);

  // fill remaining header parameters noting that header length is in words
  Header.bufferType = 0;
  Header.headerLength =
      (sizeof(struct DP::Header) + sizeof(struct DP::Parameters)) / 2;
  Header.bufferNumber = 0;
  Header.runCmdID = 0;
  Header.status = 0x3;
  Header.mcpdID = 1;

  //double FramePeriodMsec = Param.FramePeriod / 1.0e6;
  // Distribution = std::normal_distribution<double>(0.5 * FramePeriodMsec,
  //                                                 0.1 * FramePeriodMsec);
  Distribution = std::normal_distribution<double>(0.5 * 10,
                                                  0.1 * 10);

  NeutronEvent.id = 0;
  NeutronEvent.modID = 0;
  NeutronEvent.slotID = 0;
  NeutronEvent.pad = 0;
  TriggerEvent.id = 1;
  TriggerEvent.triggerID = Param.FrameSource[0];
  TriggerEvent.data = 0;
  TriggerEvent.dataID = Param.FrameSource[1];
}

int Mcpd8Source::Next(char *buffer, size_t max_size) {

  // use the data parser structure to populate buffer
  // as it is covered by unit testing

  // LastFrame, TimeNow, BaseTime
  // NomNumEvents = (TimeNow - BaseTime) * EventsPerPeriod / FramePeriod
  // while TimeNow > LastFrame + FramePeriod and events < MaxEvents:
  //     LastFrame += FramePeriod
  //     addevent(LastFrame - BaseTime)
  //     events++
  // NeutronEvents = min(NomNumEvents, MaxEvents - events)
  // for i < NeutronEvents:
  //   addevent(..)
  // BaseTime = TimeNow

  // add the maximum ethernet frame size constraint
  uint32_t MaxLength = std::min(max_size, (size_t)Param.MaxPacketSize);
  size_t MaxEvents = (MaxLength - 2 * Header.headerLength) / 6;

  // Create the trigger and neutron events
  uint64_t TimeNow = TimerReference + 1000 * RunTimer.timeus();
  size_t eventSz = sizeof(struct DP::LinearEvent);
  char *p = buffer + 2 * Header.headerLength;
  size_t NewEvents{0};

  // If there is a new frame add the frame event and continue otherwise breakout
  while (!Param.FixedPattern && TimeNow > LastFrame + Param.FramePeriod &&
         NewEvents < MaxEvents) {
    LastFrame += Param.FramePeriod;
    TriggerEvent.timeStamp = (LastFrame - Param.BaseTime) / 100;
    NewEvents++;
    memcpy(p, &TriggerEvent, eventSz);
    p += eventSz;
  }

  // insert neutron events
  for (size_t i = 0; i < Param.EventsPerFrame && NewEvents < MaxEvents; i++) {
    if (Param.FixedPattern) {
      NeutronEvent.modID = (NeutronEvent.modID + 1) % 8;
      NeutronEvent.slotID = (NeutronEvent.slotID + 1) % 8;
      NeutronEvent.position = (NeutronEvent.position + 1) % 1024;
      NeutronEvent.amplitude = (NeutronEvent.amplitude + 1) % 512;
      NeutronEvent.timeStamp++;
      NewEvents++;
      memcpy(p, &NeutronEvent, eventSz);
      p += eventSz;
    } else {
      auto Value = static_cast<uint64_t>(Distribution(Generator) * 1000000);
      auto PulseTime = LastFrame + Value;
      if (PulseTime >= Param.BaseTime && PulseTime < TimeNow) {
        NeutronEvent.modID = rand() % 8;
        NeutronEvent.slotID = rand() % 8;
        NeutronEvent.position = rand() % 1024;
        NeutronEvent.amplitude = rand() % 512 + 256;
        NeutronEvent.timeStamp = (PulseTime - Param.BaseTime) / 100;
        NewEvents++;
        memcpy(p, &NeutronEvent, eventSz);
        p += eventSz;
      }
    }
  }

  // Change the BaseTime to the current time, update the header noting that
  // buffer length should not include the first word according to the
  // documentation but the implementation appears to be the complete message
  // length (makes more sense)
  Header.settime(Param.BaseTime);
  Param.BaseTime = TimeNow;
  auto mcpID = (Param.FixedPattern ? (Header.mcpdID + 1) % 3 : rand() % 3);
  Header.mcpdID = mcpID;
  Header.bufferLength = Header.headerLength + 3 * NewEvents;
  Header.bufferNumber = ++Param.SequenceCount[mcpID];
  memset(buffer, 0, 2 * Header.headerLength);
  size_t hdrSz = sizeof(struct DP::Header);
  memcpy(buffer, &Header, hdrSz);
  return (p - buffer);
}

} // namespace Generator
} // namespace Ansto