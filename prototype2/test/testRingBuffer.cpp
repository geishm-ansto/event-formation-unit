/** Copyright (C) 2016 European Spallation Source */

#include "TestBase.h"
#include <RingBuffer.h>

using namespace std;

class RingBufferTest : public TestBase {
protected:
  virtual void SetUp() {}
  virtual void TearDown() {}
};

/** Test cases below */
TEST_F(RingBufferTest, Constructor) {
  RingBuffer buf(1000, 100);
  ASSERT_EQ(buf.getelems(), 100);
  ASSERT_EQ(buf.getsize(), 1000);
}

TEST_F(RingBufferTest, CircularWrap) {
  int N = 997;
  int size = 9000;
  RingBuffer buf(size, N);

  struct RingBuffer::Data *first = buf.getdatastruct();
  ASSERT_EQ(buf.getelems(), N);
  ASSERT_EQ(buf.getsize(), size);

  for (int i = 0; i < N; i++) {
    int *ip = (int *)buf.getdatastruct()->buffer;
    ASSERT_NE(ip, nullptr);
    ASSERT_EQ(i, buf.getindex());
    *ip = N + i;
    buf.nextbuffer();
  }
  ASSERT_EQ(0, buf.getindex());

  for (int i = 0; i < N; i++) {
    int *ip = (int *)buf.getdatastruct()->buffer;
    ASSERT_EQ(*ip, N + i);
    buf.nextbuffer();
  }
  ASSERT_EQ(first, buf.getdatastruct());
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}