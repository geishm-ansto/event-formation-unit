/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file AbstractMatcher.cpp
/// \brief AbstractMatcher class implementation
///
//===----------------------------------------------------------------------===//

#include <common/clustering/AbstractMatcher.h>
#include <common/Trace.h>

// #undef TRC_LEVEL
// #define TRC_LEVEL TRC_L_DEB

AbstractMatcher::AbstractMatcher(uint64_t latency, uint8_t plane1, uint8_t plane2, uint8_t pulse_plane)
    : latency_(latency), plane1_(plane1), plane2_(plane2), pulse_plane_(pulse_plane) {}

void AbstractMatcher::insert(const Cluster &cluster) {
  if (cluster.plane() == plane1_) {
    latest_x_ = std::max(latest_x_, cluster.time_start());
  } else if (cluster.plane() == plane2_) {
    latest_y_ = std::max(latest_y_, cluster.time_start());
  } else {
    stats_rejected_clusters++;
    return;
  }
  unmatched_clusters_.push_back(cluster);
}

void AbstractMatcher::insert(const ClusterContainer &clusters) {
  for (auto &cluster : clusters)
    insert(cluster);
}

void AbstractMatcher::insert(uint8_t plane, ClusterContainer &clusters) {
  if (clusters.empty()) {
    return;
  }
  if (plane == plane1_) {
    latest_x_ = std::max(latest_x_, clusters.back().time_start());
  } else if (plane == plane2_) {
    latest_y_ = std::max(latest_y_, clusters.back().time_start());
  } else {
    stats_rejected_clusters++;
    return;
  }
  unmatched_clusters_.splice(unmatched_clusters_.end(), clusters);
}

void AbstractMatcher::insert_pulses(HitVector &pulses) {
  if (pulses.empty())
    return;
  tracking_pulses_ = true;
  for (auto &h : pulses) {
    h.plane = pulse_plane_;
    Cluster c;
    c.insert(h);
    unmatched_clusters_.push_back(c);
    latest_pulse_ = std::max(latest_pulse_, h.time);
  }
  pulses.clear();
}

void AbstractMatcher::stash_event(Event &event) {
  matched_events.emplace_back(std::move(event));
  stats_event_count++;
}

bool AbstractMatcher::ready_to_be_matched(const Cluster &cluster) const {
  XTRACE(CLUSTER, DEB,
      "latest_x %u, latest_y %u, cl time end %u", latest_x_, latest_y_, cluster.time_end());
  // \todo print info on pulses
  auto latest = std::min(latest_x_, latest_y_);
  if (tracking_pulses_)
    latest = std::min(latest, latest_pulse_);
  return (latest > cluster.time_end()) &&
      ((latest - cluster.time_end()) > latency_);
}

