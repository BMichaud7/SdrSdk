#pragma once
// SdrSdk uses SdrTaskApi as its canonical type definitions.
// Include this or <sdr/Types.hpp> directly — they are the same types.
#include <sdr/Types.hpp>
#include "sdrsdk/units.hpp"

namespace sdr {

// DetectedSignal is defined only in SdrSdk (Spectrum analysis result).
struct DetectedSignal {
    au::QuantityD<au::Hertz> freq;       // centre frequency in Hz
    au::QuantityD<au::Hertz> bw;         // bandwidth in Hz
    double      power_dbc;
    std::string type;

    std::string str() const;
};

} // namespace sdr
