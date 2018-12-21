/** Copyright (C) 2016-2018 European Spallation Source */

#pragma once
#include <common/Buffer.h>
#include <common/clustering/Hit.h>
#include <vector>

namespace Multigrid {

class AbstractBuilder {
public:
  virtual void parse(Buffer<uint8_t> buffer) = 0;

  std::vector<Hit> ConvertedData;

  size_t stats_trigger_count{0};
  size_t stats_discarded_bytes{0};

};

}