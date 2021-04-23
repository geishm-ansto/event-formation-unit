/** Copyright (C) 2017-2018 European Spallation Source */
//===----------------------------------------------------------------------===//
///
/// \file
///
//===----------------------------------------------------------------------===//

#include <ansto/bnlp/BnlpParser.h>
#include <test/TestBase.h>

using namespace Ansto;
using namespace Ansto::Bnlp;

void SetupHeader(std::vector<uint8_t> &buffer, uint32_t packetCount,
                 uint16_t adcPk1, uint16_t adcPk2, uint32_t userReg,
                 uint8_t board, uint8_t timing, uint8_t dataSel,
                 uint8_t rateSel, uint32_t rateOut) {

  buffer[0] = 0xFF & (packetCount >> 24);
  buffer[1] = 0xFF & (packetCount >> 16);
  buffer[2] = 0xFF & (packetCount >> 8);
  buffer[3] = 0xFF & packetCount;

  buffer[4] = 0xFF & (adcPk2 >> 8);
  buffer[5] = 0xFF & adcPk2;
  buffer[6] = 0xFF & (adcPk1 >> 8);
  buffer[7] = 0xFF & adcPk1;

  buffer[8] = 0xFF & (userReg >> 12);
  buffer[9] = 0xFF & (userReg >> 4);
  buffer[10] = ((0xF & userReg) << 4) | (0xF & board);

  buffer[11] = ((0xF & timing) << 4) | (0xF & dataSel);

  buffer[12] = ((0x1 & rateSel) << 7) | (0x7F & (rateOut >> 24));
  buffer[13] = 0xFF & (rateOut >> 16);
  buffer[14] = 0xFF & (rateOut >> 8);
  buffer[15] = 0xFF & rateOut;
}

void SetupSampleData(std::vector<uint8_t> &buffer, size_t bufOfs,
                     uint8_t tsCoarse, uint32_t tsFine, uint8_t adc,
                     uint8_t timing, uint8_t chipNum, uint8_t channelNum) {
  buffer[bufOfs + 4] = tsCoarse;
  buffer[bufOfs + 5] = 0xFF & (tsFine >> 10);
  buffer[bufOfs + 2] = 0xFF & (tsFine >> 2);
  buffer[bufOfs + 3] = ((0x3 & tsFine) << 6) | (0x3F & adc);
  buffer[bufOfs + 0] = ((0xC & timing) << 4) | (0x3F & chipNum);
  buffer[bufOfs + 1] = ((0x3 & timing) << 6) | (0x3F & channelNum);
}

void SetupTimestampEvent(std::vector<uint8_t> &buffer, size_t bufOfs,
                         uint32_t timestamp, uint8_t chipID, uint8_t qdb) {
  buffer[bufOfs + 4] = 0xFF & (timestamp >> 24);
  buffer[bufOfs + 5] = 0xFF & (timestamp >> 16);
  buffer[bufOfs + 2] = 0xFF & (timestamp >> 8);
  buffer[bufOfs + 3] = 0xFF & timestamp;
  buffer[bufOfs] = chipID;
  buffer[bufOfs + 1] = qdb;
}

// Tests that need to be implemented
// Confirm the Header is correctly read, checking header size and each term.
// Confirm

std::vector<uint8_t> empty_msg(0, 0);
std::vector<uint8_t> all_zeroes(16, 0);
std::vector<uint8_t> valid_hdr(16, 0);
std::vector<uint8_t> one_sample(22, 0);
std::vector<uint8_t> one_rollover(22, 0);
std::vector<uint8_t> partials(34, 0);

constexpr uint32_t CPacketCount{0x1020304};
constexpr uint16_t CAdcPk1{0x3423};
constexpr uint16_t CAdcPk2{0x5643};
constexpr uint32_t CUserReg{0x73412};
constexpr uint8_t CBoard{0xE};
constexpr uint8_t CRollover{0x3F};
constexpr uint16_t CTiming{0x1};
constexpr uint32_t CRateOut{0x6E78E};
constexpr uint8_t CDataSel{0x0};
constexpr uint8_t CCounterSel{0x1};
constexpr uint16_t CRateSel{0x1};

class AnstoDataTest : public TestBase {
protected:
  void SetUp() override {
    tsmin = -1;
    tsmax = 0;
  }
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
  auto hdrSz = sizeof(struct Header);
  auto res = parser.parse((char *)&all_zeroes[0], hdrSz, tsmin, tsmax);
  ASSERT_EQ(res, 0);
  ASSERT_EQ(1, parser.Stats.SeqErrors);
}

