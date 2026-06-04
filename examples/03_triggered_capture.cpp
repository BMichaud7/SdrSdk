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
// Example 3 — TRIGGERED capture: record a burst when signal exceeds threshold.
#include <sdrsdk/sdrsdk.hpp>
#include <au/units/hertz.hh>
#include <au/units/seconds.hh>
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    double cf_hz = (argc > 1) ? std::stod(argv[1]) * 1e6 : 162.4e6;  // NOAA default

    sdr::SdrClient client;
    client.connect();

    std::cout << "Waiting for signal at " << cf_hz/1e6 << " MHz (threshold -70 dBFS)...\n";

    auto resp = client.triggered(
        au::hertz(cf_hz), au::hertz(200e3), au::hertz(200e3),
        /*threshold_dbfs=*/ -70.0,
        /*post_trigger=*/    au::seconds(0.5),
        /*max_captures=*/    1,
        /*duration=*/        au::seconds(30.0)
    );
    if (!resp.accepted) {
        std::cerr << "Rejected: " << resp.reject_reason << "\n";
        return 1;
    }
    int port = resp.streams.empty() ? 0 : resp.streams[0].udp_port;
    std::cout << "Task accepted, listening on port " << port << "...\n";

    sdr::IqStream stream(port, au::seconds(35.0));
    auto iq = stream.collect_complex_for(au::seconds(31.0));
    client.stop(resp.task_id);

    if (iq.empty()) {
        std::cout << "No trigger fired within 30 seconds.\n";
        return 0;
    }
    std::cout << "Captured " << iq.size() << " samples ("
              << iq.size() / 200e3 * 1000 << " ms)\n";

    // Save raw CF32 to file
    std::string fname = "capture.cf32";
    std::ofstream f(fname, std::ios::binary);
    f.write(reinterpret_cast<const char*>(iq.data()),
            iq.size() * sizeof(std::complex<float>));
    std::cout << "Saved to " << fname << " (" << f.tellp() << " bytes)\n";
    return 0;
}

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
