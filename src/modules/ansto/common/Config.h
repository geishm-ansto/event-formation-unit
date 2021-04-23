/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief class for configuration of ansto detector settings
/// reading from json file.
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <string>
#include <vector>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <nlohmann/json.hpp>
#pragma GCC diagnostic pop

namespace Ansto {

class Config {
public:
  Config() = default;
  ~Config() = default;

  /// \brief initiate configuration from file
  explicit Config(std::string jsonfile);

  /// \brief if true successfully loaded config file
  bool isConfigLoaded() const { return ConfigLoaded; }

  /// \brief get the config file loaded 
  std::string getConfigFile() const { return ConfigFile; }

  /// \brief returns the instrument name
  std::string getInstrument() const { return Instrument; }

  /// \brief get the parser for the stream 
  std::string getReader() const { return Reader; }

  /// \brief get the comms type (udp, zmq, ..)
  std::string getDAEComms() const { return DAEComms; }

  /// \brief get the json structure for the entry
  const nlohmann::json &getRoot(const std::string &tag) const {
    return Root[tag];
  }

  /// \brief returns a reference to base json structure
  const nlohmann::json &getRoot() const { return Root; }

private:

  void loadConfigFile();

  bool ConfigLoaded{false};

  std::string ConfigFile{""};

  std::string Instrument{""};

  std::string Reader{""};

  std::string DAEComms{"udp"};

  nlohmann::json Root;
};

} // namespace Ansto
