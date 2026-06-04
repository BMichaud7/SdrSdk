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
 * @file SdrClient.hpp
 * @brief High-level AMQP client for submitting IQ capture tasks to SdrResourceManager.
 *
 * SdrClient abstracts the full task lifecycle: AMQP connection, TaskRequest
 * serialisation, response parsing, and task teardown.  Physical quantities use
 * the Au units library for compile-time unit safety — passing Hz where seconds
 * are expected is a compile error, not a runtime bug.
 *
 * @par Quick start
 * @code{.cpp}
 * sdr::SdrClient client("amqp://localhost:5672");
 * client.connect();
 * auto resp = client.narrowband(
 *     sdrunit::MHz(462.5), sdrunit::MHz(12.5), sdrunit::MHz(48), sdrunit::s(3.0));
 * if (resp.accepted) {
 *     sdr::IqStream stream(resp.streams[0].udp_port);
 *     auto iq = stream.collect_complex_for(sdrunit::s(3.0));
 *     client.stop(resp.task_id);
 * }
 * @endcode
 *
 * @see IqStream, Spectrum, units.hpp
 */
#pragma once
#include <sdr/Types.hpp>
#include "sdrsdk/units.hpp"
#include <stdexcept>
#include <memory>
#include <string>
#include <vector>

namespace sdr {

/// @brief Exception thrown by SdrClient when the task request fails or times out.
struct SdrError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/**
 * @class SdrClient
 * @brief C++ client for the SdrResourceManager task API.
 *
 * Manages an AMQP connection, serialises task requests using SdrTaskApi types,
 * blocks until the response arrives, and returns a @c TaskResponse.
 *
 * @note Non-copyable (owns an AMQP connection).  Movable.
 * @note connect() must be called before any task method.
 */
class SdrClient {
public:
    /// @brief Full connection and task configuration.
    struct Config {
        std::string broker      = "amqp://localhost:5672"; ///< AMQP broker URL.
        std::string user        = "sdr_ctrl";              ///< AMQP username.
        std::string password    = "sdr_hw_test";           ///< AMQP password.
        std::string req_queue   = "sdr.task.request";      ///< Outbound task request queue.
        std::string resp_queue  = "sdr.task.response";     ///< Inbound task response queue.
        std::string dest_ip     = "127.0.0.1";             ///< Local IP for UDP IQ receive.
        au::QuantityD<au::Seconds> timeout = sdrunit::s(20.0); ///< Response wait timeout.
    };

    /**
     * @brief Construct with individual connection parameters.
     * @param broker  AMQP broker URL.
     * @param user    AMQP username.
     * @param password AMQP password.
     * @param dest_ip Local IP that SdrResourceManager streams IQ to.
     */
    explicit SdrClient(const std::string& broker   = "amqp://localhost:5672",
                       const std::string& user      = "sdr_ctrl",
                       const std::string& password  = "sdr_hw_test",
                       const std::string& dest_ip   = "127.0.0.1");
    /// @brief Construct from a full Config struct.
    explicit SdrClient(Config cfg);
    ~SdrClient();

    SdrClient(const SdrClient&)            = delete;
    SdrClient& operator=(const SdrClient&) = delete;

    /// @brief Open the AMQP connection. Must be called before any task method.
    /// @throws SdrError on connection failure or timeout.
    void connect();

    /// @brief Close the AMQP connection gracefully.
    void disconnect();

    /// @brief Return true if the AMQP connection is currently open.
    bool isConnected() const;

    /**
     * @brief Submit a fully-populated TaskRequest and wait for the response.
     * @param req Pre-built task request.
     * @return TaskResponse — check @c accepted before using streams[].
     */
    TaskResponse submit(const TaskRequest& req);

    // ── Convenience task methods ──────────────────────────────────────────────

    /**
     * @brief Request a fixed-duration IQ capture at a single frequency.
     * @param cf       Centre frequency.
     * @param bw       Requested bandwidth.
     * @param sr       Sample rate.
     * @param duration Capture duration.
     * @return TaskResponse with a single UDP port in @c streams[0].
     */
    TaskResponse narrowband(au::QuantityD<au::Hertz>   cf,
                            au::QuantityD<au::Hertz>   bw,
                            au::QuantityD<au::Hertz>   sr,
                            au::QuantityD<au::Seconds> duration = sdrunit::s(5.0));

    /**
     * @brief Request a continuous IQ stream at a single frequency.
     *        Stream continues until @c stop() is called.
     */
    TaskResponse wideband(au::QuantityD<au::Hertz>   cf,
                          au::QuantityD<au::Hertz>   bw,
                          au::QuantityD<au::Hertz>   sr,
                          au::QuantityD<au::Seconds> duration = sdrunit::s(5.0));

    /**
     * @brief Wait for received power to exceed @p threshold_dbfs, then capture.
     * @param threshold_dbfs Power level (dBFS) that triggers capture.
     * @param post_trigger   Capture duration after threshold is crossed.
     * @param max_captures   Number of trigger events before task ends.
     */
    TaskResponse triggered(au::QuantityD<au::Hertz>   cf,
                           au::QuantityD<au::Hertz>   bw,
                           au::QuantityD<au::Hertz>   sr,
                           double                     threshold_dbfs = -60.0,
                           au::QuantityD<au::Seconds> post_trigger   = sdrunit::s(0.2),
                           int                        max_captures   = 1,
                           au::QuantityD<au::Seconds> duration       = sdrunit::s(10.0));

    /**
     * @brief Submit a multi-step frequency sweep task.
     * @param entries  List of (frequency, bandwidth, sample rate) steps.
     * @param repeat   If true, the sweep repeats until @c stop() is called.
     */
    TaskResponse scan(const std::vector<ScanEntry>& entries,
                      bool                       repeat   = false,
                      au::QuantityD<au::Seconds> duration = sdrunit::s(30.0));

    /**
     * @brief Request a synchronous FFT snapshot (Welch-averaged PSD).
     * @param fft_size   FFT frame size (power of 2).
     * @param n_averages Number of frames to average.
     * @return TaskResponse — result available immediately via IqStream.
     */
    TaskResponse snapshot(au::QuantityD<au::Hertz> cf,
                          au::QuantityD<au::Hertz> bw,
                          au::QuantityD<au::Hertz> sr,
                          int fft_size = 1024, int n_averages = 4);

    /// @brief Request a calibration capture (noise floor measurement).
    TaskResponse calibration(au::QuantityD<au::Hertz>   cf,
                             au::QuantityD<au::Hertz>   bw,
                             au::QuantityD<au::Hertz>   sr,
                             au::QuantityD<au::Seconds> duration = sdrunit::s(2.0),
                             const std::vector<std::string>& devices = {});

    /// @brief Stop a running task and release its UDP port.
    /// @param task_id Task ID from @c TaskResponse::task_id.
    /// @param reason  Optional human-readable stop reason (logged by ResourceManager).
    void stop(const std::string& task_id, const std::string& reason = "");

    /// @brief Cancel a pending or running task.
    void cancel(const std::string& task_id, const std::string& reason = "");

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Config cfg_;
};

} // namespace sdr

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
