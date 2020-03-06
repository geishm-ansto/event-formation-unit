#include <test/TestBase.h>

// Use (void) to silent unused warnings.
#define AssertMsg(exp, msg) assert(((void)msg, exp))

template <size_t kSlotBytes_, size_t kNumSlots,
          size_t kSlotAlignment = kSlotBytes_, size_t kStartAlignment_ = 16,
          bool kDebug = true>
class FixedSizePool {
public:
  static const size_t kSlotBytes = std::max(kSlotBytes_, kSlotAlignment);
  static const size_t kStartAlignment =
      std::max(kSlotAlignment, kStartAlignment_);

  static const unsigned char kMemDeletedPat = 0xED;
  static const unsigned char kMemAllocatedPat = 0xCD;
  // static const uint32_t kInvalidIndex = 0xFFFFFFFF;

  uint32_t m_NumSlotsUsed;
  uint32_t m_NextFreeSlot[kNumSlots];
  alignas(kStartAlignment) unsigned char m_PoolBytes[kSlotBytes * kNumSlots];

  FixedSizePool() {
    m_NumSlotsUsed = 0;
    for (size_t i = 0; i < kNumSlots; ++i) {
      m_NextFreeSlot[i] = i;
    }

    if (kDebug) {
      memset(m_PoolBytes, kMemDeletedPat, sizeof(m_PoolBytes));
    }
  }

  ~FixedSizePool() {
    AssertMsg(m_NumSlotsUsed == 0, "All slots in pool must be empty");

    if (kDebug) {
      // test m_NextFreeSlot indices are unique
      for (size_t testIndex = 0; testIndex < kNumSlots; ++testIndex) {
        __attribute__((unused)) bool testIndexFound = false;
        for (size_t i = 0; i < kNumSlots; ++i) {
          if (m_NextFreeSlot[i] == testIndex) {
            AssertMsg(!testIndexFound,
                      "Free slots must be unique. Could mean double delete");
            testIndexFound = true;
          }
        }
        AssertMsg(testIndexFound, "All slots must be used");
      }

      for (size_t i = 0; i < sizeof(m_PoolBytes); ++i) {
        AssertMsg(m_PoolBytes[i] == kMemDeletedPat,
                  "Deleted memory must have reference pattern");
      }
    }
  }

  inline void *AllocateOne() {
    AssertMsg(m_NumSlotsUsed < kNumSlots, "Implement fallover to malloc()");
    size_t index = m_NextFreeSlot[m_NumSlotsUsed];
    AssertMsg(index < kNumSlots, "Expect capacity");

    m_NumSlotsUsed++;
    unsigned char *p = m_PoolBytes + (index * kSlotBytes);
    AssertMsg(p + kSlotBytes <= m_PoolBytes + sizeof(m_PoolBytes),
              "Don't go past end of capacity");

    if (kDebug) {
      for (size_t i = 0; i < kSlotBytes; ++i) {
        AssertMsg(p[i] == kMemDeletedPat,
                  "Unused memory must have deletion pattern");
      }
      memset(p, kMemAllocatedPat, kSlotBytes);
    }

    return static_cast<void *>(p);
  }

  inline void Deallocate(void *p) {
    size_t index = ((unsigned char *)p - m_PoolBytes) / kSlotBytes;
    AssertMsg(index < kNumSlots, "Implement fallover to free()");

    AssertMsg(m_NumSlotsUsed > 0, "Pool must have content");
    --m_NumSlotsUsed;

    m_NextFreeSlot[m_NumSlotsUsed] = index;

    if (kDebug) {
      memset(p, kMemDeletedPat, kSlotBytes);
    }
  }
};

//----------------------------------------------------------------------

class FixedSizePoolTest : public TestBase {
public:
};

TEST_F(FixedSizePoolTest, Small_Empty) {
  FixedSizePool<8, 1> pool;

  ASSERT_TRUE(pool.m_NumSlotsUsed == 0);
}

TEST_F(FixedSizePoolTest, Small_1) {
  FixedSizePool<8, 1> pool;

  void *mem = pool.AllocateOne();

  ASSERT_TRUE(pool.m_NumSlotsUsed == 1);
  ASSERT_TRUE(mem >= (void *)pool.m_PoolBytes);
  ASSERT_TRUE(mem < (void *)(pool.m_PoolBytes + sizeof(pool.m_PoolBytes)));

  pool.Deallocate(mem);
}

