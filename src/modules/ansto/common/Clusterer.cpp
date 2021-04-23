//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief class for building and managing clusters.
//===----------------------------------------------------------------------===//

#include <ansto/common/Clusterer.h>
#include <omp.h>

using namespace Ansto;

#pragma omp declare reduction (merge : std::vector<Readout> : omp_out.insert(omp_out.end(), omp_in.begin(), omp_in.end()))

// The ClusterManager maintains a pool of clusters where the normal
// lifecycle for a cluster is to move from:
//    ClusterPool -> ActiveClusters:
//      - occurs when a new event does not belong to any active clusters
//    ActiveClusters -> OutputClusters:
//      - active clusters cannot be updated when the max duration is exceeded
//    OutputClusters -> ClusterPool:
//      - returned to the cluster pool when the clusters have been processed
//        and the output is flushed
// 
// Note that the clusters are not cleared before being returned to the free list
// as they are reset when a new cluster is grabbed from the free list.
//
ClusterManager::ClusterManager(uint64_t spanT, uint64_t delT, uint16_t delX,
                               uint16_t delY, size_t nsize)
    : ClusterPool(nsize) {
  setLimits(spanT, delT, delX, delY);
  FreeClusters.resize(nsize);
  for (size_t i = 0; i < nsize; i++)
    FreeClusters[i] = &ClusterPool[i];
  ActiveClusters.reserve(nsize);
  OutputClusters.reserve(nsize);
}

ClusterManager::ClusterManager(const ClusterManager &a) {
  setLimits(a.SpanT, a.DelT, a.DelX, a.DelY);
  size_t nsize = a.ClusterPool.size();
  ClusterPool.resize(nsize);
  FreeClusters.resize(nsize);
  for (size_t i = 0; i < nsize; i++)
    FreeClusters[i] = &ClusterPool[i];
  ActiveClusters.reserve(nsize);
  OutputClusters.reserve(nsize);
}

void ClusterManager::setLimits(uint64_t spanT, uint64_t delT, uint16_t delX,
                               uint16_t delY) {
  SpanT = spanT;
  DelT = delT;
  DelX = delX;
  DelY = delY;
  DelX2 = DelX * DelX;
  DelY2 = DelY * DelY;
  DelXY2 = DelX2 * DelY2;
}

bool ClusterManager::matchesCluster(const Cluster &cls,
                                    const Readout &evt) const {
  // skip the scan through the hits if it doesn't meet the boundary condition
  if (evt.timestamp + DelT < cls.delT.lo ||
      evt.timestamp > cls.delT.hi + DelT || evt.x_posn + DelX < cls.delX.lo ||
      evt.x_posn > cls.delX.hi + DelX || evt.y_posn + DelY < cls.delY.lo ||
      evt.y_posn > cls.delY.hi + DelY || evt.timestamp > cls.delT.lo + SpanT)
    return false;
  for (auto &e : cls.hits) {
    if (eventsClose(e, evt))
      return true;
  }
  return false;
}

bool ClusterManager::eventsClose(const Readout &a, const Readout &b) const {
  uint64_t dist;
  // use a simple distance measure that supports non-uniform X and Y distance
  // to support tubes where x may represent tube number and y posn in tube
  bool valid =
      (b.timestamp - a.timestamp <= DelT || a.timestamp - b.timestamp <= DelT);
  if (valid) {
    dist = (a.x_posn - b.x_posn) * (a.x_posn - b.x_posn) * DelY2 +
           (a.y_posn - b.y_posn) * (a.y_posn - b.y_posn) * DelX2;
    valid = (dist <= DelXY2);
  }
  return valid;
}

void ClusterManager::newCluster(const Readout &evt) {
  if (!FreeClusters.empty()) {
    auto pc = FreeClusters.back();
    FreeClusters.pop_back();
    ActiveClusters.push_back(pc);
    pc->newCluster(evt);
  }
}

