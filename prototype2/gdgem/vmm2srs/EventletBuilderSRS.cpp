/** Copyright (C) 2016, 2017 European Spallation Source ERIC */

#include <cassert>
#include <common/Trace.h>
#include <fcntl.h>
#include <gdgem/vmm2srs/EventletBuilderSRS.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

BuilderSRS::BuilderSRS(SRSTime time_intepreter,
                       SRSMappings geometry_interpreter)
    : AbstractBuilder()
    , parser_(1125)
    , time_intepreter_(time_intepreter)
    , geometry_interpreter_(geometry_interpreter)
{
#ifdef DUMPTOFILE
        vmmsave.tofile("# fec, chip_id, frame counter, srs timestamp, channel, bcid, tdc, adc, overthreshold\n");
#endif
}

AbstractBuilder::ResultStats
BuilderSRS::process_buffer(char *buf, size_t size,
                           Clusterer &clusterer,
                           NMXHists &hists) {
  parser_.receive(buf, size);
  if (!parser_.elems)
    return AbstractBuilder::ResultStats();

  uint16_t fec_id = 1;                            /**< @todo not hardcode */
  uint16_t chip_id = parser_.srshdr.dataid & 0xf; /**< @todo may belong elswhere */

  Eventlet eventlet;
  for (unsigned int i = 0; i < parser_.elems; i++) {
    auto &d = parser_.data[i];
    XTRACE(PROCESS, DEB, "eventlet timestamp: srs: 0x%08x, bc: 0x%08x, tdc: 0x%08x\n",
           parser_.srshdr.time, d.bcid, d.tdc);
    XTRACE(PROCESS, DEB, "eventlet chip: %d, channel: %d\n", chip_id, d.chno);
    eventlet.time = time_intepreter_.timestamp(parser_.srshdr.time, d.bcid, d.tdc);
    eventlet.plane_id = geometry_interpreter_.get_plane(fec_id, chip_id);
    eventlet.strip = geometry_interpreter_.get_strip(fec_id, chip_id, d.chno);
    eventlet.adc = d.adc;
    eventlet.over_threshold = (d.overThreshold != 0);
    XTRACE(PROCESS, DEB, "eventlet  plane_id: %d, strip: %d\n",
           eventlet.plane_id, eventlet.strip);
    /**< @todo flags? */

    if (eventlet.plane_id == NMX_INVALID_PLANE_ID) {
      XTRACE(PROCESS, ERR, "Bad SRS mapping --  fec: %d, chip: %d\n",
             fec_id, chip_id);
      /**< @todo increment errorcounter instead? */
    } else {
      hists.bin(eventlet);
      clusterer.insert(eventlet);
    }

#ifdef DUMPTOFILE
    vmmsave.tofile("%2d, %2d, %u %u, %2d, %d, %d, %d, %d\n",
            fec_id, chip_id, parser_.srshdr.fc, parser_.srshdr.time,
            d.chno, d.bcid, d.tdc, d.adc, d.overThreshold);
#endif

  }

  return AbstractBuilder::ResultStats(parser_.elems, parser_.error);
}
