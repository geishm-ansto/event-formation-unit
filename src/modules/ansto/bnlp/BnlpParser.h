/* Copyright (C) 2017-2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief Parser for Bnlp
//===----------------------------------------------------------------------===//

#pragma once

#include <arpa/inet.h>
#include <asm/byteorder.h>
#include <vector>

#include <ansto/common/ParserFactory.h>
#include <ansto/common/Readout.h>

namespace Ansto {
namespace Bnlp {

// ***
// DEVELOPMENT MOVED TO USING ZMQ WITH THE IMPLEMENTATION IN BNLPZ
// ***
// The initial cut of the code has been tested at the level of the
// unit test cases. This code may be removed in the future if the ZMQ
// solution can meet the performance throughput.

// use inline function access to the data to avoid copies and endianess
// manipulation
struct Header {
  uint8_t s[16];
  inline uint32_t PacketCnt() { return ntohl(*(uint32_t *)s); }
  inline uint16_t AdcPeak2() const { return ntohs(*(uint16_t *)(s + 4)); }
  inline uint16_t AdcPeak1() const { return ntohs(*(uint16_t *)(s + 6)); }
  inline uint32_t UserReg() const {
    return s[8] << 12 | s[9] << 4 | s[10] >> 4;
  }
  inline uint8_t BoardID() const { return s[10] & 0xF; }
  inline uint8_t TimingSelect() const { return s[11] >> 4; }
  inline uint8_t DataSelect() const { return s[11] & 0xF; }
  inline uint8_t RateSelect() const { return s[12] >> 7; }
  inline uint32_t RateOut() const {
    return (s[12] & 0x7F) << 24 | s[13] << 16 | s[14] << 8 | s[15];
  }
};

// The sample data is not consistent with the document description.
// The structure is taken from the HMS code as it is known to
// be correct (s[4] and s[5] are switched compare to document).
struct SampleData {
  uint8_t s[6];
  inline uint8_t CoarseTS() const { return s[4]; }
  inline uint32_t FineTS() const { return s[5] << 10 | s[2] << 2 | s[3] >> 6; }
  inline uint8_t AdcValue() const { return s[3] & 0x3F; }
  inline uint8_t Timing() const { return (s[0] & 0xC0) >> 4 | s[1] >> 6; }
  inline uint8_t ChipNum() const { return s[0] & 0x3F; }
  inline uint8_t ChannelNum() const { return s[1] & 0x3F; }
};

struct TimestampEvent {
  uint8_t s[6];
  inline uint32_t Timestamp() const {
    return s[4] << 24 | s[5] << 16 | s[2] << 8 | s[3];
  }
  inline uint8_t ChipNum() const { return s[0]; }
  inline uint8_t QBD() const { return s[1]; }
};

struct __attribute__((__packed__)) Coordinate {
  int x;
  int y;
};

// Exposed these methods to unit testing
Coordinate DecodeToPixel(int board, int chip, int channel);
void MapToPad(uint chn, uint chp, int &x, int &y);

class DataParser : public IParserMethod {
public:
  enum error { OK = 0, ESIZE, EHEADER, ETIMESTAMP };
  enum DataSelect { SAMPLE_EVENT = 0, TEST_COUNTER = 1, ADC_EVENT = 2 };

  // The implementation is based on the BNL guide
  //    "One Meter Neutron PAD Detector User Guide_B2.pdf"

  // There are 3 types of UDP packets that are sent from the bnlpad.
  // Each packet has a common header followed by a list of relevant data.
  //  . Event Data = {Header | SampleData[]]}
  //  . Test Count = {Header | CounterData[]]}
  //  . HS ADC Data = {Header | AdcData[]]}

  // The incoming data is handled naturally on a big-endian machine but the
  // Header and each SampleEvent needs to be byte swapped to match the little
  // endian target

  union TimeCounter {
    uint64_t full64;
    struct __attribute__((__packed__)) {
      uint64_t fineTS : 18;
      uint64_t coarseTS : 8;
    } sample;
    struct __attribute__((__packed__)) {
      uint64_t lower32 : 32;
      uint64_t upper32 : 32;
    } extended;
  };

  union SampleEvent {
    struct SampleData sample;
    struct TimestampEvent timestamp;
  };

  struct __attribute__((__packed__)) CounterData {
    uint16_t counter : 16;
  };

  struct __attribute__((__packed__)) AdcData {
    uint16_t adc : 16;
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
  struct Header *Header{nullptr};

  // PreviousResidual and PartialResidual are used to handle
  // partial events at the end or start of packets
  std::vector<uint32_t> PreviousResidual;
  std::vector<SampleEvent> PartialResidual;
  std::vector<uint32_t> LastPacketNumber;
  std::vector<TimeCounter> Timestamps;
  uint32_t TimestampExtn{0};
  uint64_t LastTimestamp{0};
  uint64_t BaseTime{0};

  uint64_t MaxMessageGap{1000000000};
  uint32_t YPixels{0};
  uint32_t BasePixel{0};
};
} // namespace Bnlp
} // namespace Ansto
