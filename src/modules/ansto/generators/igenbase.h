/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
#pragma once

#include <cstddef>

namespace Ansto {
namespace Generator {

class ISource {
public:
  ISource() = default;
  virtual ~ISource() = default;

  virtual int Next(char *buffer, size_t max_size) = 0;
};

} // namespace Generator
} // namespace Ansto