/* Copyright (C) 2017-2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief Parser for Bnlpz
//===----------------------------------------------------------------------===//

#pragma once

#include <vector>

#include <ansto/common/ParserFactory.h>
#include <ansto/common/Readout.h>

namespace Ansto {
namespace Bnlpz {

class DataParser : public IParserMethod {
public:
  enum error { OK = 0, ESIZE, EHEADER, ETIMESTAMP };
  enum PacketType { RAW, EVENT, ENDOF };
  
  struct __attribute__((__packed__)) PacketHeader {
    int32_t PacketType;
    int32_t BoardID;
    uint64_t Timestamp;
    //uint32_t PacketNumber;
  };

  struct __attribute__((__packed__)) EventHeader {
    uint32_t packetCount;
    uint16_t adcPeak1;
    uint16_t adcPeak2;
    uint32_t timingSystem;
    uint32_t userReg;
    uint32_t boardID;
    uint32_t dataSelect;
    uint32_t rateSelect;
    uint32_t rateOut;
  };

  struct __attribute__((__packed__)) EventHeaderExt {
    uint32_t EventCount;
    struct EventHeader header;
  };

  struct __attribute__((__packed__)) NeutronData {
    uint8_t x;
    uint8_t y;
    uint8_t v;
    uint8_t w;
    uint64_t ts64;
  };

  DataParser(const std::string &confg);

  virtual int parse(const char * /*void **/ buffer, unsigned int size,
                    uint64_t &tsmin, uint64_t &tsmax) override;
  virtual size_t pixelID(uint16_t xpos, uint16_t ypos) override;
  virtual unsigned int byteErrors() override { return Stats.ErrorBytes; };
  virtual unsigned int seqErrors() override { return Stats.SeqErrors; };
  virtual const std::vector<Readout> &getReadouts() override {
    return Readouts;
  };

  static std::unique_ptr<IParserMethod>
  CreateMethod(const std::string &config) {
    return std::make_unique<DataParser>(config);
  }

  struct Stats {
    uint64_t ErrorBytes{0};
    uint64_t SeqErrors{0};
  } Stats;

  Readout prototype;
  std::vector<Readout> Readouts; // reserve space to match buffer

  std::vector<uint32_t> LastPacketNumber;
  uint64_t Timestamp{0};
  uint64_t LastTimestamp{0};
  uint64_t BaseTime{0};
  std::vector<uint16_t> EventMap;

  const PacketHeader *PHeader{nullptr};
  const EventHeaderExt *EHeader{nullptr};
  const NeutronData *Data{nullptr};

  uint64_t MaxMessageGap{1'000'000'000};
  uint32_t XPixels{0};
  uint32_t YPixels{0};
  uint32_t BasePixel{0};
};
} // namespace Bnlpz
} // namespace Ansto