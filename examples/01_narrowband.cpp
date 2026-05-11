// Example 1 — NARROWBAND task: tune, collect IQ, print stats.
#include <sdrsdk/sdrsdk.hpp>
#include <iostream>
#include <cmath>

int main() {
    sdr::SdrClient client("amqp://localhost:5672");

    std::cout << "Connecting to broker...\n";
    client.connect();

    // Tune to 476.5 MHz, 2 MHz BW, collect for 3 seconds
    auto resp = client.narrowband(476.5e6, 2e6, 2e6, 3000);
    if (!resp.accepted) {
        std::cerr << "Rejected: " << resp.reject_reason << "\n";
        return 1;
    }
    int port = resp.streams.empty() ? 0 : resp.streams[0].udp_port;
    std::cout << "Task " << resp.task_id << " accepted on port " << port << "\n";

    // Receive IQ
    sdr::IqStream stream(port);
    auto samples = stream.collect_complex(200'000);
    client.stop(resp.task_id);

    // Compute RMS power
    double pwr = 0;
    for (auto& s : samples) pwr += std::norm(s);
    pwr = 10.0 * std::log10(pwr / samples.size() + 1e-30);

    std::cout << "Collected " << samples.size() << " samples\n"
              << "Mean power: " << pwr << " dBFS\n"
              << "Packets:    " << stream.packets_rx() << "\n";
    return 0;
}
