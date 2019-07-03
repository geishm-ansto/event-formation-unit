/* Copyright (C) 2016-2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file AbstractClusterer.h
/// \brief AbstractClusterer class definition
///
//===----------------------------------------------------------------------===//

#pragma once

#include <list>
#include <deque>
#include <common/clustering/Cluster.h>

// \todo replace by deque, or....?
using ClusterContainer = std::list<Cluster>;

/// \class AbstractClusterer AbstractClusterer.h
/// \brief AbstractClusterer declares the interface for a clusterer class
///         that should group hits into clusters. Provides base functionality
///         for storage of clusters and stats counter. Other pre- and
///         post-conditions are left to the discretion of a specific
///         implementation.

class AbstractClusterer {
public:
  ClusterContainer clusters;     ///< clustered hits
  mutable size_t stats_cluster_count{0}; ///< cumulative number of clusters produced

public:
  AbstractClusterer() = default;
  virtual ~AbstractClusterer() = default;

  /// \brief inserts new hit and potentially performs some clustering
  virtual void insert(const Hit &hit) = 0;

  /// \brief inserts new hits and potentially performs some clustering
  virtual void cluster(const HitVector &hits) = 0;

  /// \brief complete clustering for any remaining hits
  virtual void flush() = 0;

  /// \brief convenience function
  /// \returns if cluster container is empty
  bool empty() const
  {
    return clusters.empty();
  }

protected:

  /// \brief moves cluster into clusters container, increments counter
  /// \param cluster to be stashed
  void stash_cluster(Cluster &cluster) {
    clusters.emplace_back(std::move(cluster));
    stats_cluster_count++;
  }

};
