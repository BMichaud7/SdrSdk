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
 * @file units.hpp
 * @brief Au units library helpers for SDR physical quantities.
 *
 * Provides the sdrunit:: namespace with convenience constructors:
  sdrunit::MHz(n), sdrunit::kHz(n), sdrunit::s(n), sdrunit::ms(n)
All SdrClient methods accept au::QuantityD<au::Hertz> and au::QuantityD<au::Seconds>.
 */
#pragma once

// Au units library — https://aurora-opensource.github.io/au/main/
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"
#include "au/units/minutes.hh"
#include "au/units/hours.hh"
#include "au/units/days.hh"
#include "au/prefix.hh"

// Bring in Au's core quantity type so consumers only need this header.
#include "au/quantity.hh"

namespace sdrunit {

// ── Frequency quantity makers ─────────────────────────────────────────────────
// Usage:  Hz(100.0)       →  100 Hz
//         kHz(2.4)        →  2.4 kHz   (= 2400 Hz)
//         MHz(98.5)       →  98.5 MHz
//         GHz(2.4)        →  2.4 GHz
//
// Conversion:
//         MHz(98.5).in(au::hertz)        →  98500000.0  (double)
//         MHz(98.5).as(kHz)              →  Quantity<kilohertz, double>(98500)

inline constexpr auto Hz  = au::hertz;
inline constexpr auto kHz = au::kilo(au::hertz);
inline constexpr auto MHz = au::mega(au::hertz);
inline constexpr auto GHz = au::giga(au::hertz);

// ── Time quantity makers ──────────────────────────────────────────────────────
// Usage:  s(1.5)          →  1.5 seconds
//         min(3.0)        →  3 minutes
//         h(0.5)          →  0.5 hours
//         day(7.0)        →  7 days
//
// Conversion:
//         min(3.0).in(au::seconds)       →  180.0

inline constexpr auto s   = au::seconds;
inline constexpr auto min = au::minutes;
inline constexpr auto h   = au::hours;
inline constexpr auto day = au::days;

} // namespace sdrunit

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
