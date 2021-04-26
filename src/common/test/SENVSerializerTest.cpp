/** Copyright (C) 2016, 2017 European Spallation Source ERIC */

#include <common/SENVSerializer.h>
#include <common/Producer.h>
#include <cstring>
#include <test/TestBase.h>
#include "senv_data_generated.h"

//#define ARRAYLENGTH 125000
#define ARRAYLENGTH 10

struct MockProducer {
  inline void produce(nonstd::span<const uint8_t>, int64_t)
  {
    NumberOfCalls++;
  }


  size_t NumberOfCalls {0};
};

class SENVSerializerTest : public TestBase {
  void SetUp() override {
    for (int i = 0; i < 200000; i++) {
      time[i] = i;
      data[i] = i % 1000;
    }
  }

  void TearDown() override {}

protected:
  uint8_t flatbuffer[1024 * 1024];
  uint64_t time[200000];
  uint16_t data[200000];
};

TEST_F(SENVSerializerTest, Serialize) {
  SENVSerializerU16 fb{ARRAYLENGTH, "nameless"};
  for (size_t i=0; i < ARRAYLENGTH; i++)
    fb.addEvent(i, i);
  auto buffer = fb.serialize();
  EXPECT_GE(buffer.size_bytes(), ARRAYLENGTH * 8);
  EXPECT_LE(buffer.size_bytes(), ARRAYLENGTH * 8 + 2048);
  ASSERT_TRUE(not buffer.empty());

  EXPECT_EQ(std::string(reinterpret_cast<const char*>(&buffer[4]), 4), "senv");
}

TEST_F(SENVSerializerTest, SerDeserialize) {
  SENVSerializerU16 fb{ARRAYLENGTH, "nameless"};
  for (size_t i=0; i < ARRAYLENGTH-1; i++)
    fb.addEvent(i, i);
  auto buffer = fb.serialize();

  memset(flatbuffer, 0, sizeof(flatbuffer));

  auto events = GetSampleEnvironmentData(flatbuffer);

  ASSERT_NE(events->MessageCounter(), 1);

  memcpy(flatbuffer, buffer.data(), buffer.size_bytes());
  EXPECT_EQ(std::string(reinterpret_cast<char*>(&flatbuffer[4]), 4), "senv");
  events = GetSampleEnvironmentData(flatbuffer);
  EXPECT_EQ(events->Name()->str(), "nameless");
  ASSERT_EQ(events->MessageCounter(), 1);
}

TEST_F(SENVSerializerTest, DeserializeCheckData16) {
  SENVSerializerU16 fb{ARRAYLENGTH, "nameless"};
  for (int i = 0; i < ARRAYLENGTH - 1; i++) {
    auto len = fb.addEvent(time[i], data[i]);
    ASSERT_EQ(len, 0);
    ASSERT_EQ(fb.eventCount(), i+1);
  }

  constexpr uint32_t CChannelId{23};
  constexpr Location CLocation{Location::Middle};
  constexpr uint64_t CPacketTS{123456};
  constexpr double CTimeDelta{7.0};

  fb.setChannel(CChannelId);
  fb.setPacketTimestamp(CPacketTS);
  fb.setTimestampLocation(CLocation);
  fb.setTimeDelta(CTimeDelta);

  EXPECT_EQ(fb.getChannel(), CChannelId);
  EXPECT_EQ(fb.getPacketTimestamp(), CPacketTS);
  EXPECT_EQ(fb.getTimestampLocation(), CLocation);
  EXPECT_EQ(fb.getTimeDelta(), CTimeDelta);

  auto buffer = fb.serialize();
  ASSERT_TRUE(not buffer.empty());

  memcpy(flatbuffer, buffer.data(), buffer.size_bytes());
  EXPECT_EQ(std::string(reinterpret_cast<char*>(&flatbuffer[4]), 4), "senv");

  auto veri = flatbuffers::Verifier(buffer.data(), buffer.size_bytes());
  ASSERT_TRUE(VerifySampleEnvironmentDataBuffer(veri));
  auto events = GetSampleEnvironmentData(flatbuffer);
  EXPECT_EQ(events->Name()->str(), "nameless");

  auto timevec = events->Timestamps();
  EXPECT_EQ(timevec->size(), ARRAYLENGTH - 1);
  auto valuevec = events->Values_as_UInt16Array()->value();
  EXPECT_EQ(valuevec->size(), ARRAYLENGTH - 1);

  for (int i = 0; i < ARRAYLENGTH - 1; i++) {
    EXPECT_EQ((*timevec)[i], time[i]);
    EXPECT_EQ((*valuevec)[i], data[i]);
  }

  EXPECT_EQ(events->Name()->str(), "nameless");
  EXPECT_EQ(events->Channel(), CChannelId);
  EXPECT_EQ(events->PacketTimestamp(), CPacketTS);
  EXPECT_EQ(events->TimestampLocation(), CLocation);
  EXPECT_EQ(events->TimeDelta(), CTimeDelta);
  EXPECT_EQ(events->Name()->str(), "nameless");
}

