/** Copyright (C) 2016, 2017 European Spallation Source ERIC */

#include <loki/readout/DataParser.h>
#include <loki/readout/DataParserTestData.h>
#include <test/TestBase.h>

using namespace Loki;

class DataParserTest : public TestBase {
protected:
  DataParser Parser;
  void SetUp() override {}
  void TearDown() override {}
};


TEST_F(DataParserTest, Constructor) {
  ASSERT_EQ(Parser.Stats.Readouts, 0);
  ASSERT_EQ(Parser.Stats.Headers, 0);
  ASSERT_EQ(Parser.Stats.ErrorHeaders, 0);
  ASSERT_EQ(Parser.Stats.ErrorBytes, 0);
}

TEST_F(DataParserTest, BadSize) {
  auto Res = Parser.parse((char *)&ErrBadRingGoodFEN[0], 3);
  ASSERT_EQ(Res, 0);
  ASSERT_EQ(Parser.Stats.Readouts, 0);
  ASSERT_EQ(Parser.Stats.ErrorBytes, 3);
}

TEST_F(DataParserTest, BadRingGoodFEN) {
  auto Res = Parser.parse((char *)&ErrBadRingGoodFEN[0], 4);
  ASSERT_EQ(Res, 0);
  ASSERT_EQ(Parser.Stats.Readouts, 0);
  ASSERT_EQ(Parser.Stats.ErrorHeaders, 1);
  ASSERT_EQ(Parser.Stats.ErrorBytes, 4);
}

TEST_F(DataParserTest, GoodRingBadFEN) {
  auto Res = Parser.parse((char *)&ErrGoodRingBadFEN[0], 4);
  ASSERT_EQ(Res, 0);
  ASSERT_EQ(Parser.Stats.Readouts, 0);
  ASSERT_EQ(Parser.Stats.ErrorHeaders, 1);
  ASSERT_EQ(Parser.Stats.ErrorBytes, 4);
}

TEST_F(DataParserTest, GoodRingGoodFEN) {
  auto Res = Parser.parse((char *)&OkThreeLokiReadouts[0], 4);
  ASSERT_EQ(Res, 0);
  ASSERT_EQ(Parser.Stats.Readouts, 0);
  ASSERT_EQ(Parser.Stats.ErrorHeaders, 0);
  ASSERT_EQ(Parser.Stats.ErrorBytes, 0);
}



int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
