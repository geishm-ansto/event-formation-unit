/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief class for configuration of ansto detector settings
/// reading from json file.
//===----------------------------------------------------------------------===//

#pragma once

#include <map>
#include <memory>
#include <string>

#include "Readout.h"

namespace Ansto {

template <typename T>
ssize_t InvokeReceive(void *object, void *buffer, int size) {
  return static_cast<T *>(object)->receive(buffer, size);
}

template <typename T> int InvokeSend(void *object, void *buffer, int size) {
  return static_cast<T *>(object)->send(buffer, size);
}

/// Common interface for the Ansto detector parsers
class IParserMethod {
public:
  IParserMethod() = default;
  virtual ~IParserMethod() = default;

  /// parse's the buffer content and returns the number of events and 
  /// absolute time range
  virtual int parse(const char * buffer, unsigned int size,
                    uint64_t &tsmin, uint64_t &tsmax) = 0;

  /// converts the 2D coordinate to a pixel id
  virtual size_t pixelID(uint16_t xpos, uint16_t ypos) = 0;

  /// number of bad events in the parsed data
  virtual unsigned int byteErrors() = 0;

  /// buffer sequence errors returned
  virtual unsigned int seqErrors() = 0;

  /// reference to the parsed results
  virtual const std::vector<Readout> &getReadouts() = 0;
};

/// Method for registering and creating instance of the parser
class ParserMethodFactory {
public:
  using TCreateMethod = std::unique_ptr<IParserMethod> (*)(const std::string &);

public:
  ParserMethodFactory() = delete;

  /// register by creation method by name
  static bool addMethod(const std::string &name, TCreateMethod funcCreate);

  /// create the parser and pass the config file
  static std::unique_ptr<IParserMethod> create(const std::string &name,
                                               const std::string &config);
};

/// helper to register in a single line
template <typename TDerived> class RegisteredParser {
public:
  RegisteredParser(const std::string &name) {
    ParserMethodFactory::addMethod(name, TDerived::CreateMethod);
  }
};

} // namespace Ansto