// expected header values
TEST_F(AnstoDataTest, ValidBuffer) {

  SetupHeader(valid_hdr, CPacketCount, CAdcPk1, CAdcPk2, CUserReg, CBoard,
              CTiming, CDataSel, CRateSel, CRateOut);

  auto hdr = (struct Header &)(valid_hdr[0]);
  ASSERT_EQ(hdr.PacketCnt(), CPacketCount);
  ASSERT_EQ(hdr.AdcPeak2(), CAdcPk2);
  ASSERT_EQ(hdr.AdcPeak1(), CAdcPk1);
  ASSERT_EQ(hdr.UserReg(), CUserReg);
  ASSERT_EQ(hdr.BoardID(), CBoard);
  ASSERT_EQ(hdr.TimingSelect(), CTiming);
  ASSERT_EQ(hdr.DataSelect(), CDataSel);
  ASSERT_EQ(hdr.RateSelect(), CRateSel);
  ASSERT_EQ(hdr.RateOut(), CRateOut);

  DataParser parser("");
  auto hdrSz = sizeof(struct Header);
  auto res = parser.parse((char *)&valid_hdr[0], hdrSz, tsmin, tsmax);

  ASSERT_EQ(res, 0);
  ASSERT_EQ(0, parser.Stats.ErrorBytes);
  ASSERT_EQ(parser.Header->PacketCnt(), CPacketCount);
  ASSERT_EQ(parser.Header->AdcPeak2(), CAdcPk2);
  ASSERT_EQ(parser.Header->AdcPeak1(), CAdcPk1);
  ASSERT_EQ(parser.Header->UserReg(), CUserReg);
  ASSERT_EQ(parser.Header->BoardID(), CBoard);
  ASSERT_EQ(parser.Header->TimingSelect(), CTiming);
  ASSERT_EQ(parser.Header->DataSelect(), CDataSel);
  ASSERT_EQ(parser.Header->RateSelect(), CRateSel);
  ASSERT_EQ(parser.Header->RateOut(), CRateOut);
}

TEST_F(AnstoDataTest, ValidOneSample) {
  constexpr uint8_t CoarseTS{23};
  constexpr uint32_t FineTS{1234};
  constexpr uint8_t ADC{45};
  constexpr uint8_t Timing{13};
  constexpr uint8_t ChipNum{34};
  constexpr uint8_t ChannelNum{28};
  constexpr size_t DataOfs{sizeof(struct Header)};

  SetupHeader(one_sample, CPacketCount, CAdcPk1, CAdcPk2, CUserReg, CBoard,
              CTiming, CDataSel, CRateSel, CRateOut);
  SetupSampleData(one_sample, DataOfs, CoarseTS, FineTS, ADC, Timing, ChipNum,
                  ChannelNum);

  auto data = (struct SampleData &)(one_sample[DataOfs]);
  ASSERT_EQ(data.CoarseTS(), CoarseTS);
  ASSERT_EQ(data.FineTS(), FineTS);
  ASSERT_EQ(data.AdcValue(), ADC);
  ASSERT_EQ(data.Timing(), Timing);
  ASSERT_EQ(data.ChipNum(), ChipNum);
  ASSERT_EQ(data.ChannelNum(), ChannelNum);

  DataParser parser("");
  auto res = parser.parse((char *)&one_sample[0], one_sample.size(), tsmin, tsmax);

  ASSERT_EQ(res, 1);
  ASSERT_EQ(0, parser.Stats.ErrorBytes);
  ASSERT_EQ(1, parser.Readouts.size());

  ASSERT_EQ(Ansto::EventType::Neutron, parser.Readouts[0].event_type);
  ASSERT_EQ(CBoard, parser.Readouts[0].source);
  ASSERT_EQ(ADC, parser.Readouts[0].weight);
}

TEST_F(AnstoDataTest, RolloverEvent) {
  constexpr uint8_t CoarseTS{23};
  constexpr size_t DataOfs{sizeof(struct Header)};

  SetupHeader(one_rollover, CPacketCount, CAdcPk1, CAdcPk2, CUserReg, CBoard,
              CTiming, CDataSel, CRateSel, CRateOut);
  SetupTimestampEvent(one_rollover, DataOfs, (CoarseTS << 18), 0x3F, 0);

  DataParser parser("");
  auto res = parser.parse((char *)&one_rollover[0], one_rollover.size(), tsmin, tsmax);

  ASSERT_EQ(res, 1);
  ASSERT_EQ(0, parser.Stats.ErrorBytes);
  ASSERT_EQ(1, parser.Readouts.size());
  ASSERT_EQ((CoarseTS << 18), parser.Timestamps[CBoard].full64);

  SetupHeader(one_rollover, CPacketCount, CAdcPk1, CAdcPk2, CUserReg, CBoard,
              CTiming, CDataSel, CRateSel, CRateOut);
  SetupTimestampEvent(one_rollover, DataOfs, 0, 0x3F, 0);

  res = parser.parse((char *)&one_rollover[0], one_rollover.size(), tsmin, tsmax);

  ASSERT_EQ(res, 1);
  ASSERT_EQ(0, parser.Stats.ErrorBytes);
  ASSERT_EQ(1, parser.Readouts.size());
  ASSERT_EQ((1LL << 32), parser.Timestamps[CBoard].full64);
}

