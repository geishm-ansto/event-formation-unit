/** Copyright (C) 2017-2018 European Spallation Source */
//===----------------------------------------------------------------------===//
///
/// \file
///
//===----------------------------------------------------------------------===//

#include <ansto/generators/mcpd8.h>
#include <ansto/mcpd8/DataParser.h>
#include <test/TestBase.h>

using namespace Ansto;
using namespace Ansto::Mcpd8;
using namespace Ansto::Generator;

class Mcpd8GenTest : public TestBase {
protected:
  void SetUp() override {
    ConfigFile = TEST_JSON_PATH "testsim.json";
  }
  void TearDown() override {}

  std::string ConfigFile;
};

// default json config parameters

/** Test cases below */
TEST_F(Mcpd8GenTest, ParseTest) {
  Mcpd8Source source("", ConfigFile);
  DataParser parser("");
  char buffer[10000];
  const int MaxBufferSize = 8900;
  uint64_t TSmin = -1;
  uint64_t TSmax = 0;
  auto datalen = source.Next(buffer, MaxBufferSize);
  parser.parse(buffer, datalen, TSmin, TSmax);

  EXPECT_EQ(0, parser.Stats.ErrorBytes);
  EXPECT_EQ(parser.Header->bufferLength, (datalen / 2));
  EXPECT_EQ(parser.Header->bufferType, 0);

  // Check the sequence of neutron events
  auto readouts = parser.getReadouts();
  auto prev = readouts[0];
  for (size_t i = 1; i < readouts.size(); i++) {
    auto &curr = readouts[i];
    EXPECT_EQ((curr.timestamp - prev.timestamp), 100);
    EXPECT_EQ((curr.y_posn - prev.y_posn), 1);
    EXPECT_EQ((curr.weight - prev.weight), 1);
    prev = curr;
  }

  uint64_t pts = 0;
  const int delay = 10'000;
  for (int i = 0; i < 10; i++) {
    usleep(delay);
    auto datalen = source.Next(buffer, MaxBufferSize);
    parser.parse(buffer, datalen, TSmin, TSmax);
    uint64_t cts = parser.Header->nanosecs();
    if (pts > 0) {
      EXPECT_GE((cts - pts), delay * 1000);
      EXPECT_LT((cts - pts), delay * 2 * 1000);
    }
    pts = cts;
  }
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
