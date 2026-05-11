// Example 3 — TRIGGERED capture: record a burst when signal exceeds threshold.
#include <sdrsdk/sdrsdk.hpp>
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    double cf_hz = (argc > 1) ? std::stod(argv[1]) * 1e6 : 162.4e6;  // NOAA default

    sdr::SdrClient client;
    client.connect();

    std::cout << "Waiting for signal at " << cf_hz/1e6 << " MHz (threshold -70 dBFS)...\n";

    auto resp = client.triggered(
        cf_hz, 200e3, 200e3,   // cf, bw, sr
        /*threshold_dbfs=*/ -70.0,
        /*post_trigger_ms=*/ 500,
        /*max_captures=*/    1,
        /*duration_ms=*/     30000
    );
    if (!resp) {
        std::cerr << "Rejected: " << resp.reject_reason << "\n";
        return 1;
    }
    std::cout << "Task accepted, listening on port " << resp.udp_port() << "...\n";

    sdr::IqStream stream(resp.udp_port(), 35.0);
    auto iq = stream.collect_complex_for(31.0);
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