TEST_F(AnstoDataTest, PartialEvents) {
  constexpr uint8_t CoarseTS{23};
  constexpr uint32_t FineTS{1234};
  constexpr uint8_t ADC{45};
  constexpr uint8_t Timing{13};
  constexpr uint8_t ChipNum{34};
  constexpr uint8_t ChannelNum{28};
  constexpr size_t DataOfs{sizeof(struct Header)};

  SetupHeader(partials, CPacketCount, CAdcPk1, CAdcPk2, CUserReg, CBoard,
              CTiming, CDataSel, CRateSel, CRateOut);
  SetupSampleData(partials, DataOfs, CoarseTS, 100, ADC, Timing, ChipNum,
                  ChannelNum);
  SetupSampleData(partials, DataOfs + 6, CoarseTS, FineTS, ADC, Timing, ChipNum,
                  ChannelNum);

  DataParser parser("");
  auto res =
      parser.parse((char *)&partials[0], DataOfs + 2, tsmin, tsmax); // leave a residual
  ASSERT_EQ(res, 0);
  SetUp();  // reset tsmin, tsmax
  SetupHeader(partials, CPacketCount + 1, CAdcPk1, CAdcPk2, CUserReg, CBoard,
              CTiming, CDataSel, CRateSel, CRateOut);
  memmove(partials.data() + DataOfs, partials.data() + DataOfs + 2, 10);
  res = parser.parse((char *)&partials[0], DataOfs + 4 + 6, tsmin, tsmax);

  ASSERT_EQ(res, 2);
  ASSERT_EQ(0, parser.Stats.ErrorBytes);
  ASSERT_EQ(2, parser.Readouts.size());
  uint64_t basetime = parser.Readouts[0].timestamp;
  ASSERT_EQ(Ansto::EventType::Neutron, parser.Readouts[0].event_type);
  ASSERT_EQ(CBoard, parser.Readouts[0].source);
  ASSERT_EQ(ADC, parser.Readouts[0].weight);

  ASSERT_EQ((FineTS - 100) * 100, parser.Readouts[1].timestamp - basetime);
  ASSERT_EQ(Ansto::EventType::Neutron, parser.Readouts[1].event_type);
  ASSERT_EQ(CBoard, parser.Readouts[1].source);
  ASSERT_EQ(ADC, parser.Readouts[1].weight);

  ASSERT_EQ(tsmin, parser.Readouts[0].timestamp);
  ASSERT_EQ(tsmax, parser.Readouts[1].timestamp);
}

TEST_F(AnstoDataTest, CPPixelMapping) {
  struct CP {
    int chip;
    int channel;
    Coordinate coord;
  };

  std::vector<struct CP> MaptoPadTests{
      {0, 46, {7, 7}},          {0, 45, {2, 7}},          {0, 0, {7, 3}},
      {0, 10, {5, 1}},          {9, 46, {0 + 8, 0 + 16}}, // flipped
      {9, 45, {5 + 8, 0 + 16}},                           // flipped
      {13, 0, {7 + 16, 3 + 8}}, {13, 10, {5 + 16, 1 + 8}}};

  for (auto &t : MaptoPadTests) {
    int x, y;
    MapToPad(t.chip, t.channel, x, y);
    ASSERT_EQ(x, t.coord.x);
    ASSERT_EQ(y, t.coord.y);
  }
}

TEST_F(AnstoDataTest, BCPPixelMapping) {
  struct BCP {
    int board;
    int chip;
    int channel;
    Coordinate coord;
  };

  std::vector<struct BCP> MaptoPadTests{
      {4, 0, 46, {40, 7}}, // zero rotn, zero offset: board id 4 -> 5, f->b
      {4, 0, 45, {45, 7}},
      {7, 0, 0, {40 + 48, 3}}, // zero rotn: board id 7 -> 4
      {7, 0, 10, {42 + 48, 1}},
      {10, 9, 46, {39 + 144, 0 + 16 + 96}}, // zero rotn: 10 -> 8
      {9, 9, 45, {34 + 96, 0 + 16 + 96}}    // zero rotn: 9 -> 9
  };

  // The final rotation is in question so apply the inverse before
  // checking the value
  auto rot90 = [&](int n, Ansto::Bnlp::Coordinate &c) {
    auto t = c.y;
    c.y = n - 1 - c.x;
    c.x = t;
  };

  for (auto &t : MaptoPadTests) {
    auto c = DecodeToPixel(t.board, t.chip, t.channel);
    rot90(192, c);
    ASSERT_EQ(c.x, t.coord.x);
    ASSERT_EQ(c.y, t.coord.y);
  }
}