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
// Example 2 — Scan 80–200 MHz and print detected signals.
#include <sdrsdk/sdrsdk.hpp>
#include <au/units/hertz.hh>
#include <au/units/seconds.hh>
#include <iostream>
#include <iomanip>

int main() {
    sdr::SdrClient client;
    client.connect();

    sdr::Spectrum spectrum(8192);

    // Build scan table: 80–200 MHz in 20 MHz steps
    std::vector<sdr::ScanEntry> entries;
    int step = 0;
    for (double cf = 90e6; cf <= 190e6; cf += 20e6)
        entries.push_back({step++, cf, 20e6, 20e6, 500});

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Scanning 80–200 MHz...\n\n";

    for (auto& e : entries) {
        auto cf = au::hertz(e.center_freq_hz);
        auto bw = au::hertz(e.bandwidth_hz);
        auto sr = au::hertz(e.sample_rate_sps);

        auto resp = client.wideband(cf, bw, sr, au::seconds(2.5));
        if (!resp.accepted) {
            std::cerr << "  rejected at " << e.center_freq_hz/1e6 << " MHz\n";
            continue;
        }

        int port = resp.streams.empty() ? 0 : resp.streams[0].udp_port;
        sdr::IqStream stream(port);
        auto iq = stream.collect_complex_for(au::seconds(2.0));
        client.stop(resp.task_id);

        auto signals = spectrum.analyse(iq, cf, sr);
        std::cout << "── " << (e.center_freq_hz - e.bandwidth_hz/2)/1e6
                  << "–" << (e.center_freq_hz + e.bandwidth_hz/2)/1e6
                  << " MHz (" << iq.size() << " samples)\n";
        if (signals.empty()) {
            std::cout << "  (nothing above threshold)\n";
        } else {
            for (auto& s : signals)
                std::cout << "  " << s.str() << "\n";
        }
    }
    return 0;
}
