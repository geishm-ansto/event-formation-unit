/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief cluster class for detection events.
//===----------------------------------------------------------------------===//

#pragma once

#include <vector>

#include "Readout.h"

namespace Ansto {

/// captures the min max range for a type
template <typename T> struct Range {
  void extend(T x) {
    lo = std::min(lo, x);
    hi = std::max(hi, x);
  }
  void merge(const Range<T> &x) {
    lo = std::min(lo, x.lo);
    hi = std::max(hi, x.hi);
  }
  T lo, hi;
};

/// progressively capture sums to recover mean and variance
struct Stats {
  Stats(void) = default;
  Stats(double w, double x, double y)
      : wgt(w), wgtX(w * x), wgtY{w * y}, wgtX2(w * x * x),
        wgtY2(w * y * y) {}

  void clear() { wgt = wgtX = wgtY = wgtX2 = wgtY2 = 0.0; }
  void add(double w, double x, double y) {
    wgt += w;
    wgtX += w * x;
    wgtY += w * y;
    wgtX2 += w * x * x;
    wgtY2 += w * y * y;
  }

  void add(const Stats &stB) {
    wgt += stB.wgt;
    wgtX += stB.wgtX;
    wgtY += stB.wgtY;
    wgtX2 += stB.wgtX2;
    wgtY2 += stB.wgtY2;
  }

  double mean() const { return wgtX / wgt; }
  double variance() const { return wgtX2 - wgtX * wgtX / wgt; }

  double wgt{0.};
  double wgtX{0.};
  double wgtY{0.};
  double wgtX2{0.};
  double wgtY2{0.};
};

/// single cluster that captures hits, range and stats
struct Cluster {
  Cluster();

  void operator=(const Cluster &a);

  void addEvent(const Readout &evt);

  void addCluster(const Cluster &a);

  void newCluster(const Readout &evt);

  void clear();

  /// reduces the cluster to a single point
  Readout asReadout() const;

  std::vector<Readout> hits;
  Range<uint64_t> delT;
  Range<int16_t> delX;
  Range<int16_t> delY;

  Stats moment;  ///< amplitude based weighting
  Stats moment2; ///< support for energy weighthing, is this necessary?
};
} // namespace Ansto
