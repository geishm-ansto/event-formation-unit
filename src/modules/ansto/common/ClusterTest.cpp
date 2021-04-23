/** Copyright (C) 2016, 2017 European Spallation Source ERIC */

#include "Clusterer.h"
#include <algorithm>
#include <atomic>
#include <omp.h>
#include <test/TestBase.h>

using namespace Ansto;

auto MapToCluster = [](Cluster &cls, std::vector<Readout> events) {
  for (auto &e : events)
    cls.addEvent(e);
};

class AnstoClusterTest : public TestBase {
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(AnstoClusterTest, CheckStats) {
  // fill up the course bins with the data
  std::vector<Readout> events{
      {1000, 0, 0, 101, 108, 1, 0}, {1500, 0, 0, 99, 92, 1, 0},
      {2000, 0, 0, 102, 100, 1, 0}, {2500, 0, 0, 98, 100, 1, 0},
      {3000, 0, 0, 102, 100, 1, 0}, {3500, 0, 0, 98, 100, 1, 0},
      {4000, 0, 0, 103, 100, 1, 0}, {4500, 0, 0, 97, 100, 1, 0}};
  auto cls = Cluster();
  MapToCluster(cls, events);

  EXPECT_EQ(cls.hits.size(), 8UL);
  EXPECT_EQ(cls.moment.mean(), 100);
  EXPECT_EQ(cls.moment.variance(), 36);
  EXPECT_EQ(cls.moment2.mean(), 100);
  EXPECT_EQ(cls.moment2.variance(), 36);
}

TEST_F(AnstoClusterTest, CheckMerge) {
  // fill up the course bins with the data
  std::vector<Readout> eventsA{{1000, 0, 0, 101, 108, 1, 0},
                               {1500, 0, 0, 99, 92, 1, 0},
                               {2000, 0, 0, 102, 100, 1, 0},
                               {2500, 0, 0, 98, 100, 1, 0}};
  std::vector<Readout> eventsB{{3000, 0, 0, 102, 100, 1, 0},
                               {3500, 0, 0, 98, 100, 1, 0},
                               {4000, 0, 0, 103, 100, 1, 0},
                               {4500, 0, 0, 97, 100, 1, 0}};

  auto clsA = Cluster();
  MapToCluster(clsA, eventsA);
  auto clsB = Cluster();
  MapToCluster(clsB, eventsB);
  clsB = clsA;
  EXPECT_EQ(clsA.moment.mean(), clsB.moment.mean());
  EXPECT_EQ(clsA.moment2.mean(), clsB.moment2.mean());
  clsB.clear();
  MapToCluster(clsB, eventsB);
  clsA.addCluster(clsB);

  EXPECT_EQ(clsA.hits.size(), 8UL);
  EXPECT_EQ(clsA.moment.mean(), 100);
  EXPECT_EQ(clsA.moment.variance(), 36);
  EXPECT_EQ(clsA.moment2.mean(), 100);
  EXPECT_EQ(clsA.moment2.variance(), 36);
}

class AnstoClusterManagerTest : public TestBase {
public:
  void SetUp() override {
    clusterer = new ClusterManager(1000, 100, 2, 2, 1000);
  }
  void TearDown() override {}

