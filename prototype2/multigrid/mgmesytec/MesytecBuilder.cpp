/** Copyright (C) 2017 European Spallation Source ERIC */

#include <multigrid/mgmesytec/MesytecBuilder.h>

#include <common/Trace.h>
// #undef TRC_LEVEL
// #define TRC_LEVEL TRC_L_DEB

namespace Multigrid {

void MesytecBuilder::parse(Buffer<uint8_t> buffer) {


  stats_discarded_bytes += sis3153parser.parse(Buffer<uint8_t>(buffer));

  for (const auto &b : sis3153parser.buffers) {

    stats_discarded_bytes += vmmr16Parser.parse(b);

    if (vmmr16Parser.converted_data.empty())
      continue;

    if (dumpfile) {
      dumpfile->push(vmmr16Parser.converted_data);
    }

    if (vmmr16Parser.externalTrigger()) {

    }

    for (const auto& r : vmmr16Parser.converted_data) {
      if (!digital_geometry.is_valid(r.bus, r.channel, r.adc))
        continue;
      if (digital_geometry.isWire(r.bus, r.channel)) {
        hit.plane = 0;
        hit.coordinate = digital_geometry.wire(r.bus, r.channel);
        hit.weight = digital_geometry.rescale(r.bus, r.channel, r.adc);
      }
      else if (digital_geometry.isGrid(r.bus, r.channel)) {
        hit.plane = 1;
        hit.coordinate = digital_geometry.grid(r.bus, r.channel);
        hit.weight = digital_geometry.rescale(r.bus, r.channel, r.adc);
      }

      if (r.external_trigger) {
        hit.plane = 99;
        hit.coordinate = 0;
        hit.weight = 0;
      }
      hit.time = r.total_time;

      ConvertedData.push_back(hit);
    }

  }
}

}
