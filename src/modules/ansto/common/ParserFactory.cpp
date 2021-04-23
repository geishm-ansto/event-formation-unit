/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief Factory methods for ansto parsers
///
//===----------------------------------------------------------------------===//

#include "ParserFactory.h"

namespace Ansto {

using std::string;
using TCreateMethod = ParserMethodFactory::TCreateMethod;

static std::map<std::string, TCreateMethod> &get_map() {
  static std::map<std::string, TCreateMethod> methods{};
  return methods;
}

bool ParserMethodFactory::addMethod(const string &name,
                                   TCreateMethod funcCreate) {
  auto& methods = get_map();
  auto it = methods.find(name);
  if (it == methods.end()) {
    methods[name] = funcCreate;
    return true;
  }
  return false;
}

std::unique_ptr<IParserMethod>
ParserMethodFactory::create(const string &name, const string &config) {

  auto& methods = get_map();
  auto it = methods.find(name);
  if (it != methods.end())
    return it->second(config); // call the createFunc
  return nullptr;
}

} // namespace Ansto