/*
========================================================================
Project: OpenRFStack
Author:  Brendan Michaud
Year:    2026
Part of OpenRFStack (https://github.com/OpenRFStack)

Licensed under the Personal Use License.
Do not use for commercial, organizational, or military purposes.
Contact author for permission: https://github.com/OpenRFStack
========================================================================
*/
/**
 * @file Types.hpp
 * @brief SdrSdk public type aliases — re-exports SdrTaskApi types into the sdr:: namespace.
 *
 * Include this or <sdr/Types.hpp> directly. Provides TaskRequest, TaskResponse,
AssignedStream, ScanEntry, SnapshotResult, DetectedSignal and Au unit types.
 */
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

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
