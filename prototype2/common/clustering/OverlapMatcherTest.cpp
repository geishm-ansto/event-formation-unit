/** Copyright (C) 2017 European Spallation Source ERIC */

#include <common/clustering/OverlapMatcher.h>
#include <test/TestBase.h>

class OverlapMatcherTest : public TestBase {
protected:
  ClusterContainer x, y;
  OverlapMatcher matcher{600, 0, 1};

  Cluster mock_cluster(uint8_t plane, uint16_t strip_start, uint16_t strip_end,
                       uint64_t time_start, uint64_t time_end) {
    Cluster ret;
    Hit e;
    e.plane = plane;
    e.weight = 1;
    double time_step = (time_end - time_start) / 10.0;
    for (e.time = time_start; e.time <= time_end; e.time += time_step)
      for (e.coordinate = strip_start; e.coordinate <= strip_end; ++e.coordinate)
        ret.insert(e);
    e.time = time_end;
    ret.insert(e);
    return ret;
  }
};

TEST_F(OverlapMatcherTest, OneX) {
  x.push_back(mock_cluster(0, 0, 10, 0, 200));
  matcher.insert(0, x);
  matcher.match(true);
  ASSERT_EQ(matcher.stats_cluster_count, 1);
  ASSERT_EQ(matcher.matched_clusters.size(), 1);
  EXPECT_EQ(matcher.matched_clusters.front().time_span(), 201);
  EXPECT_EQ(matcher.matched_clusters.front().c1.hit_count(), 122);
  EXPECT_EQ(matcher.matched_clusters.front().c2.hit_count(), 0);
}

TEST_F(OverlapMatcherTest, OneY) {
  y.push_back(mock_cluster(1, 0, 10, 0, 200));
  matcher.insert(1, y);
  matcher.match(true);
  ASSERT_EQ(matcher.stats_cluster_count, 1);
  ASSERT_EQ(matcher.matched_clusters.size(), 1);
  EXPECT_EQ(matcher.matched_clusters.front().time_span(), 201);
  EXPECT_EQ(matcher.matched_clusters.front().c1.hit_count(), 0);
  EXPECT_EQ(matcher.matched_clusters.front().c2.hit_count(), 122);
}

TEST_F(OverlapMatcherTest, TwoX) {
  x.push_back(mock_cluster(0, 0, 10, 0, 200));
  x.push_back(mock_cluster(0, 0, 10, 500, 700));
  matcher.insert(0, x);
  matcher.match(true);
  ASSERT_EQ(matcher.stats_cluster_count, 2);
  ASSERT_EQ(matcher.matched_clusters.size(), 2);
  EXPECT_EQ(matcher.matched_clusters.front().time_span(), 201);
  EXPECT_EQ(matcher.matched_clusters.back().time_span(), 201);
  EXPECT_EQ(matcher.matched_clusters.front().c1.hit_count(), 122);
  EXPECT_EQ(matcher.matched_clusters.front().c2.hit_count(), 0);
  EXPECT_EQ(matcher.matched_clusters.back().c1.hit_count(), 122);
  EXPECT_EQ(matcher.matched_clusters.back().c2.hit_count(), 0);
}

TEST_F(OverlapMatcherTest, TwoY) {
  y.push_back(mock_cluster(1, 0, 10, 0, 200));
  y.push_back(mock_cluster(1, 0, 10, 500, 700));
  matcher.insert(1, y);
  matcher.match(true);
  ASSERT_EQ(matcher.stats_cluster_count, 2);
  ASSERT_EQ(matcher.matched_clusters.size(), 2);
  EXPECT_EQ(matcher.matched_clusters.front().time_span(), 201);
  EXPECT_EQ(matcher.matched_clusters.back().time_span(), 201);
  EXPECT_EQ(matcher.matched_clusters.front().c1.hit_count(), 0);
  EXPECT_EQ(matcher.matched_clusters.front().c2.hit_count(), 122);
  EXPECT_EQ(matcher.matched_clusters.back().c1.hit_count(), 0);
  EXPECT_EQ(matcher.matched_clusters.back().c2.hit_count(), 122);
}