void ClusterManager::newCluster(const Cluster &a) {
  if (!FreeClusters.empty()) {
    auto pc = FreeClusters.back();
    FreeClusters.pop_back();
    ActiveClusters.push_back(pc);
    *pc = a;
  }
}

void ClusterManager::dropActiveCluster(size_t index) {
  // drops the active cluster efficiently at the cost of losing order
  // but it needs to search all the elements so order is not important
  size_t nsize = ActiveClusters.size();
  if (index < nsize) {
    auto p = ActiveClusters[index];
    FreeClusters.push_back(p);
    ActiveClusters[index] = ActiveClusters[nsize - 1];
    ActiveClusters.pop_back();
  }
}

void ClusterManager::moveActiveClusters(ClusterManager &b) {
  for (auto pc : b.ActiveClusters) {
    newCluster(*pc);
    b.FreeClusters.push_back(pc);
  }
  b.ActiveClusters.clear();
}

// The process of adding an event is to find any
// matching clusters but if there is more than one
// match then merge those clusters to a single matching
// cluster. Add the event to the matching cluster else
// start a new cluster from this event if there was no 
// match.
void ClusterManager::addEvent(const Readout &evt) {
  // event could be linked to N clusters
  constexpr int MaxLinks{5};
  std::array<std::pair<size_t, Cluster *>, MaxLinks> clusters;
  size_t hits = 0;

  // looks for matching clusters
  for (size_t i = 0; i < ActiveClusters.size(); i++) {
    auto pcls = ActiveClusters[i];
    if (hits < MaxLinks && matchesCluster(*pcls, evt))
      clusters[hits++] = std::make_pair(i, pcls);
  }

  // if matching clusters, merge the linked clusters and add the event
  if (hits > 0) {
    for (size_t i = 1; i < hits; i++) {
      mergeClusters(*clusters[0].second, *clusters[i].second);
      dropActiveCluster(clusters[i].first);
    }
    addToCluster(*clusters[0].second, evt);
  } else {
    newCluster(evt);
  }
}

void ClusterManager::clearActive() {
  // just move the cluster index from active to free
  for (auto pc : ActiveClusters) {
    FreeClusters.push_back(pc);
  }
  ActiveClusters.clear();
}

void ClusterManager::addToCluster(Cluster &cls, const Readout &evt) {
  cls.hits.emplace_back(evt);
  cls.delT.extend(evt.timestamp);
  cls.delX.extend(evt.x_posn);
  cls.delY.extend(evt.y_posn);
  cls.moment.add(evt.weight, evt.x_posn, evt.y_posn);
  cls.moment2.add(evt.weight * evt.weight, evt.x_posn, evt.y_posn);
}

void ClusterManager::mergeClusters(Cluster &clA, Cluster &clB) {
  clA.addCluster(clB);
  clB.clear();
}

size_t ClusterManager::processActive(uint64_t start_bin, uint64_t end_bin) {
  // Clusters which started before start_bin will be discarded
  // while those that started in the boundary are saved to output
  constexpr int MaxClusters{100};
  std::array<Cluster *, MaxClusters> alive;
  size_t count = 0;
  for (auto pc : ActiveClusters) {
    if (pc->delT.lo < start_bin) {
      FreeClusters.push_back(pc);
      continue;
    }
    if (pc->delT.lo < end_bin) {
      OutputClusters.push_back(pc);
    } else if (count < MaxClusters) {
      alive[count++] = pc;
    }
  }
  auto removed = ActiveClusters.size() - count;
  if (count != ActiveClusters.size()) {
    ActiveClusters.clear();
    for (size_t i = 0; i < count; i++)
      ActiveClusters.push_back(alive[i]);
  }
  return removed;
}

size_t ClusterManager::clearOutput() {
  auto n = OutputClusters.size();
  for (auto pc : OutputClusters) {
    FreeClusters.push_back(pc);
  }
  OutputClusters.clear();
  return n;
}