TEST_F(SENVSerializerTest, DeserializeCheckData64) {
  SENVSerializerU64 fb{ARRAYLENGTH, "nameless"};
  for (int i = 0; i < ARRAYLENGTH - 1; i++) {
    auto len = fb.addEvent(time[i], data[i]);
    ASSERT_EQ(len, 0);
    ASSERT_EQ(fb.eventCount(), i+1);
  }

  constexpr uint32_t CChannelId{23};
  constexpr Location CLocation{Location::Middle};
  constexpr uint64_t CPacketTS{123456};
  constexpr double CTimeDelta{7.0};

  fb.setChannel(CChannelId);
  fb.setPacketTimestamp(CPacketTS);
  fb.setTimestampLocation(CLocation);
  fb.setTimeDelta(CTimeDelta);

  EXPECT_EQ(fb.getChannel(), CChannelId);
  EXPECT_EQ(fb.getPacketTimestamp(), CPacketTS);
  EXPECT_EQ(fb.getTimestampLocation(), CLocation);
  EXPECT_EQ(fb.getTimeDelta(), CTimeDelta);

  auto buffer = fb.serialize();
  ASSERT_TRUE(not buffer.empty());

  memcpy(flatbuffer, buffer.data(), buffer.size_bytes());
  EXPECT_EQ(std::string(reinterpret_cast<char*>(&flatbuffer[4]), 4), "senv");

  auto veri = flatbuffers::Verifier(buffer.data(), buffer.size_bytes());
  ASSERT_TRUE(VerifySampleEnvironmentDataBuffer(veri));
  auto events = GetSampleEnvironmentData(flatbuffer);
  EXPECT_EQ(events->Name()->str(), "nameless");

  auto timevec = events->Timestamps();
  EXPECT_EQ(timevec->size(), ARRAYLENGTH - 1);
  auto valuevec = events->Values_as_UInt64Array()->value();
  EXPECT_EQ(valuevec->size(), ARRAYLENGTH - 1);

  for (int i = 0; i < ARRAYLENGTH - 1; i++) {
    EXPECT_EQ((*timevec)[i], time[i]);
    EXPECT_EQ((*valuevec)[i], data[i]);
  }

  EXPECT_EQ(events->Name()->str(), "nameless");
  EXPECT_EQ(events->Channel(), CChannelId);
  EXPECT_EQ(events->PacketTimestamp(), CPacketTS);
  EXPECT_EQ(events->TimestampLocation(), CLocation);
  EXPECT_EQ(events->TimeDelta(), CTimeDelta);

}

TEST_F(SENVSerializerTest, AutoDeserialize) {
  SENVSerializerU16 fb{ARRAYLENGTH, "nameless"};
  MockProducer mp;
  auto Produce = [&mp](auto A, auto B) {
    mp.produce(A, B);
  };
  fb.setProducerCallback(Produce);

  for (int i = 0; i < ARRAYLENGTH - 1; i++) {
    auto len = fb.addEvent(time[i], data[i]);
    ASSERT_EQ(len, 0);
    ASSERT_EQ(fb.eventCount(), i + 1);
  }
  EXPECT_EQ(mp.NumberOfCalls, 0);

  auto len = fb.addEvent(time[ARRAYLENGTH - 1], data[ARRAYLENGTH - 1]);
  EXPECT_GT(len, 0);
  EXPECT_EQ(mp.NumberOfCalls, 1);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
