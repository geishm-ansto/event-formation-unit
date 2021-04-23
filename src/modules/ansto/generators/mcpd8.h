/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
#pragma once
#include <ansto/generators/igenbase.h>
#include <ansto/mcpd8/DataParser.h>
#include <common/Timer.h>
#include <nlohmann/json.hpp>
#include <random>
#include <vector>

using std::string;
using DP = Ansto::Mcpd8::DataParser;

namespace Ansto {
namespace Generator {

struct Mcpd8Param {
  Mcpd8Param() = default;
  Mcpd8Param(const nlohmann::json &simulate) {
    FixedPattern = simulate["FixedPattern"];
    BaseTime = simulate["BaseTime"];
    FramePeriod = simulate["FramePeriod"];
    EventsPerFrame = simulate["EventsPerFrame"];
    MsgId = 0;
    if (FramePeriod == 0)
      FramePeriod = 40000000LU;
    MaxPacketSize = simulate["MaxPacketSize"];
    FrameSource = simulate["FrameSource"].get<std::vector<uint8_t>>();
  }

  // all times in nsec
  bool FixedPattern{false};
  uint64_t BaseTime{0};
  uint64_t FramePeriod{0};
  uint32_t EventsPerFrame{0};
  uint16_t MsgId{0};
  uint16_t MaxPacketSize{1472};
  std::vector<uint8_t> FrameSource{7, 0};
  std::vector<uint8_t> SequenceCount{0, 0, 0, 0};
};

class Mcpd8Source : public ISource {
public:
  Mcpd8Source(const string &file, const string &config);

  virtual int Next(char *buffer, size_t max_size) override;

  struct Mcpd8Param Param;
  struct DP::Header Header;
  struct DP::LinearEvent NeutronEvent;
  struct DP::TriggerEvent TriggerEvent;

  uint64_t LastFrame{0};
  std::default_random_engine Generator;
  std::normal_distribution<double> Distribution;

  Timer RunTimer;
  uint64_t TimerReference{0};
};

} // namespace Generator
} // namespace Ansto