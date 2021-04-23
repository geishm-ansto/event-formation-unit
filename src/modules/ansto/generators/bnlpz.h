/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
#pragma once
#include <ansto/bnlpz/BnlpzParser.h>
#include <ansto/generators/igenbase.h>
#include <common/Timer.h>
#include <nlohmann/json.hpp>
#include <random>
#include <vector>

using std::string;

namespace Ansto {
namespace Generator {
using namespace Bnlpz;

struct BnlpzParam {
  BnlpzParam() = default;
  BnlpzParam(const nlohmann::json &simulate) {
    BaseTime = simulate["BaseTime"].get<uint64_t>();
    FramePeriod = simulate["FramePeriod"].get<uint64_t>();
    EventsPerFrame = simulate["EventsPerFrame"].get<uint32_t>();
    MsgId = 0;
    if (FramePeriod == 0)
      FramePeriod = 40000000LU;
    MaxPacketSize = simulate["MaxPacketSize"].get<uint16_t>();
  }

  // all times in nsec
  uint64_t BaseTime{0};
  uint64_t FramePeriod{0};
  uint32_t EventsPerFrame{0};
  uint16_t MsgId{0};
  uint16_t MaxPacketSize{1472};
};

class BnlpzSource : public ISource {
public:
  BnlpzSource(const string &file, const string &config);

  virtual int Next(char *buffer, size_t max_size) override;

  struct BnlpzParam Param;
  DataParser::PacketHeader PHeader;
  DataParser::EventHeaderExt EHeader;
  DataParser::NeutronData NeutronEvent;

  uint64_t LastFrame{0};
  std::default_random_engine Generator;
  std::uniform_int_distribution<uint64_t> Distribution;

  Timer RunTimer;
  uint64_t TimerReference{0};
  std::vector<uint32_t> SequenceCount;
};

} // namespace Generator
} // namespace Ansto