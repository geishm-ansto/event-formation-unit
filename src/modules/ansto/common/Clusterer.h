/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief cluster class for detection events.
//===----------------------------------------------------------------------===//

#pragma once

#include "Cluster.h"

namespace Ansto {

using Bucket = std::vector<Readout>;

class ClusterManager {
public:
  /// \brief Manages the allocation of events to clusters.
  ///
  /// \param spanT max cluster duration in nsecs
  /// \param delT min separation in time between adjacent events in nsecs
  /// \param delX min separation in pixels between adjacent events
  /// \param delY min separation in pixels between adjacent events
  /// \param nsize maximum number of active clusters
  ClusterManager(uint64_t spanT, uint64_t delT, uint16_t delX, uint16_t delY,
                 size_t nsize);
  
  /// copy constructor 
  ClusterManager(const ClusterManager &a);

  /// defines the max cluster duration and min separation between adjacent
  /// events
  void setLimits(uint64_t spanT, uint64_t delT, uint16_t delX, uint16_t delY);

  /// allocates the event to an existing cluster or start new cluster 
  void addEvent(const Readout &evt);

  /// discards all currently active events
  void clearActive();

  /// discards clusters before start and clusters before end to output
  size_t processActive(uint64_t start_bin, uint64_t end_bin);

  /// move the clusters to this manager
  void moveActiveClusters(ClusterManager &b);

  /// discards all closed output events 
  size_t clearOutput();

  /// reduce the clusters to single events and clear the output
  size_t flushOutput(std::vector<Readout> &readouts);

  /// number of active clusters that can be allocated events 
  size_t numActive() const { return ActiveClusters.size(); }

  /// number of closed clusters that cannot be updated 
  size_t numOutput() const { return OutputClusters.size(); }

  /// remaining cluster capacity from pool 
  size_t capacity() const { return FreeClusters.size(); }

private:
  bool matchesCluster(const Cluster &cls, const Readout &evt) const;
  bool eventsClose(const Readout &a, const Readout &b) const;
  void newCluster(const Readout &evt);
  void newCluster(const Cluster &a);
  void addToCluster(Cluster &cls, const Readout &evt);
  void mergeClusters(Cluster &clA, Cluster &clB);
  void dropActiveCluster(size_t index);

  std::vector<Cluster> ClusterPool;
  std::vector<Cluster *> ActiveClusters;
  std::vector<Cluster *> FreeClusters;
  std::vector<Cluster *> OutputClusters;
  uint64_t SpanT;
  uint64_t DelT;
  uint16_t DelX, DelY;
  uint64_t DelX2, DelY2, DelXY2;
};


// ParallelClusterer processes a block of Events by running multiple
// ClusterManagers on different threads.
//
class ParallelClusterer {
public:
  /// \brief Parallize the cluster process by running cluster managers on several threads.
  ///
  /// \param numThreads number of threads with one cluster manager per thread
  /// \param spanT max cluster duration in nsecs
  /// \param delT min separation in time between adjacent events in nsecs
  /// \param delX min separation in pixels between adjacent events
  /// \param delY min separation in pixels between adjacent events
  /// \param nsize maximum number of active clusters
  ParallelClusterer(int numthreads, uint64_t spanT, uint64_t delT,
                    uint16_t delX, uint16_t delY, size_t nsize);

  /// \brief Process the raw events buckets and save reduced clusters to the output.
  ///
  /// \param baseTime absolute time floor for the first event bin in nsecs from unix epoch
  /// \param binWidth bin width in nsecs
  /// \param eventBins collection of uniform bin buckets
  /// \param neutronEvents output for cluster reduced events
  /// \return size_t number of reduced output events
  /// 
  size_t ProcessEvents(const uint64_t baseTime, uint64_t binWidth,
                       const std::vector<Bucket> &eventBins,
                       std::vector<Readout> &neutronEvents);

private:
  int NumThreads;
  std::vector<ClusterManager> ClusterGroup;
};
} // namespace Ansto