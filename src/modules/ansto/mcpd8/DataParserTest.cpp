/** Copyright (C) 2017-2018 European Spallation Source */
//===----------------------------------------------------------------------===//
///
/// \file
///
//===----------------------------------------------------------------------===//

#include <ansto/mcpd8/DataParser.h>
#include <test/TestBase.h>

using namespace Ansto;
using namespace Ansto::Mcpd8;

std::vector<uint16_t> all_zeroes(21, 0);
std::vector<uint16_t> valid_command(21, 0);
std::vector<uint16_t> no_events(21, 0);
std::vector<uint16_t> mdll_no_events(21, 0);
std::vector<uint16_t> bad_sequence(21, 0);
std::vector<uint16_t> one_event(24, 0);
std::vector<uint16_t> event_error(21, 0);
std::vector<uint16_t> event_residue(28, 0);
std::vector<uint16_t> mdll_events(27, 0);
std::vector<uint16_t> trig_events(51, 0);

class AnstoDataTest : public TestBase {
protected:
  void SetUp() override {
    tsmin = -1;
    tsmax = 0;
    valid_command[0] = 9;
    valid_command[1] = 0xf000;
    FillMessage(no_events);
    FillMessage(mdll_no_events);
    FillMessage(bad_sequence);
    bad_sequence[3] = 5;
    FillMessage(one_event, 1);
    FillMessage(event_error, 0);
    event_error[0] += 2;
    FillMessage(event_residue, 2);
    event_residue[0] += 1;
    FillMessage(mdll_events, 2, true);
    FillMessage(trig_events, 2, false, 8);
  }
  void TearDown() override {}

  void FillMessage(std::vector<uint16_t> &msg, uint16_t nevents = 0,
                   bool is_mdll = false, uint16_t trigevs = 0) {
    msg[0] = 21 + 3 * (nevents + trigevs);
    msg[1] = (is_mdll ? 2 : 0);
    msg[2] = 9;
    msg[3] = 1;
    msg[5] = 0x0003;

    // timestamp
    msg[6] = 0x0001;
    msg[7] = 0x0010;
    msg[8] = 0x0100;

    // add linear events
    for (size_t i = 0; i < nevents; i++) {
      msg[21 + i * 3] = i + 1; // timestamp
      if (is_mdll) {
        if (i % 2 == 1) {
          msg[22 + i * 3] = ((i + 10) << 3);         // data (i+10)
          msg[23 + i * 3] = 0x8000 | ((i + 4) << 8); // data id (i+4)
        } else {
          msg[22 + i * 3] =
              ((i + 2) << 13) | ((i + 3) << 3); // x posn (i+2), y posn (i+3)
          msg[23 + i * 3] = 0x0000 | ((i + 4) << 7); // ampl (i+4)
        }
      } else {
        msg[22 + i * 3] =
            ((i + 2) << 13) | ((i + 3) << 3); // ampl (i+2), y posn (i+3)
        msg[23 + i * 3] = 0x0000 | (1 << 12) | (i << 7); // x posn (i+8)
      }
    }

    // add trigger events
    uint16_t k = 0;
    for (size_t i = nevents; i < nevents + trigevs; i++) {
      msg[21 + i * 3] = i + 1;          // timestamp
      msg[22 + i * 3] = ((i + 3) << 3); // data (i+3)
      msg[23 + i * 3] = 0x8000 | ((k % 8) << 12) |
                        ((i % 5) << 8); // trigID (k % 8), dataID (i % 5)
      k++;
    }
  }
  uint64_t tsmin, tsmax;
};

/** Test cases below */
TEST_F(AnstoDataTest, Constructor) {
  DataParser parser("");
  EXPECT_EQ(0, parser.Stats.ErrorBytes);
  EXPECT_EQ(nullptr, parser.Data);
  EXPECT_EQ(nullptr, parser.Header);
}

// all zeroes in buffer
TEST_F(AnstoDataTest, ZeroedBuffer) {
  DataParser parser("");
  auto hdrSz = sizeof(struct DataParser::Header);
  auto res = parser.parse((char *)&all_zeroes[0], hdrSz - 1, tsmin, tsmax);
  ASSERT_EQ(res, -parser.error::ESIZE);
  ASSERT_EQ(nullptr, parser.Header);
  ASSERT_EQ(nullptr, parser.Data);
  ASSERT_EQ(hdrSz - 1, parser.Stats.ErrorBytes);

  // mismatch between actual and expected buffer length
  res = parser.parse((char *)&all_zeroes[0], hdrSz, tsmin, tsmax);
  ASSERT_EQ(res, -parser.error::ESIZE);
  ASSERT_NE(nullptr, parser.Header);
  ASSERT_EQ(nullptr, parser.Data);
  ASSERT_EQ(hdrSz, parser.Stats.ErrorBytes);
  ASSERT_EQ(0, parser.Header->bufferLength);
}

// valid command message
TEST_F(AnstoDataTest, ValidCommand) {
  DataParser parser("");
  auto res = parser.parse((char *)&valid_command[0], valid_command.size() * 2,
                          tsmin, tsmax);
  ASSERT_EQ(res, 0);
  ASSERT_EQ(0, parser.Stats.ErrorBytes);
  ASSERT_EQ(0, parser.Stats.SeqErrors);
}

