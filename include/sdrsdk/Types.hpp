#pragma once
// SdrSdk uses SdrTaskApi as its canonical type definitions.
// Include this or <sdr/Types.hpp> directly — they are the same types.
#include <sdr/Types.hpp>

namespace sdr {

// DetectedSignal is defined only in SdrSdk (Spectrum analysis result).
struct DetectedSignal {
    double      freq_mhz;
    double      bw_khz;
    double      power_dbc;
    std::string type;

    std::string str() const;
};

} // namespace sdr
