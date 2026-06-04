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
 * @file IqStream.hpp
 * @brief UDP IQ receiver — collects CF32 samples from SdrResourceManager.
 *
 * IqStream binds a UDP socket on the port assigned by SdrResourceManager
 * (returned in @c TaskResponse::streams[0].udp_port) and provides blocking
 * collect methods that accumulate IQ packets until the requested sample count
 * or duration is reached.
 *
 * @par Example
 * @code{.cpp}
 * sdr::IqStream stream(resp.streams[0].udp_port);
 * auto iq = stream.collect_complex_for(sdrunit::s(3.0)); // blocks 3 seconds
 * // iq is std::vector<std::complex<float>>
 * @endcode
 *
 * @see SdrClient, Spectrum
 */
#pragma once
#include <sdr/Types.hpp>
#include "sdrsdk/units.hpp"
#include <vector>
#include <complex>
#include <functional>

namespace sdr {

/**
 * @class IqStream
 * @brief UDP receiver for CF32 IQ packets from SdrResourceManager.
 *
 * Non-copyable.  Owns a bound UDP socket for the lifetime of the object.
 * All @c collect* methods block until the requested data is received or
 * the internal timeout expires.
 */
class IqStream {
public:
    /**
     * @brief Bind a UDP socket on @p port.
     * @param port    UDP port assigned by SdrResourceManager (from TaskResponse).
     * @param timeout Maximum wait per packet before a receive error is raised.
     * @throws std::runtime_error if the socket cannot be bound.
     */
    explicit IqStream(int port,
                      au::QuantityD<au::Seconds> timeout = sdrunit::s(10.0));
    ~IqStream();

    IqStream(const IqStream&)            = delete;
    IqStream& operator=(const IqStream&) = delete;

    /// @brief Collect @p n_samples interleaved float32 I,Q,I,Q,… samples.
    std::vector<float>                collect(int n_samples);

    /// @brief Collect interleaved samples for a fixed duration (blocking).
    std::vector<float>                collect_for(au::QuantityD<au::Seconds> duration);

    /// @brief Collect @p n_samples complex<float> samples.
    std::vector<std::complex<float>>  collect_complex(int n_samples);

    /// @brief Collect complex<float> samples for a fixed duration (blocking).
    std::vector<std::complex<float>>  collect_complex_for(au::QuantityD<au::Seconds> duration);

    /**
     * @brief Stream packets to a callback until the callback returns false or timeout.
     * @param cb      Called for each received packet. Return false to stop.
     * @param timeout Overall receive timeout.
     */
    using PacketCb = std::function<bool(const IqPacketHeader&,
                                        const float* iq, int n_samples)>;
    void stream(PacketCb cb,
                au::QuantityD<au::Seconds> timeout = sdrunit::s(30.0));

    int  port()       const { return port_; }    ///< Bound UDP port.
    long packets_rx() const { return pkts_rx_; } ///< Total packets received.
    long samples_rx() const { return samp_rx_; } ///< Total IQ samples received.
    long overflows()  const { return overflow_; }///< Overflow packet count.

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

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