TEST_F(OverlapMatcherTest, OneXOneY) {
  x.push_back(mock_cluster(0, 0, 10, 0, 200));
  y.push_back(mock_cluster(1, 0, 10, 500, 700));
  matcher.insert(0, x);
  matcher.insert(1, y);
  matcher.match(true);
  ASSERT_EQ(matcher.stats_cluster_count, 2);
  ASSERT_EQ(matcher.matched_clusters.size(), 2);
  EXPECT_EQ(matcher.matched_clusters.front().time_span(), 201);
  EXPECT_EQ(matcher.matched_clusters.back().time_span(), 201);
  EXPECT_EQ(matcher.matched_clusters.front().c1.hit_count(), 122);
  EXPECT_EQ(matcher.matched_clusters.front().c2.hit_count(), 0);
  EXPECT_EQ(matcher.matched_clusters.back().c1.hit_count(), 0);
  EXPECT_EQ(matcher.matched_clusters.back().c2.hit_count(), 122);
}

TEST_F(OverlapMatcherTest, OneXY) {
  x.push_back(mock_cluster(0, 0, 10, 0, 200));
  y.push_back(mock_cluster(1, 0, 10, 0, 200));
  matcher.insert(0, x);
  matcher.insert(1, y);
  matcher.match(true);
  ASSERT_EQ(matcher.stats_cluster_count, 1);
  ASSERT_EQ(matcher.matched_clusters.size(), 1);
  EXPECT_EQ(matcher.matched_clusters.front().time_span(), 201);
  EXPECT_EQ(matcher.matched_clusters.front().c1.hit_count(), 122);
  EXPECT_EQ(matcher.matched_clusters.front().c2.hit_count(), 122);
}

TEST_F(OverlapMatcherTest, TwoXY) {
  x.push_back(mock_cluster(0, 0, 10, 0, 200));
  y.push_back(mock_cluster(1, 0, 10, 1, 300));
  x.push_back(mock_cluster(0, 0, 10, 600, 800));
  y.push_back(mock_cluster(1, 0, 10, 650, 850));
  matcher.insert(0, x);
  matcher.insert(1, y);
  matcher.match(true);
  ASSERT_EQ(matcher.stats_cluster_count, 2);
  ASSERT_EQ(matcher.matched_clusters.size(), 2);
  EXPECT_EQ(matcher.matched_clusters.front().time_span(), 301);
  EXPECT_EQ(matcher.matched_clusters.back().time_span(), 251);
  EXPECT_EQ(matcher.matched_clusters.front().c1.hit_count(), 122);
  EXPECT_EQ(matcher.matched_clusters.front().c2.hit_count(), 122);
  EXPECT_EQ(matcher.matched_clusters.back().c1.hit_count(), 122);
  EXPECT_EQ(matcher.matched_clusters.back().c2.hit_count(), 122);
}

TEST_F(OverlapMatcherTest, JustIntside) {
  x.push_back(mock_cluster(0, 0, 10, 0, 200));
  y.push_back(mock_cluster(1, 0, 10, 200, 400));
  matcher.insert(0, x);
  matcher.insert(1, y);
  matcher.match(true);
  ASSERT_EQ(matcher.matched_clusters.size(), 1);
}

TEST_F(OverlapMatcherTest, JustOutside) {
  x.push_back(mock_cluster(0, 0, 10, 0, 199));
  y.push_back(mock_cluster(1, 0, 10, 200, 401));
  matcher.insert(0, x);
  matcher.insert(1, y);
  matcher.match(true);
  ASSERT_EQ(matcher.matched_clusters.size(), 2);
}

TEST_F(OverlapMatcherTest, DontForce) {
  x.push_back(mock_cluster(0, 0, 10, 0, 200));
  y.push_back(mock_cluster(1, 0, 10, 200, 401));
  matcher.insert(0, x);
  matcher.insert(1, y);
  matcher.match(false);
  ASSERT_EQ(matcher.matched_clusters.size(), 0);

  x.push_back(mock_cluster(0, 0, 10, 800, 1000));
  matcher.insert(0, x);
  matcher.match(false);
  ASSERT_EQ(matcher.matched_clusters.size(), 0);

  y.push_back(mock_cluster(1, 0, 10, 900, 1000));
  matcher.insert(1, y);
  matcher.match(false);
  ASSERT_EQ(matcher.matched_clusters.size(), 0);

  x.push_back(mock_cluster(0, 0, 10, 1000, 1200));
  matcher.insert(0, x);
  matcher.match(false);
  ASSERT_EQ(matcher.matched_clusters.size(), 1);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
