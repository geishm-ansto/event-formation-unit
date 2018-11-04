/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
//===----------------------------------------------------------------------===//

#include <common/clustering/OverlapMatcher.h>

#include <cmath>
#include <algorithm>

void OverlapMatcher::match(bool flush) {
  unmatched_clusters_.sort([](const Cluster &c1, const Cluster &c2) {
    return c1.time_end() < c2.time_end();
  });

  Event evt;
  while (!unmatched_clusters_.empty()) {

    auto cluster = unmatched_clusters_.begin();

    if (!flush && !ready_to_be_matched(*cluster))
      break;

    if (!evt.empty() && !evt.time_overlap(*cluster)) {
      stash_event(evt);
      evt.clear();
    }

    evt.merge(*cluster);
    unmatched_clusters_.pop_front();
  }

  // If anything is left, stash it
  // \todo maybe only on flush? otherwise return to queue?
  if (!evt.empty()) {
    stash_event(evt);
  }
}
