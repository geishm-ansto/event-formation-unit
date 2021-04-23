/* Copyright (C) 2017-2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief Parser for MCPD-8, based on the mesytec system and data format
/// description
//===----------------------------------------------------------------------===//

#pragma once

#include <vector>

#include <ansto/common/ParserFactory.h>
#include <ansto/common/Readout.h>

namespace Ansto {
namespace Mcpd8 {

// Type masks
constexpr uint16_t TypeMask = 0x8000;
constexpr uint16_t MdllType = 0x0002;

class DataParser : public IParserMethod {
public:
  enum error { OK = 0, ESIZE, EHEADER, ETIMESTAMP };

  // Common header shared between cmd, data and MDL messages
  // CMD =  {Header|CheckSum|Data[]}
  // Data = {Header|Parameters|EventElement[]} where the event could
  //            be either a LinearEvent, MDLLEvent or TriggerEvent

  // Notes on header:
  // bufferNumber is incremental 16 bit counter to detect lost data

  struct __attribute__((__packed__)) Header {
    uint16_t bufferLength;
    uint16_t bufferType;
    uint16_t headerLength;
    uint16_t bufferNumber;
    uint16_t runCmdID;
    uint8_t status;
    uint8_t mcpdID;
    uint16_t timeStamp[3];

    inline uint64_t nanosecs() {
      uint64_t nsec{0};
      memcpy(&nsec, timeStamp, 6);
      return nsec * 100;
    }
    inline void settime(uint64_t nsec) {
      uint64_t ticks = nsec / 100;
      memcpy(timeStamp, &ticks, 6);
    }
  };

  struct __attribute__((__packed__)) Parameters {
    uint16_t param[4][3];
  };

  struct __attribute__((__packed__)) EventElement {
    uint16_t word[3];
  };

  struct __attribute__((__packed__)) TriggerEvent {
    uint64_t timeStamp : 19;
    uint64_t data : 21;
    uint64_t dataID : 4;
    uint64_t triggerID : 3;
    uint64_t id : 1;

    inline uint32_t nanosecs() { return timeStamp * 100; }
  };

  struct __attribute__((__packed__)) TriggerData {
    uint32_t data : 24;
    uint32_t source : 8;
  };

  struct __attribute__((__packed__)) LinearEvent {
    uint64_t timeStamp : 19;
    uint64_t position : 10;
    uint64_t amplitude : 10;
    uint64_t slotID : 3;
    uint64_t pad : 2;
    uint64_t modID : 3;
    uint64_t id : 1;

    // according to the mesytec manual slotID is 5 bits but
    // 1..3 are used and valid - ignored the bits 4 and 5
    inline uint32_t nanosecs() { return timeStamp * 100; }
  };

  struct __attribute__((__packed__)) MDLLEvent {
    uint64_t timeStamp : 19;
    uint64_t yPosition : 10;
    uint64_t xPosition : 10;
    uint64_t amplitude : 8;
    uint64_t id : 1;

    // according to the mesytec manual x precedes y but tests
    // suggests that it is a misprint
    inline uint32_t nanosecs() { return timeStamp * 100; }
  };

  enum CommandID { Reset, StartDAQ, StopDAQ, ContinueDAQ };

  DataParser(const std::string &confg);

  virtual int parse(const char *buffer, unsigned int size, uint64_t &tsmin,
                    uint64_t &tsmax) override;
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

  struct Header *Header{nullptr};
  struct Parameters *Params{nullptr};
  struct EventElement *Data{nullptr};

  std::vector<uint8_t> PreviousSeqNum;
  uint64_t LastTimestamp{0};
  uint64_t BaseTime{0};
  std::vector<uint16_t> EventMap;

  uint32_t XPixels{0};
  uint32_t YPixels{0};
  uint32_t BasePixel{0};
};
} // namespace Mcpd8
} // namespace Ansto
