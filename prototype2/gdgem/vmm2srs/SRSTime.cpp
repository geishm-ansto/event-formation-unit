/** Copyright (C) 2016, 2017 European Spallation Source ERIC */

#include <gdgem/vmm2srs/SRSTime.h>
#include <sstream>

void SRSTime::set_rebin_tdc(bool rebin_tdc) { rebin_tdc_ = rebin_tdc; }

void SRSTime::set_bc_clock(double bc_clock) { bc_clock_ = bc_clock; }

void SRSTime::set_tac_slope(double tac_slope) { tac_slope_ = tac_slope; }

void SRSTime::set_trigger_resolution(double trigger_resolution) {
  trigger_resolution_ = trigger_resolution;
}

void SRSTime::set_target_resolution(double target_resolution) {
  target_resolution_ns_ = target_resolution;
}

bool SRSTime::rebin_tdc() const { return rebin_tdc_; }

double SRSTime::bc_clock() const { return bc_clock_; }

double SRSTime::tac_slope() const { return tac_slope_; }

double SRSTime::trigger_resolution() const { return trigger_resolution_; }

double SRSTime::target_resolution() const { return target_resolution_ns_; }

double SRSTime::chip_time(uint16_t bc, uint16_t tdc) const {
  // Calculate bcTime [us]

  double bcTime = bc / bc_clock_;

  // TDC time: pTAC * tdc value (8 bit)/ramp length
  // [ns]

  // TDC has reduced resolution due to most significant bit problem of current
  // sources (like ADC)
  if (rebin_tdc_) {
    int tdcRebinned = (int) tdc / 8;
    tdc = tdcRebinned * 8;
  }

  // should this not be 256.0?
  double tdcTime = tac_slope_ * static_cast<double>(tdc) / 255.0;

  // Chip time: bcid plus tdc value
  // Talk Vinnie: HIT time  = BCIDx25 + ADC*125/255 [ns]
  return bcTime * 1000 + tdcTime;
}

double SRSTime::timestamp_ns(uint32_t trigger, uint16_t bc, uint16_t tdc) {
  if (trigger < recent_trigger_)
    bonus_++;
  recent_trigger_ = trigger;

  return (bonus_ << 32) + (trigger * trigger_resolution_) + chip_time(bc, tdc);
}

uint64_t SRSTime::timestamp(uint32_t trigger, uint16_t bc, uint16_t tdc) {
  return static_cast<uint64_t>(timestamp_ns(trigger, bc, tdc) *
      target_resolution_ns_);
}

std::string SRSTime::debug() const {
  std::stringstream ss;
  ss << "("
     << "trigger*" << trigger_resolution_ << " + bc*1000/" << bc_clock_
     << " + tdc*" << tac_slope_ << "/256"
     << ")ns * " << target_resolution_ns_;
  return ss.str();
}