size_t ClusterManager::flushOutput(std::vector<Readout> &readouts) {
  auto n = OutputClusters.size();
  for (auto pc : OutputClusters) {
    readouts.emplace_back(pc->asReadout());

#if 0
    // This section is only included for debugging the cluster processing
    // and should be commented out in the normal build. The code tags the
    // clustered events and records the underlying hits to be examined.
    auto &r = readouts.back();
    r.data = 1;
    if (pc->hits.size() > 1) {
      for (auto &h : pc->hits) {
        h.data = 0;
        readouts.emplace_back(h);
      }
    }
#endif

    FreeClusters.push_back(pc);
  }
  OutputClusters.clear();
  return n;
}

ParallelClusterer::ParallelClusterer(int maxthreads, uint64_t spanT,
                                     uint64_t delT, uint16_t delX,
                                     uint16_t delY, size_t nsize)
    : NumThreads(maxthreads) {
  for (int i = 0; i < NumThreads; i++)
    ClusterGroup.emplace_back(spanT, delT, delX, delY, nsize);
}

size_t ParallelClusterer::ProcessEvents(const uint64_t baseTime,
                                        uint64_t binWidth,
                                        const std::vector<Bucket> &eventBins,
                                        std::vector<Readout> &neutronEvents) {

  // lambda fn defines the block processing function
  auto ProcessBlock = [&](int igroup, size_t startix, size_t points,
                          std::vector<Readout> &output) -> size_t {
    ClusterManager &clmp = ClusterGroup[igroup];
    size_t events = 0;
    if (igroup > 0) {
      // Capture events in this bin that may be part of an
      // earlier cluster.
      // igroup 0 carries the clusters from the last fine bin
      for (auto i = startix - 2; i < startix; i++) {
        auto &bin = eventBins[i];
        for (auto e : bin) {
          clmp.addEvent(e);
        }
      }
    }

    // Process the events in the bin segments. It assumes that the
    // cluster span is less than the fine bin width so that clusters
    // that started before the bin time can be closed at the end of
    // the processing.
    for (size_t i = 0; i < points; i++) {
      auto &bin = eventBins[startix + i];
      for (auto &e : bin) {
        clmp.addEvent(e);
      }

      // Any events in subsequent loops cannot be assigned to clusters that
      // originated before bin_time because the cluster span will be exceeded.
      // These clusters can be processed.
      uint64_t bin_time = baseTime + binWidth * (startix + i);
      uint64_t cutoff = (bin_time > binWidth ? bin_time - binWidth : 0);
      clmp.processActive(cutoff, bin_time);

      events += clmp.flushOutput(output);
    }
    events += clmp.flushOutput(output);
    return events;
  };

  // setup the parallel structure using omp, note that the actual
  omp_set_num_threads(NumThreads);
  size_t clusterCount = 0;
  size_t npoints = eventBins.size();
  int iam, nt, ipoints, istart;
#pragma omp parallel default(shared) private(iam, nt, ipoints, istart) reduction(+:clusterCount) reduction(merge:neutronEvents)
  {
    iam = omp_get_thread_num();
    nt = omp_get_num_threads();
    ipoints = npoints / nt; /* size of partition */
    istart = iam * ipoints; /* starting array index */
    if (iam == nt - 1)      /* last thread may do more */
      ipoints = npoints - istart;
    clusterCount += ProcessBlock(iam, istart, ipoints, neutronEvents);
    // Assume neutronEvents is reallocated on every call but
    // keeping open vectors and then merging turned out to be no
    // more efficient that this simpler solution, so sticking with it. 
  }
  // Move the active cluster from the last block to the first block before the
  // next cycle. Note that the fine bins cannot be cleared untill all the
  // threads have completed because of the overlap in the block processing.
  if (NumThreads > 1)
    ClusterGroup[0].moveActiveClusters(ClusterGroup[NumThreads - 1]);

  return clusterCount;
}