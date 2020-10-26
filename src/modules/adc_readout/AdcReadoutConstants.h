/* Copyright (C) 2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// \brief Parsing code for ADC readout.
///
//===----------------------------------------------------------------------===//

#pragma once

#include <cstdint>

/// \brief Clock frequency of the MRF timing hardware clock.

static const std::int32_t TimerClockFrequencyExternal88Mhz = 88'052'500; // Hz

static const std::int32_t TimerClockFrequencyExternal = TimerClockFrequencyExternal88Mhz / 2; // Hz

static const std::int32_t TimerClockFrequencyInternal = 45'000'000; // Hz
