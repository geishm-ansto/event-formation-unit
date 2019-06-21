/** Copyright (C) 2016, 2017 European Spallation Source ERIC */

#include <common/analysis/ReducedEvent.h>
#include <fmt/format.h>

uint32_t ReducedHit::center_rounded() const {
  return static_cast<uint32_t>(std::round(center));
}

std::string ReducedHit::debug() const {
  return fmt::format("{}(lu={},uu={}) t={} *{}",
      center, uncert_lower, uncert_upper, average_time, hits_used);
}

std::string ReducedEvent::debug() const {
  return fmt::format("m={} x=[{}], y=[{}], z=[{}], t={} {}",
      module, x.debug(), y.debug(), z.debug(),
      time, (good ? "good" : "bad"));
}
