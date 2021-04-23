/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief class for collecting events as clusters
//===----------------------------------------------------------------------===//

#include <ansto/common/Cluster.h>
#include <cmath>

using namespace Ansto;

Cluster::Cluster() { hits.reserve(10); }

void Cluster::operator=(const Cluster &a) {
  hits.clear();
  for (auto &evt : a.hits)
    hits.emplace_back(evt);
  delT = a.delT;
  delX = a.delX;
  delY = a.delY;
  moment = a.moment;
  moment2 = a.moment2;
}

void Cluster::newCluster(const Readout &evt) {
  hits.clear();
  hits.emplace_back(evt);
  delT.lo = delT.hi = evt.timestamp;
  delX.lo = delX.hi = evt.x_posn;
  delY.lo = delY.hi = evt.y_posn;
  moment = Stats(evt.weight, evt.x_posn, evt.y_posn);
  moment2 = Stats(evt.weight * evt.weight, evt.x_posn, evt.y_posn);
}

void Cluster::addEvent(const Readout &evt) {
  // adds the event to the hits, extends the range and
  // updates the stats
  if (hits.empty()) {
    newCluster(evt);
  } else {
    hits.emplace_back(evt);
    delT.extend(evt.timestamp);
    delX.extend(evt.x_posn);
    delY.extend(evt.y_posn);
    moment.add(evt.weight, evt.x_posn, evt.y_posn);
    moment2.add(evt.weight * evt.weight, evt.x_posn, evt.y_posn);
  }
}

void Cluster::addCluster(const Cluster &clB) {
  // individually add the hits to ensure the range and stats are updated
  for (auto &e : clB.hits) {
    addEvent(e);
  }
}

void Cluster::clear() {
  hits.clear();
  moment.clear();
  moment2.clear();
}

Readout Cluster::asReadout() const {
  Readout rd;
  if (hits.size() > 0) {
    rd = hits[0];
    rd.timestamp = delT.lo;
    double wgt = moment.wgt;
    rd.x_posn = static_cast<uint16_t>(round(moment.wgtX / wgt));
    rd.y_posn = static_cast<uint16_t>(round(moment.wgtY / wgt));
    rd.weight = static_cast<uint16_t>(moment.wgt);
  }
  return rd;
}