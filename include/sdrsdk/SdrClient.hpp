#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  SdrSdk/SdrClient.hpp
//  High-level AMQP client for the SDR controller task API.
//
//  Usage:
//      sdr::SdrClient client({ .broker = "amqp://localhost:5672" });
//      client.connect();
//
//      auto resp = client.narrowband(476.5e6, 2e6, 2e6, /*duration_ms=*/3000);
//      if (resp) {
//          sdr::IqStream stream(resp.udp_port());
//          auto samples = stream.collect_complex(100'000);
//          client.stop(resp.task_id);
//      }
//      client.disconnect();
// ═══════════════════════════════════════════════════════════════════════════
#include "Types.hpp"
#include <stdexcept>
#include <functional>
#include <atomic>
#include <memory>

namespace sdr {

// Thrown when the broker rejects a task or the connection times out.
struct SdrError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class SdrClient {
public:
    struct Config {
        std::string broker      = "amqp://localhost:5672";
        std::string user        = "sdr_ctrl";
        std::string password    = "sdr_hw_test";
        std::string req_queue   = "sdr.task.request";
        std::string resp_queue  = "sdr.task.response";
        std::string dest_ip     = "127.0.0.1";
        int         timeout_ms  = 20000;
    };

    // Simple constructor: just supply broker URL (and optionally credentials).
    explicit SdrClient(const std::string& broker   = "amqp://localhost:5672",
                       const std::string& user      = "sdr_ctrl",
                       const std::string& password  = "sdr_hw_test",
                       const std::string& dest_ip   = "127.0.0.1");

    // Full-config constructor for advanced use.
    explicit SdrClient(Config cfg);
    ~SdrClient();

    // Non-copyable (owns a proton connection thread)
    SdrClient(const SdrClient&)            = delete;
    SdrClient& operator=(const SdrClient&) = delete;

    // Connect to the broker. Throws SdrError if unreachable after timeout.
    void connect();
    void disconnect();
    bool isConnected() const;

    // ── Generic submit ───────────────────────────────────────────────────────
    // Sends req and waits for the controller's ACCEPTED/REJECTED response.
    // Returns a TaskResponse; check resp.accepted or use if (resp) {...}.
    TaskResponse submit(const TaskRequest& req);

    // ── Convenience overloads ────────────────────────────────────────────────

    // NARROWBAND — streams demodulated IQ to a UDP port
    TaskResponse narrowband(double cf_hz, double bw_hz, double sr_sps,
                            int duration_ms = 5000);

    // WIDEBAND — streams raw IQ (record_raw_iq=true)
    TaskResponse wideband(double cf_hz, double bw_hz, double sr_sps,
                          int duration_ms = 5000);

    // TRIGGERED — captures a burst when power exceeds threshold_dbfs
    TaskResponse triggered(double cf_hz, double bw_hz, double sr_sps,
                           double threshold_dbfs = -60.0,
                           int post_trigger_ms = 200,
                           int max_captures = 1,
                           int duration_ms = 10000);

    // SCAN — steps through a list of frequencies, dwelling at each
    TaskResponse scan(const std::vector<ScanEntry>& entries,
                      bool repeat = false,
                      int duration_ms = 30000);

    // SNAPSHOT — synchronous FFT; response contains task_id when done
    TaskResponse snapshot(double cf_hz, double bw_hz, double sr_sps,
                          int fft_size = 1024, int n_averages = 4);

    // CALIBRATION — multi-channel coherent IQ collection
    TaskResponse calibration(double cf_hz, double bw_hz, double sr_sps,
                             int duration_ms = 2000,
                             const std::vector<std::string>& devices = {});

    // ── Task lifecycle ───────────────────────────────────────────────────────
    void stop  (const std::string& task_id, const std::string& reason = "");
    void cancel(const std::string& task_id, const std::string& reason = "");

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Config cfg_;
};

} // namespace sdr
