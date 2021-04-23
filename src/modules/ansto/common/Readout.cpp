/** Copyright (C) 2016, 2017 European Spallation Source ERIC */

#include <fmt/format.h>
#include "Readout.h"

namespace Ansto {

const std::map<std::string, EventType> StringToEventType{
  {"Blank", Blank},
  {"Neutron", Neutron},
  {"FrameStart", FrameStart},
  {"FrameAuxStart", FrameAuxStart},
  {"FrameDeassert", FrameDeassert},
  {"Veto", Veto},
  {"BeamMonitor", BeamMonitor},
  {"OtherEvent", OtherEvent},
  {"Invalid", Invalid}
}; 

std::string Readout::debug() const {
  return fmt::format("Event={} time={} id={} x={} y={} w={} data={}",
      event_type, timestamp, source, x_posn, y_posn, weight, data);
}

}