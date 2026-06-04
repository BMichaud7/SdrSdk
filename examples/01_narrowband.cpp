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
// Example 1 — NARROWBAND task: tune, collect IQ, print stats.
#include <sdrsdk/sdrsdk.hpp>
#include <au/units/hertz.hh>
#include <au/units/seconds.hh>
#include <iostream>
#include <cmath>

int main() {
    sdr::SdrClient client("amqp://localhost:5672");

    std::cout << "Connecting to broker...\n";
    client.connect();

    // Tune to 476.5 MHz, 2 MHz BW, collect for 3 seconds
    auto resp = client.narrowband(au::hertz(476.5e6), au::hertz(2e6),
                                  au::hertz(2e6), au::seconds(3.0));
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

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