  ClusterManager *clusterer;
};

TEST_F(AnstoClusterManagerTest, CheckLimits) {
  uint64_t MaxSpan{10'000};
  uint64_t MaxDT{300};
  uint16_t MaxDX{5};
  uint16_t MaxDY{10};
  clusterer->setLimits(MaxSpan, MaxDT, MaxDX, MaxDY);
  Readout first{10'000'000ULL, 0, 0, 100, 200, 50, 0};

  clusterer->addEvent(first);
  EXPECT_EQ(clusterer->numActive(), 1);

  Readout evt = first;
  evt.timestamp += MaxDT;
  evt.x_posn += MaxDX / 2;
  evt.y_posn += MaxDY / 2;
  clusterer->addEvent(evt);
  EXPECT_EQ(clusterer->numActive(), 1);
  evt.x_posn = first.x_posn + MaxDX;
  evt.y_posn = first.y_posn;
  clusterer->addEvent(evt);
  EXPECT_EQ(clusterer->numActive(), 1);
  evt.x_posn = first.x_posn;
  evt.y_posn = first.y_posn + MaxDY;
  clusterer->addEvent(evt);
  EXPECT_EQ(clusterer->numActive(), 1);

  evt.timestamp = first.timestamp + MaxSpan + 1;
  clusterer->addEvent(evt);
  EXPECT_EQ(clusterer->numActive(), 2);
  evt.x_posn += MaxDX + 1;
  clusterer->addEvent(evt);
  EXPECT_EQ(clusterer->numActive(), 3);
  evt.y_posn += MaxDY + 1;
  clusterer->addEvent(evt);
  EXPECT_EQ(clusterer->numActive(), 4);
}

TEST_F(AnstoClusterManagerTest, CheckStreak) {
  uint64_t MaxSpan{900};
  uint64_t MaxDT{100};
  uint16_t MaxDX{2};
  uint16_t MaxDY{2};
  clusterer->setLimits(MaxSpan, MaxDT, MaxDX, MaxDY);
  std::vector<Readout> events;
  for (uint16_t i = 0; i < 10; i++) {
    events.emplace_back(Readout{1'000 + i * MaxDT, 0, 0,
                                (uint16_t)(i * MaxDX / 2),
                                (uint16_t)(i * MaxDY / 2), 50, 0});
  }
  for (auto &e : events)
    clusterer->addEvent(e);
  EXPECT_EQ(clusterer->numActive(), 1);
  clusterer->clearActive();
  std::vector<int> order{0, 9, 5, 6, 1, 8, 2, 7, 4, 3};
  for (auto i : order) {
    auto &e = events[i];
    clusterer->addEvent(e);
  }
  EXPECT_EQ(clusterer->numActive(), 1);

  // add an event outside the span window and confirm 2 clusters are created
  events.emplace_back(Readout{1'000 + 10 * MaxDT, 0, 0, (uint16_t)(5 * MaxDX),
                              (uint16_t)(5 * MaxDY), 50, 0});
  order.push_back(10);
  clusterer->clearActive();
  for (auto &e : events)
    clusterer->addEvent(e);
  EXPECT_EQ(clusterer->numActive(), 2);
  clusterer->clearActive();
  for (auto i : order) {
    auto &e = events[i];
    clusterer->addEvent(e);
  }
  EXPECT_EQ(clusterer->numActive(), 2);
}

TEST_F(AnstoClusterManagerTest, ClosingActiveClusters) {
  auto capacity = clusterer->capacity();
  // Confirm events that are outside the max span
  uint64_t MaxSpan{100};
  uint64_t MaxDT{100};
  uint16_t MaxDX{2};
  uint16_t MaxDY{2};
  clusterer->setLimits(MaxSpan, MaxDT, MaxDX, MaxDY);
  std::vector<Readout> events;
  for (uint16_t i = 0; i < 10; i++) {
    events.emplace_back(Readout{1'000 + i * MaxDT, 0, 0, (uint16_t)(i * MaxDX),
                                (uint16_t)(i * MaxDY), 50, 0});
  }
  for (auto &e : events)
    clusterer->addEvent(e);
  EXPECT_EQ(clusterer->numActive(), 10);
  EXPECT_EQ(clusterer->capacity(), capacity - 10);

  clusterer->processActive(1'001, 1'000 + 5 * MaxDT);
  EXPECT_EQ(clusterer->numActive(), 5);
  EXPECT_EQ(clusterer->numOutput(), 4); // first is returned to free
  EXPECT_EQ(clusterer->capacity(), capacity - 9);
  clusterer->clearActive();
  clusterer->clearOutput();
  EXPECT_EQ(clusterer->capacity(), capacity);
}

TEST_F(AnstoClusterManagerTest, PerformanceTest) {
  uint64_t MaxSpan{900};
  uint64_t MaxDT{100};
  uint16_t MaxDX{2};
  uint16_t MaxDY{2};
  clusterer->setLimits(MaxSpan, MaxDT, MaxDX, MaxDY);
  std::deque<Readout> events;
  int N = 500'000;
  int M = 10;
  // total events is N * M * len(order)
  std::vector<uint16_t> order{0, 4, 1, 3, 2};
  int cnt{0};
  size_t sum{0};
  Readout rba{0, 0, 0, 0, 0, 50, 0};
  for (int i = 0; i < N; i++) {
    uint64_t baseT;
    for (int j = 0; j < M; j++) {
      cnt++;
      baseT = i * 1000 + j * 100;
      uint16_t baseX = (cnt * 5) % 997;
      uint16_t baseY = (cnt * 3) % 997;

      // event sequence defines a valid streak
      for (auto k : order) {
        rba.timestamp = baseT + k * MaxDT;
        rba.x_posn = (uint16_t)(baseX + k * MaxDX / 2);
        rba.y_posn = (uint16_t)(baseY + k * MaxDY / 2);
        events.emplace_back(rba);
      }
    }
    while (!events.empty()) {
      auto &e = events.front();
      clusterer->addEvent(e);
      events.pop_front();
    }
    clusterer->processActive(0, baseT);
    sum += clusterer->clearOutput();
  }
  EXPECT_EQ(sum, N * M - 1);
}

TEST_F(AnstoClusterManagerTest, ParallelClusterTest) {
  uint64_t MaxSpan{900};
  uint64_t MaxDT{100};
  uint16_t MaxDX{2};
  uint16_t MaxDY{2};
  clusterer->setLimits(MaxSpan, MaxDT, MaxDX, MaxDY);
  uint64_t NeutronsPerBin = {5};
  uint64_t FineBinWidth = (1 << 10);
  uint64_t NumFinBins = (1 << 10);
  uint64_t NumCoarseBins = (1 << 10);
  uint64_t CoarseBinWidth = NumFinBins * FineBinWidth;
  std::vector<Bucket> FineBins(NumFinBins);
  for (auto &bin : FineBins)
    bin.reserve(100);
  std::vector<Readout> outputs;

  int numThreads{4};
  ParallelClusterer clusterer(numThreads, MaxSpan, MaxDT, MaxDX, MaxDY, 1000);
  std::vector<uint16_t> order{0, 4, 1, 3, 2};
  int cnt{0};
  size_t sum{0};
  Readout rba{0, 0, 0, 0, 0, 50, 0};
  for (uint64_t i = 0; i < NumCoarseBins; i++) {
    uint64_t baseT = i * CoarseBinWidth;
    for (uint64_t j = 0; j < NumFinBins; j++) {
      for (uint64_t k = 0; k < NeutronsPerBin; k++) {
        cnt++;
        uint64_t timestamp = baseT + j * FineBinWidth + k * 5;
        uint16_t baseX = (cnt * 5) % 997;
        uint16_t baseY = (cnt * 3) % 997;

        // event sequence defines a valid streak
        for (auto k : order) {
          rba.timestamp = timestamp + k * MaxDT;
          rba.x_posn = (uint16_t)(baseX + k * MaxDX / 2);
          rba.y_posn = (uint16_t)(baseY + k * MaxDY / 2);
          FineBins[j].emplace_back(rba);
        }
      }
    }
    sum += clusterer.ProcessEvents(baseT, FineBinWidth, FineBins, outputs);
    for (auto &bin : FineBins)
      bin.clear();
  }
  EXPECT_EQ(sum, outputs.size());
  EXPECT_EQ(sum, (NumCoarseBins * NumFinBins - 1) * NeutronsPerBin);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
