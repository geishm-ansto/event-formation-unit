/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief class for mapping neutron events to timebins.
//===----------------------------------------------------------------------===//

#include <ansto/common/EventBinning.h>
#include <atomic>
#include <common/Log.h>
#include <common/Trace.h>
#include <omp.h>

using namespace Ansto;

EventBinning::EventBinning(int coarseBinBits, int fineWidthBits,
                           int fineBinBits)
    : BaseIx(0), EndIx(0), BaseTime(0), FineWidthBits(fineWidthBits),
      CoarseWidthBits(fineBinBits + fineWidthBits),
      CoarseBinWidth(1 << (fineBinBits + fineWidthBits)),

      FineBinWidth(1 << fineWidthBits), NumCoarseBins(1 << coarseBinBits),
      NumFineBins(1 << fineBinBits) {

  FineBins.resize(NumFineBins);
  for (auto &bin : FineBins)
    bin.reserve(128);
  CoarseBins.resize(NumCoarseBins);
  for (auto &bin : CoarseBins)
    bin.reserve(20'000);
}

size_t EventBinning::insert(const std::vector<Readout> &readouts) {
  size_t N = readouts.size();
  size_t valid = 0;
  uint64_t maxt = BaseTime;
  for (size_t i = 0; i < N; i++) {
    const auto &r = readouts[i];
    if (validEventTime(r.timestamp)) {
      auto ix = coarseIndex(r.timestamp);
      CoarseBins[ix].emplace_back(r);
      valid++;
      maxt = std::max(maxt, r.timestamp);
    }
  }
  if (valid) {
    if ((maxt - BaseTime) >= timeSpan()) {
      EndIx = (coarseIndex(maxt) + 1) % NumCoarseBins;
    }
  }
  return valid;
}

size_t EventBinning::resetBins(uint64_t basetime) {
  // if the new base time is within the current window and it is non-empty
  // drop the events before the new time leaving other events
  size_t sum{0};
  if (!empty() && validEventTime(basetime)) {
    auto newIx = coarseIndex(basetime);
    if (BaseIx != newIx)
      clearFineBins();
    while (BaseIx != newIx) {
      sum += incrementBase();
    }
  } else {
    // flush the bins and reset the time
    clearFineBins();
    sum = clearCoarseBins();
    BaseTime = ((basetime >> CoarseWidthBits) << CoarseWidthBits);
    BaseIx = EndIx = coarseIndex(basetime);
  }
  return sum;
}

// Mapping to fine bins does not delete the data in the coarse bin
// as it is initialized with a pointer to the data
size_t EventBinning::mapToFineBins() {
  size_t sum{0};
  if (!empty()) {
    for (auto &r : CoarseBins[BaseIx]) {
      FineBins[fineIndex(r.timestamp)].emplace_back(r);
      sum++;
    }
  }
  return sum;
}

size_t EventBinning::clearFineBins() {
  size_t sum = 0;
  for (auto &bin : FineBins) {
    sum += bin.size();
    bin.clear();
  }
  return sum;
}

size_t EventBinning::clearCoarseBins() {
  size_t sum = 0;
  for (auto &bin : CoarseBins) {
    sum += bin.size();
    bin.clear();
  }
  return sum;
}

size_t EventBinning::incrementBase() {
  size_t sum = 0;
  if (!empty()) {
    auto &bin = CoarseBins[BaseIx];
    sum += bin.size();
    bin.clear();
    BaseIx = (BaseIx + 1) % NumCoarseBins;
    BaseTime += CoarseBinWidth;
  }
  return sum;
}

// Dumps the raw events in the bins for the time window
size_t EventBinning::dumpBlock(uint64_t window, std::vector<Readout> &output) {
  size_t sum{0};
  uint64_t endTime = BaseTime + window;
  while (!empty() && BaseTime < endTime) {
    for (auto & r : CoarseBins[BaseIx]) {
      output.emplace_back(r);
      sum++;
    }
    incrementBase();
  }
  return sum;
}