TEST_F(FixedSizePoolTest, Small_1_NewlyAllocatedPattern) {
  FixedSizePool<8, 1> pool;
  unsigned char *mem = (unsigned char *)pool.AllocateOne();
  for (size_t i = 0; i < pool.kSlotBytes; ++i) {
    ASSERT_TRUE(mem[i] == pool.kMemAllocatedPat);
  }
  pool.Deallocate(mem);
}

TEST_F(FixedSizePoolTest, Small_1_Fail_DoubleFree) {
  using FixedSizePool_t = FixedSizePool<8, 2>;
  ASSERT_DEATH(
      {
        FixedSizePool_t pool;
        void *mem = pool.AllocateOne();
        pool.AllocateOne();
        pool.Deallocate(mem);
        pool.Deallocate(mem);
      },
      "Free slots must be unique. Could mean double delete");
}

TEST_F(FixedSizePoolTest, Small_1_Fail_DeallocateWrong) {
  using FixedSizePool_t = FixedSizePool<8, 1>;
  ASSERT_DEATH(
      {
        FixedSizePool_t pool;
        void *mem = pool.AllocateOne();
        pool.Deallocate(mem);
        pool.Deallocate(mem);
      },
      "Pool must have content");
}

TEST_F(FixedSizePoolTest, Small_1_Fail_NotEmpty) {
  using FixedSizePool_t = FixedSizePool<8, 1>;
  ASSERT_DEATH(
      {
        FixedSizePool_t pool;
        pool.AllocateOne();
      },
      "All slots in pool must be empty");
}

TEST_F(FixedSizePoolTest, Small_1_Fail_DirtyRefPattern) {
  using FixedSizePool_t = FixedSizePool<8, 1>;
  ASSERT_DEATH(
      {
        FixedSizePool_t pool;
        void *mem = pool.AllocateOne();
        pool.Deallocate(mem);
        *(uint32_t *)mem = 0; // dirty the kMemDeleted zone
      },
      "Deleted memory must have reference pattern");
}

TEST_F(FixedSizePoolTest, Large_1) {
  FixedSizePool<8, 1000> pool;

  void *mem = pool.AllocateOne();

  ASSERT_TRUE(pool.m_NumSlotsUsed == 1);
  ASSERT_TRUE(mem >= (void *)pool.m_PoolBytes);
  ASSERT_TRUE(mem < (void *)(pool.m_PoolBytes + sizeof(pool.m_PoolBytes)));

  pool.Deallocate(mem);
}

TEST_F(FixedSizePoolTest, Large_All) {
  FixedSizePool<8, 1000> pool;

  void *allocs[1000];
  for (uint32_t i = 0; i < 1000; ++i) {
    allocs[i] = pool.AllocateOne();

    ASSERT_TRUE(pool.m_NumSlotsUsed == i + 1);
    ASSERT_TRUE(allocs[i] >= (void *)pool.m_PoolBytes);
    ASSERT_TRUE(allocs[i] <
                (void *)(pool.m_PoolBytes + sizeof(pool.m_PoolBytes)));
  }

  for (int i = 0; i < 1000; ++i) {
    pool.Deallocate(allocs[i]);
  }
}

//----------------------------------------------------------------------

TEST_F(FixedSizePoolTest, Align_AtSame) {
  FixedSizePool<8, 2, 8> pool;

  void *mem = pool.AllocateOne();
  ASSERT_EQ(size_t(mem) % 8, 0);

  void *mem2 = pool.AllocateOne();
  ASSERT_EQ(size_t(mem2) % 8, 0);

  ASSERT_TRUE(size_t(mem2) - size_t(mem) >= 8);

  pool.Deallocate(mem);
  pool.Deallocate(mem2);
}

TEST_F(FixedSizePoolTest, Align_At64) {
  FixedSizePool<8, 2, 64> pool;

  void *mem = pool.AllocateOne();
  ASSERT_EQ(size_t(mem) % 64, 0);

  void *mem2 = pool.AllocateOne();
  ASSERT_EQ(size_t(mem2) % 64, 0);

  ASSERT_TRUE(size_t(mem2) - size_t(mem) >= 8);

  pool.Deallocate(mem);
  pool.Deallocate(mem2);
}

TEST_F(FixedSizePoolTest, Align_At2) {
  FixedSizePool<64, 2, 2> pool;

  void *mem = pool.AllocateOne();
  ASSERT_EQ(size_t(mem) % 2, 0);

  void *mem2 = pool.AllocateOne();
  ASSERT_EQ(size_t(mem2) % 2, 0);

  ASSERT_TRUE(size_t(mem2) - size_t(mem) >= 64);

  pool.Deallocate(mem);
  pool.Deallocate(mem2);
}
