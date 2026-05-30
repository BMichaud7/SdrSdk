#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  SdrSdk/IqStream.hpp — UDP IQ receiver.
//  IqPacketHeader and IQ_PACKET_MAGIC come from SdrTaskApi.
// ═══════════════════════════════════════════════════════════════════════════
#include <sdr/Types.hpp>
#include "sdrsdk/units.hpp"
#include <vector>
#include <complex>
#include <functional>

namespace sdr {

class IqStream {
public:
    explicit IqStream(int port,
                      au::QuantityD<au::Seconds> timeout = sdrunit::s(10.0));
    ~IqStream();

    IqStream(const IqStream&)            = delete;
    IqStream& operator=(const IqStream&) = delete;

    std::vector<float>                collect(int n_samples);
    std::vector<float>                collect_for(au::QuantityD<au::Seconds> duration);
    std::vector<std::complex<float>>  collect_complex(int n_samples);
    std::vector<std::complex<float>>  collect_complex_for(au::QuantityD<au::Seconds> duration);

    using PacketCb = std::function<bool(const IqPacketHeader&,
                                        const float* iq, int n_samples)>;
    void stream(PacketCb cb,
                au::QuantityD<au::Seconds> timeout = sdrunit::s(30.0));

    int  port()       const { return port_; }
    long packets_rx() const { return pkts_rx_; }
    long samples_rx() const { return samp_rx_; }
    long overflows()  const { return overflow_; }

private:
    int   port_;
    double timeout_s_;
    int   fd_       = -1;
    long  pkts_rx_  = 0;
    long  samp_rx_  = 0;
    long  overflow_  = 0;

    void open_socket();
    void close_socket();
};

} // namespace sdr
