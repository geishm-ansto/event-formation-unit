/* Copyright (C) 2019 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief Ansto detector
///
//===----------------------------------------------------------------------===//

#include "AnstoBase.h"
#include <common/Detector.h>

static struct Ansto::AnstoSettings localAnstoSettings;

void SetCLIArguments(CLI::App __attribute__((unused)) & parser) {
  parser.add_option("--dumptofile", localAnstoSettings.FilePrefix,
                    "dump to specified file")->group("Ansto");

  parser.add_option("-f, --file", localAnstoSettings.ConfigFile,
                    "ANSTO instrument specific calibration (json) file")
                    ->group("Ansto");

  parser.add_option("--h5filesplit", localAnstoSettings.H5SplitTime,
                    "Specify interval to split HDF5 files")
                    ->group("Ansto");
}

PopulateCLIParser PopulateParser{SetCLIArguments};

class ANSTO : public Ansto::AnstoBase {
public:
  explicit ANSTO(BaseSettings Settings)
      : Ansto::AnstoBase(std::move(Settings), localAnstoSettings) {}
};

DetectorFactory<ANSTO> Factory;