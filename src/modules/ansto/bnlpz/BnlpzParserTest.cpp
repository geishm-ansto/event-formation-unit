/** Copyright (C) 2017-2018 European Spallation Source */
//===----------------------------------------------------------------------===//
///
/// \file
///
//===----------------------------------------------------------------------===//

#include <ansto/bnlpz/BnlpzParser.h>
#include <test/TestBase.h>

using namespace Ansto;
using namespace Ansto::Bnlpz;

template <typename T>
size_t insertValue(std::vector<uint8_t> &buffer, size_t offset, T &value) {
  auto p = (T *)(buffer.data() + offset);
  *p = value;
  return sizeof(T);
}

void SetupPacketHeader(std::vector<uint8_t> &buffer, int32_t packetType,
                       int32_t boardID, uint64_t timestamp) {

  size_t offset = 0;
  offset += insertValue<int32_t>(buffer, offset, packetType);
  offset += insertValue<int32_t>(buffer, offset, boardID);
  offset += insertValue<uint64_t>(buffer, offset, timestamp);
}

void SetupExtHeader(std::vector<uint8_t> &buffer, uint32_t packetCount,
                    uint16_t adcPk1, uint16_t adcPk2, uint32_t userReg,
                    uint32_t board, uint32_t timing, uint32_t dataSel,
                    uint32_t rateSel, uint32_t rateOut) {

  size_t offset = sizeof(DataParser::PacketHeader);

  offset += insertValue<uint32_t>(buffer, offset,
                                  packetCount); // 1st is for extended Header
  offset += insertValue<uint32_t>(
      buffer, offset, packetCount); // following goes into EventHeader
  offset += insertValue<uint16_t>(buffer, offset, adcPk1);
  offset += insertValue<uint16_t>(buffer, offset, adcPk2);
  offset += insertValue<uint32_t>(buffer, offset, timing);
  offset += insertValue<uint32_t>(buffer, offset, userReg);
  offset += insertValue<uint32_t>(buffer, offset, board);
  offset += insertValue<uint32_t>(buffer, offset, dataSel);
  offset += insertValue<uint32_t>(buffer, offset, rateSel);
  offset += insertValue<uint32_t>(buffer, offset, rateOut);
}

void SetupNeutronData(std::vector<uint8_t> &buffer, size_t index, uint8_t x,
                      uint8_t y, uint8_t v, uint8_t w, uint64_t ts) {

  size_t offset = sizeof(DataParser::PacketHeader) +
                  sizeof(DataParser::EventHeaderExt) +
                  index * sizeof(DataParser::NeutronData);
  buffer[offset + 0] = x;
  buffer[offset + 1] = y;
  buffer[offset + 2] = v;
  buffer[offset + 3] = w;
  insertValue<uint64_t>(buffer, offset + 4, ts);
}

constexpr size_t HeaderSize{sizeof(DataParser::PacketHeader) +
                            sizeof(DataParser::EventHeaderExt)};
constexpr size_t NeutronSize{sizeof(DataParser::NeutronData)};

// Tests that need to be implemented
// Confirm the Header is correctly read, checking header size and each term.
// Confirm

std::vector<uint8_t> empty_msg(0, 0);
std::vector<uint8_t> all_zeroes(HeaderSize, 0);
std::vector<uint8_t> valid_hdr(HeaderSize, 0);
std::vector<uint8_t> one_sample(HeaderSize + NeutronSize, 0);
std::vector<uint8_t> two_sample(HeaderSize + 2 * NeutronSize, 0);

constexpr uint32_t CPacketCount{0x1020304};
constexpr uint16_t CAdcPk1{0x3423};
constexpr uint16_t CAdcPk2{0x5643};
constexpr uint32_t CUserReg{0x73412};
constexpr uint32_t CBoard{0xE};
constexpr uint32_t CTiming{0x1};
constexpr uint32_t CRateOut{0x6E78E};
constexpr uint32_t CDataSel{0x0};
constexpr uint32_t CCounterSel{0x1};
constexpr uint32_t CRateSel{0x1};
constexpr uint64_t CPacketTS{0x6789ABCDEF};
constexpr int32_t CBoardID{0xE};

