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
 * @file sdrsdk.hpp
 * @brief Umbrella header — include this for the complete SdrSdk public API.
 *
 * Includes SdrClient, IqStream, Spectrum, Types, and units. The only header
an application needs to include to use the full SDK.
 */
#pragma once
// Umbrella header — include this for full SDK access.
#include <sdr/Types.hpp>          // canonical types (from SdrTaskApi)
#include <sdr/MessageCodec.hpp>   // wire encode/decode (from SdrTaskApi)
#include "SdrClient.hpp"
#include "IqStream.hpp"
#include "Spectrum.hpp"

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
