/* Copyright (C) 2017-2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
///                              WARNING
///
///                              ACHTUNG
///
///                              AVISO
///
///                              ADVARSEL
///
///                              DĖMESIO
///
///                              UWAGA
///
///
///
///          MODIFY THIS FILE ONLY AFTER UNDERSTANDING THE FOLLOWING
///
///
///   Any changes to non-static variable definitions will likely break h5 file
/// writing and compatibility. If you rename, reorder or change the type of any
/// of the member variables, you MUST:
///    A) Increment FormatVersion by 1
///    B) Ensure the hdf5 TypeTrait maps the struct correctly
///
/// If you cannot ensure the above, consult someone who can.
///
/// \file
///
/// \brief Readout struct for ANSTO parsers.
///
//===----------------------------------------------------------------------===//

#pragma once

#include <common/DumpFile.h>

namespace Ansto {

/// common detector acquisition events 
enum EventType : uint8_t {
  Blank,
  Neutron,
  FrameStart,
  FrameAuxStart,
  FrameDeassert,  ///< \todo is it needed?
  Veto,           ///< \todo is it needed?
  BeamMonitor,
  OtherEvent,
  Invalid = 0xFF
};

extern const std::map<std::string, EventType> StringToEventType;

/// Common output format from all ansto parsers.
/// 
/// Optimized for neutron events with explicit x, y posn and
/// weighting. Source and data are event type specific attributes.
struct __attribute__((packed)) Readout {
  /// \todo use constexpr string_view when c++17 arrives
  static std::string DatasetName() { return "ansto_readouts"; }
  static uint16_t FormatVersion() { return 0; }

  /// \todo consider reordering these to optimize
  /// !!! DO NOT MODIFY BELOW - READ HEADER FIRST !!!
  uint64_t timestamp{0};
  uint8_t event_type{0};
  uint8_t source{0};
  uint16_t x_posn{0};
  uint16_t y_posn{0};
  uint16_t weight{0};
  uint32_t data{0};

  // !!! DO NOT MODIFY ABOVE -- READ HEADER FIRST !!!

  // \brief prints values for to_string purposes
  std::string debug() const;
};

} // namespace Ansto

namespace hdf5 {

namespace datatype {
template <> class TypeTrait<Ansto::Readout> {
public:
  H5_COMPOUND_DEFINE_TYPE(Ansto::Readout) {
    H5_COMPOUND_INIT;
    /// Make sure ALL member variables are inserted
    H5_COMPOUND_INSERT_MEMBER(timestamp);
    H5_COMPOUND_INSERT_MEMBER(event_type);
    H5_COMPOUND_INSERT_MEMBER(source);
    H5_COMPOUND_INSERT_MEMBER(x_posn);
    H5_COMPOUND_INSERT_MEMBER(y_posn);
    H5_COMPOUND_INSERT_MEMBER(weight);
    H5_COMPOUND_INSERT_MEMBER(data);
    H5_COMPOUND_RETURN;
  }
};
} // namespace datatype

} // namespace hdf5

namespace Ansto {

using ReadoutFile = DumpFile<Readout>;
}