class AnstoDataTest : public TestBase {
protected:
  void SetUp() override {tsmin = -1; tsmax = 0;}
  void TearDown() override {}
  uint64_t tsmin, tsmax;
};

TEST_F(AnstoDataTest, Constructor) {
  DataParser parser("");
  EXPECT_EQ(0, parser.Stats.ErrorBytes);
}

// all zeroes in buffer
TEST_F(AnstoDataTest, ZeroedBuffer) {
  DataParser parser("");
  auto res = parser.parse((char *)&all_zeroes[0], HeaderSize, tsmin, tsmax);
  ASSERT_EQ(DataParser::error::EHEADER, -res);
  ASSERT_EQ(1, parser.Stats.SeqErrors);
}

// expected header values
TEST_F(AnstoDataTest, ValidBuffer) {
  SetupPacketHeader(valid_hdr, DataParser::PacketType::EVENT, CBoardID,
                    CPacketTS);
  SetupExtHeader(valid_hdr, CPacketCount, CAdcPk1, CAdcPk2, CUserReg, CBoard,
                 CTiming, CDataSel, CRateSel, CRateOut);
  DataParser parser("");
  auto res = parser.parse((char *)&valid_hdr[0], HeaderSize, tsmin, tsmax);
  ASSERT_EQ(res, 0);
  ASSERT_EQ(0, parser.Stats.ErrorBytes);
  ASSERT_EQ(parser.EHeader->header.packetCount, CPacketCount);
  ASSERT_EQ(parser.EHeader->header.adcPeak2, CAdcPk2);
  ASSERT_EQ(parser.EHeader->header.adcPeak1, CAdcPk1);
  ASSERT_EQ(parser.EHeader->header.userReg, CUserReg);
  ASSERT_EQ(parser.EHeader->header.boardID, CBoard);
  ASSERT_EQ(parser.EHeader->header.timingSystem, CTiming);
  ASSERT_EQ(parser.EHeader->header.dataSelect, CDataSel);
  ASSERT_EQ(parser.EHeader->header.rateSelect, CRateSel);
  ASSERT_EQ(parser.EHeader->header.rateOut, CRateOut);
}

TEST_F(AnstoDataTest, OneSample) {
  SetupPacketHeader(one_sample, DataParser::PacketType::EVENT, CBoardID,
                    CPacketTS);
  SetupExtHeader(one_sample, CPacketCount, CAdcPk1, CAdcPk2, CUserReg, CBoard,
                 CTiming, CDataSel, CRateSel, CRateOut);
  SetupNeutronData(one_sample, 0, 1, 2, 3, 4, 5);
  DataParser parser("");
  auto res = parser.parse((char *)&one_sample[0], HeaderSize + NeutronSize, tsmin, tsmax);
  ASSERT_EQ(res, 1);
  ASSERT_EQ(0, parser.Stats.ErrorBytes);
  ASSERT_EQ(parser.EHeader->header.packetCount, CPacketCount);
  ASSERT_EQ(parser.EHeader->header.adcPeak2, CAdcPk2);
  ASSERT_EQ(parser.EHeader->header.adcPeak1, CAdcPk1);
  ASSERT_EQ(parser.EHeader->header.userReg, CUserReg);
  ASSERT_EQ(parser.EHeader->header.boardID, CBoard);
  ASSERT_EQ(parser.EHeader->header.timingSystem, CTiming);
  ASSERT_EQ(parser.EHeader->header.dataSelect, CDataSel);
  ASSERT_EQ(parser.EHeader->header.rateSelect, CRateSel);
  ASSERT_EQ(parser.EHeader->header.rateOut, CRateOut);

  ASSERT_EQ(parser.Data[0].x, 1);
  ASSERT_EQ(parser.Data[0].y, 2);
  ASSERT_EQ(parser.Data[0].v, 3);
  ASSERT_EQ(parser.Data[0].w, 4);
  ASSERT_EQ(parser.Data[0].ts64, 5);

  ASSERT_EQ(parser.Readouts[0].event_type, Ansto::EventType::Neutron);
  ASSERT_EQ(parser.Readouts[0].source, CBoard);
  ASSERT_EQ(parser.Readouts[0].x_posn, 1);
  ASSERT_EQ(parser.Readouts[0].y_posn, 2);
  ASSERT_EQ(((parser.Readouts[0].data >> 16)) & 0xFFFF, 3);
  ASSERT_EQ((parser.Readouts[0].data & 0xFFFF), 4);
  ASSERT_EQ(parser.Readouts[0].timestamp, tsmin);
  ASSERT_EQ(parser.Readouts[0].timestamp, tsmax);
}