// missing messages
TEST_F(AnstoDataTest, MissingMessage) {
  DataParser parser("");
  auto res = parser.parse((char *)&bad_sequence[0], bad_sequence.size() * 2,
                          tsmin, tsmax);
  ASSERT_EQ(res, 0);
  ASSERT_EQ(0, parser.Stats.ErrorBytes);
  ASSERT_EQ(1, parser.Stats.SeqErrors);
}

// valid event header but no data
TEST_F(AnstoDataTest, ValidNoEvents) {
  DataParser parser("");
  auto res =
      parser.parse((char *)&no_events[0], no_events.size() * 2, tsmin, tsmax);
  ASSERT_EQ(res, 0);
  ASSERT_EQ(0, parser.Stats.ErrorBytes);
  ASSERT_EQ(0, parser.Stats.SeqErrors);
}

// one event passed
TEST_F(AnstoDataTest, OneEvent) {
  DataParser parser("");
  auto res =
      parser.parse((char *)&one_event[0], one_event.size() * 2, tsmin, tsmax);
  ASSERT_EQ(res, 1);
  ASSERT_EQ(0, parser.Stats.ErrorBytes);
  ASSERT_EQ(0, parser.Stats.SeqErrors);
  uint64_t value = 0x10000100001 * 100;
  ASSERT_EQ(value, parser.Header->nanosecs());
  parser.Header->settime(value);
  ASSERT_EQ(value, parser.Header->nanosecs());
}

// event error
TEST_F(AnstoDataTest, EventError) {
  DataParser parser("");
  auto sz = event_error.size() * 2;
  auto res = parser.parse((char *)&event_error[0], sz, tsmin, tsmax);
  ASSERT_EQ(res, -parser.error::ESIZE);
  ASSERT_EQ(sz, parser.Stats.ErrorBytes);
  ASSERT_EQ(0, parser.Stats.SeqErrors);
}

// two events passed
TEST_F(AnstoDataTest, EventResidue) {
  DataParser parser("");
  auto sz = event_residue.size() * 2;
  auto res = parser.parse((char *)&event_residue[0], sz, tsmin, tsmax);
  ASSERT_EQ(res, 2);
  ASSERT_EQ(sz, parser.Stats.ErrorBytes);
  ASSERT_EQ(0, parser.Stats.SeqErrors);
  uint64_t basetime = parser.Readouts[0].timestamp;
  ASSERT_EQ(100, parser.Readouts[1].timestamp - basetime);
  ASSERT_EQ(8, parser.Readouts[0].x_posn);
  ASSERT_EQ(9, parser.Readouts[1].x_posn);
  ASSERT_EQ(3, parser.Readouts[0].y_posn);
  ASSERT_EQ(4, parser.Readouts[1].y_posn);
  ASSERT_EQ(2, parser.Readouts[0].weight);
  ASSERT_EQ(3, parser.Readouts[1].weight);
}

// two mdll events passed, 1 neutron and 1 trigger
TEST_F(AnstoDataTest, TwoMdllEvents) {
  DataParser parser("");
  auto sz = mdll_events.size() * 2;
  auto res = parser.parse((char *)&mdll_events[0], sz, tsmin, tsmax);
  ASSERT_EQ(res, 2);
  ASSERT_EQ(0, parser.Stats.ErrorBytes);
  ASSERT_EQ(0, parser.Stats.SeqErrors);
  ASSERT_EQ(Ansto::Neutron, parser.Readouts[0].event_type);
  uint64_t basetime = parser.Readouts[0].timestamp;
  ASSERT_EQ(2, parser.Readouts[0].x_posn);
  ASSERT_EQ(3, parser.Readouts[0].y_posn);
  ASSERT_EQ(4, parser.Readouts[0].weight);

  ASSERT_NE(Ansto::Neutron, parser.Readouts[1].event_type);
  ASSERT_EQ(100, parser.Readouts[1].timestamp - basetime);
  ASSERT_EQ(tsmin, parser.Readouts[0].timestamp);
  ASSERT_EQ(tsmax, parser.Readouts[1].timestamp);
  ASSERT_EQ(0, parser.Readouts[1].x_posn);
  ASSERT_EQ(0, parser.Readouts[1].y_posn);
  auto t = (DataParser::TriggerData *)&parser.Readouts[1].data;
  ASSERT_EQ(11, t->data);
  ASSERT_EQ(5, t->source);
}

TEST_F(AnstoDataTest, IncludeTriggers) {
  DataParser parser("");
  auto sz = trig_events.size() * 2;
  auto res = parser.parse((char *)&trig_events[0], sz, tsmin, tsmax);
  ASSERT_EQ(res, 10);

  for (size_t i = 2; i < 10; i++) {
    ASSERT_NE(Ansto::Neutron, parser.Readouts[i].event_type);
    ASSERT_EQ(i * 100,
              parser.Readouts[i].timestamp - parser.Readouts[0].timestamp);
    ASSERT_EQ(tsmin, parser.Readouts[0].timestamp);
    ASSERT_EQ(tsmax, parser.Readouts[9].timestamp);
    ASSERT_EQ(0, parser.Readouts[i].x_posn);
    ASSERT_EQ(0, parser.Readouts[i].y_posn);
    auto *t = (DataParser::TriggerData *)&parser.Readouts[i].data;
    ASSERT_EQ(i + 3, t->data);
    ASSERT_EQ((i % 5), t->source);
  }
}
