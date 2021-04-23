/** Copyright (C) 2016, 2017 European Spallation Source ERIC */

#include "EventBinning.h"
#include <algorithm>
#include <atomic>
#include <omp.h>
#include <test/TestBase.h>

using namespace Ansto;

constexpr int CFineBinBits{10};
constexpr int CFineWidthBits{11};
constexpr int CCoarseBinBits{10};
constexpr uint64_t CCoarseBW = (1 << (CFineWidthBits + CFineBinBits));
constexpr uint64_t CFineBW = (1 << CFineWidthBits);
constexpr uint64_t CFineBins = (1 << CFineBinBits);

static EventBinning binner(CCoarseBinBits, CFineWidthBits, CFineBinBits);

class AnstoEventTest : public TestBase {
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(AnstoEventTest, ConstructInts) {
  std::deque<int> dq;
  for (size_t i = 0; i < 10; i++)
    dq.push_back(i);

  for (size_t i = 0; i < 25'000'000; i++) {
    dq.emplace_back(i);
    dq.front();
    dq.pop_front();
  }
  EXPECT_EQ(10, dq.size());
}

TEST_F(AnstoEventTest, ConstructEvents) {
  std::deque<Readout> dq;
  Readout r{0ULL, 0, 0, 1, 2, 3, 0};
  for (size_t i = 0; i < 10; i++)
    dq.emplace_back(r);
  for (size_t i = 0; i < 25'000'000; i++) {
    dq.emplace_back(r);
    dq.front();
    dq.pop_front();
  }
  EXPECT_EQ(10, dq.size());
}

TEST_F(AnstoEventTest, AtomicLoad) {
  omp_set_num_threads(4);
  uint64_t N = 25'000'000;
  size_t sum = 0;
  std::vector<std::atomic_flag> tags(1024);
  std::vector<std::deque<Readout>> events(1024);
  Readout r{0ULL, 0, 0, 1, 2, 3, 0};
#pragma omp parallel for default(shared) reduction(+ : sum)
  for (uint64_t i = 0; i < N; i++) {
    size_t ix = i % 1024;
    while (tags[ix].test_and_set(std::memory_order_acquire))
      ;
    r.timestamp = i;
    events[ix].emplace_back(r);
    sum += 1;
    events[ix].pop_front();
    tags[ix].clear(std::memory_order_acquire);
  }
  EXPECT_EQ(N, sum);
}

class AnstoEventBinningTest : public TestBase {
protected:
  const size_t N{1'000'000};
  std::vector<Readout> Readouts;
  void SetUp() override {

    binner.resetBins(0);
    binner.clearCoarseBins();
    binner.clearFineBins();

    Readouts.resize(N);
    uint64_t CWindow = ((1 << (CCoarseBinBits)) - 1) * CCoarseBW;
    srand(1);
    for (size_t i = 0; i < N; ++i) {
      auto &r = Readouts[i];
      r.event_type = Neutron;
      r.x_posn = rand() % 190;
      r.y_posn = rand() % 190;
      r.timestamp = rand() % CWindow;
      r.weight = 100 + rand() % 100;
    }
    // include 2 non-netron events
    Readouts[1].event_type = FrameStart;
    Readouts[10].event_type = FrameAuxStart;
  }
  void TearDown() override {}
};

TEST_F(AnstoEventBinningTest, Constructor) {
  ////EventBinning binner(CCoarseBinBits, CFineWidthBits, CFineBinBits);
  EXPECT_EQ((1ULL << (CCoarseBinBits + CFineWidthBits + CFineBinBits)),
            binner.eventWindow());
  EXPECT_EQ(0, binner.getBaseTime());
  EXPECT_EQ(true, binner.empty());
  EXPECT_EQ(false, binner.validEventTime(binner.eventWindow()));
  EXPECT_EQ(false, binner.validEventTime(binner.eventWindow() + 1));
  EXPECT_EQ(true, binner.validEventTime(binner.eventWindow() - CCoarseBW - 1));
  EXPECT_EQ(3 * (1 << (CFineWidthBits + CFineBinBits)), binner.getBinTime(3));
}

TEST_F(AnstoEventBinningTest, InsertOneEvent) {
  uint16_t X = 1, Y = 2;
  uint64_t T = CCoarseBW * 3 + 123;
  std::vector<Readout> events{{T, 0, 0, 0, X, Y, 0}};
  ////EventBinning binner(CCoarseBinBits, CFineWidthBits, CFineBinBits);
  auto sum = binner.insert(events);
  EXPECT_EQ(1, sum);
  EXPECT_EQ(false, binner.empty());
  EXPECT_EQ(CCoarseBW * 4, binner.timeSpan());
  sum = binner.clearFineBins();
  EXPECT_EQ(0, sum);
  sum = binner.clearCoarseBins();
  EXPECT_EQ(1, sum);
  binner.insert(events);
  sum = 0;
  sum += binner.incrementBase();
  sum += binner.incrementBase();
  sum += binner.incrementBase();
  EXPECT_EQ(0, sum);
  sum += binner.incrementBase();
  EXPECT_EQ(1, sum);
}

TEST_F(AnstoEventBinningTest, ResetBins) {
  // small shift in basetime
  uint64_t rawtime = 12 * CCoarseBW + 7;
  uint64_t basetime = 12 * CCoarseBW;
  ////EventBinning binner(CCoarseBinBits, CFineWidthBits, CFineBinBits);
  EXPECT_EQ((1ULL << (CCoarseBinBits + CFineWidthBits + CFineBinBits)),
            binner.eventWindow());
  EXPECT_EQ(0, binner.getBaseTime());
  binner.resetBins(rawtime);
  EXPECT_EQ(basetime, binner.getBaseTime());

  rawtime = 1234UL * (1 << 25) + 567;
  basetime = 1234UL * (1 << 25);
  binner.resetBins(rawtime);
  EXPECT_EQ(basetime, binner.getBaseTime());
  EXPECT_EQ(true, binner.empty());
  EXPECT_EQ(false, binner.validEventTime(binner.eventWindow() + basetime));
  EXPECT_EQ(false, binner.validEventTime(binner.eventWindow() + basetime + 1));
  EXPECT_EQ(true, binner.validEventTime(binner.eventWindow() + basetime -
                                        CCoarseBW - 1));
  EXPECT_EQ((3 * CCoarseBW + basetime), binner.getBinTime(3));
}

TEST_F(AnstoEventBinningTest, MapFineBins) {
  std::vector<Readout> events;
  events.resize(CFineBins);
  for (size_t i = 0; i < CFineBins; i++) {
    events[i].timestamp = i * CFineBW;
  }
  // EventBinning binner(CCoarseBinBits, CFineWidthBits, CFineBinBits);
  auto valid = binner.insert(events);
  EXPECT_EQ(CFineBins, valid);
  auto mapped = binner.mapToFineBins();
  EXPECT_EQ(CFineBins, mapped);
  for (size_t i = 0; i < CFineBins; i++) {
    auto &bin = binner.getFineBin(i);
    EXPECT_EQ(1, bin.size());
  }
  auto binned = binner.clearFineBins();
  EXPECT_EQ(CFineBins, binned);
}

TEST_F(AnstoEventBinningTest, Index25MEvents) {
  const size_t N = 25;
  // EventBinning binner(CCoarseBinBits, CFineWidthBits, CFineBinBits);
  size_t binned = 0;
  for (size_t loop = 0; loop < N; loop++) {
    for (auto &r : Readouts) {
      if (binner.validEventTime(r.timestamp)) {
        binner.coarseIndex(r.timestamp);
        binned++;
      }
    }
  }
  EXPECT_EQ((N * Readouts.size()), binned);
}

TEST_F(AnstoEventBinningTest, MaxBinSize50MEvents) {
  const size_t N = 50;
  // EventBinning binner(CCoarseBinBits, CFineWidthBits, CFineBinBits);
  size_t points = 50 * Readouts.size();
  size_t nominal = points / (binner.getNumCoarseBins() - 1);
  size_t binned = 0;
  for (size_t loop = 0; loop < N; loop++) {
    binned += binner.insert(Readouts);
  }
  EXPECT_EQ((N * Readouts.size()), binned);
  double sumSqr{0}, sum{0};
  size_t maxSz{0}, minSz{1'000'000};
  auto M = binner.getNumCoarseBins();
  for (size_t i = 0; i < M - 1; i++) {
    auto n = binner.getCoarseBin(i).size();
    maxSz = std::max(maxSz, n);
    minSz = std::min(minSz, n);
    sum += n;
    sumSqr += n * n;
  }
  double mu = sum / M;
  double sigma = sqrt((sumSqr - M * mu * mu) / M);
  EXPECT_LT(mu, 1.2 * nominal);
  EXPECT_GT(mu, 0.8 * nominal);
  EXPECT_LT(sigma, 0.5 * points / M);
  EXPECT_EQ((1 << CCoarseBinBits), M);
  EXPECT_LT(maxSz, 2 * nominal);
  EXPECT_GT(minSz, (nominal / 2));
}

TEST_F(AnstoEventBinningTest, InsertClear25MEvents) {
  const size_t N = 25;
  // EventBinning binner(CCoarseBinBits, CFineWidthBits, CFineBinBits);
  size_t valid = 0, binned = 0;
  for (size_t loop = 0; loop < N; loop++) {
    valid += binner.insert(Readouts);
    binned += binner.clearCoarseBins();
  }
  EXPECT_EQ(valid, binned);
  EXPECT_EQ((N * Readouts.size()), valid);
}

TEST_F(AnstoEventBinningTest, Insert25MEvents) {
  const size_t N = 25;
  // EventBinning binner(CCoarseBinBits, CFineWidthBits, CFineBinBits);
  size_t valid = 0, binned = 0;
  uint64_t blockSz = binner.eventWindow() - CCoarseBW;
  for (size_t loop = 0; loop < N; loop++) {
    valid += binner.insert(Readouts);
    while (!binner.empty()) {
      binned += binner.incrementBase();
    }
    // shift the time by the window
    for (auto &r : Readouts)
      r.timestamp += blockSz;
  }
  EXPECT_EQ(valid, binned);
  EXPECT_EQ((N * Readouts.size()), valid);
}

TEST_F(AnstoEventBinningTest, InsertFine25MEvents) {
  const size_t N = 25;
  // EventBinning binner(CCoarseBinBits, CFineWidthBits, CFineBinBits);
  size_t valid = 0, binned = 0;
  uint64_t blockSz = binner.eventWindow() - CCoarseBW;
  for (size_t loop = 0; loop < N; loop++) {
    valid += binner.insert(Readouts);
    while (!binner.empty()) {
      binned += binner.mapToFineBins();
      binner.clearFineBins();
      binner.incrementBase();
    }
    // shift the time by the window
    for (auto &r : Readouts)
      r.timestamp += blockSz;
  }
  EXPECT_EQ(valid, binned);
  EXPECT_EQ((N * Readouts.size()), valid);
}

TEST_F(AnstoEventBinningTest, DumpEvents) {
  // EventBinning binner(CCoarseBinBits, CFineWidthBits, CFineBinBits);
  size_t binned{0}, dumped{0};
  binned += binner.insert(Readouts);
  const int N{10};
  uint64_t blockSz = binner.eventWindow() / N;
  std::vector<Readout> output;
  for (int i = 0; i < (N + 2); i++) {
    dumped += binner.dumpBlock(blockSz, output);
  }
  EXPECT_EQ(binner.timeSpan(), 0);
  EXPECT_EQ(binned, Readouts.size());
  EXPECT_EQ(dumped, binned);
  EXPECT_EQ(output.size(), dumped);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