TEST_F(AnstoDataTest, TwoSample) {
  SetupPacketHeader(two_sample, DataParser::PacketType::EVENT, CBoardID,
                    CPacketTS);
  SetupExtHeader(two_sample, CPacketCount, CAdcPk1, CAdcPk2, CUserReg, CBoard,
                 CTiming, CDataSel, CRateSel, CRateOut);
  SetupNeutronData(two_sample, 0, 1, 2, 3, 4, 5);
  SetupNeutronData(two_sample, 1, 6, 7, 8, 9, 10);

  DataParser parser("");
  auto res = parser.parse((char *)&two_sample[0], HeaderSize + 2 * NeutronSize, tsmin, tsmax);
  ASSERT_EQ(res, 2);
  ASSERT_EQ(0, parser.Stats.ErrorBytes);
  ASSERT_EQ(parser.EHeader->header.packetCount, CPacketCount);
  ASSERT_EQ(parser.EHeader->header.adcPeak2, CAdcPk2);
  ASSERT_EQ(parser.EHeader->header.adcPeak1, CAdcPk1);
  ASSERT_EQ(parser.EHeader->header.userReg, CUserReg);
  ASSERT_EQ(parser.EHeader->header.boardID, CBoard);
  ASSERT_EQ(parser.EHeader->header.timingSystem, CTiming);
  ASSERT_EQ(parser.EHeader->header.dataSelect, CDataSel);
  ASSERT_EQ(parser.EHeader->header.rateSelect, CRateSel);
  ASSERT_EQ(parser.EHeader->header.rateOut, CRateOut);

  ASSERT_EQ(parser.Data[0].x, 1);
  ASSERT_EQ(parser.Data[0].y, 2);
  ASSERT_EQ(parser.Data[0].v, 3);
  ASSERT_EQ(parser.Data[0].w, 4);
  ASSERT_EQ(parser.Data[0].ts64, 5);

  ASSERT_EQ(parser.Data[1].x, 6);
  ASSERT_EQ(parser.Data[1].y, 7);
  ASSERT_EQ(parser.Data[1].v, 8);
  ASSERT_EQ(parser.Data[1].w, 9);
  ASSERT_EQ(parser.Data[1].ts64, 10);

  ASSERT_EQ(parser.Readouts[0].event_type, Ansto::EventType::Neutron);
  ASSERT_EQ(parser.Readouts[0].source, CBoard);
  ASSERT_EQ(parser.Readouts[0].x_posn, 1);
  ASSERT_EQ(parser.Readouts[0].y_posn, 2);
  ASSERT_EQ(((parser.Readouts[0].data >> 16)) & 0xFFFF, 3);
  ASSERT_EQ((parser.Readouts[0].data & 0xFFFF), 4);

  // time is rel minimum in nsecs (*100)
  ASSERT_EQ(parser.Readouts[1].event_type, Ansto::EventType::Neutron);
  ASSERT_EQ(parser.Readouts[1].source, CBoard);
  ASSERT_EQ(parser.Readouts[1].x_posn, 6);
  ASSERT_EQ(parser.Readouts[1].y_posn, 7);
  ASSERT_EQ(((parser.Readouts[1].data >> 16)) & 0xFFFF, 8);
  ASSERT_EQ((parser.Readouts[1].data & 0xFFFF), 9);
  ASSERT_EQ(parser.Readouts[1].timestamp - parser.Readouts[0].timestamp, 500);

  ASSERT_EQ(tsmin, parser.Readouts[0].timestamp);
  ASSERT_EQ(tsmax, parser.Readouts[1].timestamp);
}
