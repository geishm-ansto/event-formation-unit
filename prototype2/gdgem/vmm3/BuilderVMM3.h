/* Copyright (C) 2016-2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief Class for creating NMX hits from SRS/VMM data
///
//===----------------------------------------------------------------------===//

#pragma once
#include <gdgem/nmx/AbstractBuilder.h>
#include <gdgem/srs/SRSMappings.h>
#include <gdgem/srs/SRSTime.h>
#include <gdgem/vmm3/ParserVMM3.h>
#include <gdgem/vmm3/CalibrationFile.h>
#include <gdgem/nmx/Readout.h>

#include <common/Trace.h>
// #undef TRC_LEVEL
// #define TRC_LEVEL TRC_L_DEB

namespace Gem {

class BuilderVMM3 : public AbstractBuilder {
public:
  BuilderVMM3(SRSTime time_intepreter, SRSMappings geometry_interpreter,
              uint16_t adc_threshold, std::string dump_dir,
              std::shared_ptr<CalibrationFile> calfile);

  ~BuilderVMM3() { XTRACE(INIT, DEB, "BuilderVMM3 destructor called"); }

  /// \todo Martin document
  ResultStats process_buffer(char *buf, size_t size) override;

 private:
  std::shared_ptr<CalibrationFile> calfile_;
  VMM3SRSData parser_;
  SRSTime time_intepreter_;
  SRSMappings geometry_interpreter_;

  uint16_t adc_threshold_ {0};

  std::shared_ptr<ReadoutFile> readout_file_;

  // preallocated and reused
  Readout readout;
  Hit hit;

  uint32_t geom_errors {0};
  uint32_t below_adc_threshold {0};
};

}
