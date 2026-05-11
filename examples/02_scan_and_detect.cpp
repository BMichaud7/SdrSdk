// Example 2 — Scan 80–200 MHz and print detected signals.
#include <sdrsdk/sdrsdk.hpp>
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

    // Submit WIDEBAND tasks per step (could also use SCAN task type)
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Scanning 80–200 MHz...\n\n";

    for (auto& e : entries) {
        auto resp = client.wideband(e.center_freq_hz, e.bandwidth_hz,
                                    e.sample_rate_sps, 2500);
        if (!resp.accepted) { std::cerr << "  rejected at " << e.center_freq_hz/1e6 << " MHz\n"; continue; }

        int port = resp.streams.empty() ? 0 : resp.streams[0].udp_port;
        sdr::IqStream stream(port);
        auto iq = stream.collect_complex_for(2.0);
        client.stop(resp.task_id);

        auto signals = spectrum.analyse(iq, e.center_freq_hz, e.sample_rate_sps);
        double lo = (e.center_freq_hz - e.bandwidth_hz/2) / 1e6;
        double hi = (e.center_freq_hz + e.bandwidth_hz/2) / 1e6;
        std::cout << "── " << lo << "–" << hi << " MHz (" << iq.size() << " samples)\n";
        if (signals.empty()) {
            std::cout << "  (nothing above threshold)\n";
        } else {
            for (auto& s : signals)
                std::cout << "  " << s.str() << "\n";
        }
    }
    return 0;
}
