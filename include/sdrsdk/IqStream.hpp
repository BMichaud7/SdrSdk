#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  SdrSdk/IqStream.hpp
//  UDP IQ receiver — collects CF32 samples streamed by the controller.
//
//  Usage:
//      sdr::IqStream stream(resp.udp_port());
//      auto samples = stream.collect_complex(100'000);
//      // or for raw interleaved floats:
//      auto raw = stream.collect(n_samples);   // size = n_samples * 2
// ═══════════════════════════════════════════════════════════════════════════
#include "Types.hpp"
#include <vector>
#include <complex>
#include <functional>
#include <atomic>

namespace sdr {

class IqStream {
public:
    // port      — UDP port returned in TaskResponse::udp_port()
    // timeout_s — how long to wait for the first packet before giving up
    explicit IqStream(int port, double timeout_s = 10.0);
    ~IqStream();

    IqStream(const IqStream&)            = delete;
    IqStream& operator=(const IqStream&) = delete;

    // Collect exactly n_samples interleaved I,Q float32 values.
    // Returns when n_samples received or timeout expires (may be fewer).
    std::vector<float> collect(int n_samples);

    // Collect for a fixed wall-clock duration.
    std::vector<float> collect_for(double seconds);

    // Convenience: return as complex<float> (size = n_samples).
    std::vector<std::complex<float>> collect_complex(int n_samples);
    std::vector<std::complex<float>> collect_complex_for(double seconds);

    // Streaming callback — called for each received packet.
    // Return false to stop collection early.
    using PacketCb = std::function<bool(const IqPacketHeader&,
                                        const float* iq,
                                        int n_samples)>;
    void stream(PacketCb cb, double timeout_s = 30.0);

    int  port()         const { return port_; }
    long packets_rx()   const { return pkts_rx_; }
    long samples_rx()   const { return samp_rx_; }
    long overflows()    const { return overflow_; }

private:
    int          port_;
    double       timeout_s_;
    int          fd_ = -1;
    long         pkts_rx_  = 0;
    long         samp_rx_  = 0;
    long         overflow_  = 0;

    void open_socket();
    void close_socket();
};

} // namespace sdr
