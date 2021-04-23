/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief class for mapping neutron events to timebins.
//===----------------------------------------------------------------------===//

#pragma once

#include <array>
#include <deque>
#include <vector>
#include <atomic>

#include "Readout.h"

namespace Ansto {

using Bucket = std::vector<Readout>;

class EventBinning {
public:
  // \brief to improve perfromance all the bin widths and number of bins are 
  // powers of 2 such that FineBinWidth = (1 << FineWidthBits), etc. where the
  // width is in nsec units 

  /// \brief Bin events by time in to buckets where bins.
  ///
  /// The bins are 2**n width for better performance. There are two
  /// layers: coarse and fine bins, where data in a coarse bin is mapped
  /// to the fine bins for a finer grouping of events. The coarse bin width 
  /// is 2**fineBinBits * 2**fineWidthBits nsecs. 
  /// The coarse window is 2**coarseBinBits * (coarse bin width).
  ///
  /// \param coarseBinBits number of coarse bins is 2**bits
  /// \param fineWidthBits fine bin width is 2**bits in nsecs
  /// \param fineBinBits number of fine bins is 2**bits
  ///
  EventBinning(int coarseBinBits, int fineWidthBits, int fineBinBits);

  /// inserts events into the coarse time bins ignoring events outside the
  /// coarse window
  size_t insert(const std::vector<Readout> &events);  

  /// returns if the event time is inside the coarse window
  inline bool validEventTime(uint64_t evtime) const {
    return ((evtime - BaseTime) >> CoarseWidthBits ) < (NumCoarseBins - 1);
  }

  /// returns the coarse bin index for the event time
  inline size_t coarseIndex(uint64_t evtime) const {
    return (evtime >> CoarseWidthBits) % NumCoarseBins;
  }

  /// returns the fine bin index for the event time
  inline size_t fineIndex(uint64_t evtime) const {
    return (evtime >> (FineWidthBits)) % NumFineBins;
  }

  /// flush and reset the bins to the new base time, returning the number
  // of events dropped
  size_t resetBins(uint64_t basetime);

  /// returns if there is data in the bins
  inline bool empty() const { return BaseIx == EndIx; }

  /// timegap between the start and end of the data (nsecs)
  inline uint64_t timeSpan() const {
    uint64_t delta =
        EndIx >= BaseIx ? EndIx - BaseIx : EndIx + NumCoarseBins - BaseIx;
    return CoarseBinWidth * delta;
  }

  /// returns the timespan covered by the coarse bins in nsec
  inline uint64_t eventWindow() const {
    return CoarseBinWidth * NumCoarseBins;
  }

  /// copies the events in the first coarse bin to the fine bins
  uint64_t mapToFineBins();

  /// returns the number of fine bin events cleared
  size_t clearFineBins();

    /// returns the number of fine coarse events cleared
  size_t clearCoarseBins();

  /// clear the base coarse bin, inc index and returning number of events dropped
  size_t incrementBase();

  /// returns the base time in nsecs
  inline uint64_t getBaseTime() const { return BaseTime; }

  /// returns the time for the coarse bin index 
  uint64_t getBinTime(size_t ix) const {
    return ix * CoarseBinWidth + BaseTime;
  }

  /// dump the events from basetime for the window to the output
  size_t dumpBlock(uint64_t window, std::vector<Readout>& output);

  /// returns the fine bin bucket
  inline const Bucket& getFineBin(size_t ix) { return FineBins[ix];} 

  /// returns a const reference to the fine bins
  inline const std::vector<Bucket>& getFineBins() const { return FineBins; }

  /// returns the coarse bin bucket
  inline const Bucket& getCoarseBin(size_t ix) { return CoarseBins[ix];} 

  /// return coarse bin width in nsecs
  inline uint64_t getCoarseBinWidth() const { return CoarseBinWidth; }

  /// return fine bin width in nsecs
  inline uint64_t getFineBinWidth() const { return FineBinWidth; }

  /// return fine bin width in bits
  inline int getFineWidthBits() const { return FineWidthBits; }

  /// return number of fine bins
  inline uint64_t getNumFineBins() const { return NumFineBins; }

   /// return number of coarse bins
  inline uint64_t getNumCoarseBins() const { return NumCoarseBins; }

private:
  size_t BaseIx, EndIx;
  uint64_t BaseTime;
  const int FineWidthBits;
  const int CoarseWidthBits;
  const uint64_t CoarseBinWidth, FineBinWidth;
  const uint64_t NumCoarseBins, NumFineBins;

  std::vector<Bucket> CoarseBins;
  std::vector<Bucket> FineBins;
};

} // namespace Ansto