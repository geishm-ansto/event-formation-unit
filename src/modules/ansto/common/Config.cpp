/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief using nlohmann json parser to read configurations from file
//===----------------------------------------------------------------------===//

#include <ansto/common/Config.h>
#include <common/Log.h>
#include <common/Trace.h>
#include <fstream>

namespace Ansto {

Config::Config(std::string jsonfile) : ConfigFile(jsonfile) {

  loadConfigFile();

  if (!isConfigLoaded()) {
    throw std::runtime_error("Unable to load configuration file.");
  }
}

void Config::loadConfigFile() {

  if (ConfigFile.empty()) {
    LOG(INIT, Sev::Info,
        "JSON config - no config file specified, using default configuration");
    throw std::runtime_error("No config file provided.");
  }

  LOG(INIT, Sev::Info, "JSON config - loading configuration from file {}",
      ConfigFile);
  std::ifstream t(ConfigFile);
  std::string jsonstring((std::istreambuf_iterator<char>(t)),
                         std::istreambuf_iterator<char>());

  if (!t.good()) {
    XTRACE(INIT, ERR, "Invalid Json file: %s", ConfigFile.c_str());
    throw std::runtime_error(
        "Ansto config file error - requested file unavailable.");
  }

  try {
    Root = nlohmann::json::parse(jsonstring);

    /// extract common config parameters below and reader specific externally
    Instrument = Root["Instrument"].get<std::string>();
    Reader = Root["Reader"].get<std::string>();
    DAEComms = Root["DAEComms"].get<std::string>();

  } catch (...) {
    LOG(INIT, Sev::Error, "JSON config - error: Invalid Json file: {}",
        ConfigFile);
    return;
  }

  ConfigLoaded = true;
}

} // namespace Ansto
