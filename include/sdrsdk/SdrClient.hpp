#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  SdrSdk/SdrClient.hpp — AMQP client for the SDR controller task API.
//
//  Types come directly from SdrTaskApi so they match the wire format exactly.
//
//  Usage:
//      sdr::SdrClient client("amqp://localhost:5672");
//      client.connect();
//
//      auto resp = client.narrowband(476.5e6, 2e6, 2e6, 3000);
//      if (resp.accepted) {
//          sdr::IqStream stream(resp.streams[0].udp_port);
//          auto samples = stream.collect_complex(100'000);
//          client.stop(resp.task_id);
//      }
// ═══════════════════════════════════════════════════════════════════════════
#include <sdr/Types.hpp>
#include <stdexcept>
#include <memory>
#include <string>
#include <vector>

namespace sdr {

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

    explicit SdrClient(const std::string& broker   = "amqp://localhost:5672",
                       const std::string& user      = "sdr_ctrl",
                       const std::string& password  = "sdr_hw_test",
                       const std::string& dest_ip   = "127.0.0.1");
    explicit SdrClient(Config cfg);
    ~SdrClient();

    SdrClient(const SdrClient&)            = delete;
    SdrClient& operator=(const SdrClient&) = delete;

    void connect();
    void disconnect();
    bool isConnected() const;

    // Generic submit — accepts any fully-populated TaskRequest.
    TaskResponse submit(const TaskRequest& req);

    // ── Convenience methods (build the TaskRequest for you) ──────────────────

    TaskResponse narrowband(double cf_hz, double bw_hz, double sr_sps,
                            int duration_ms = 5000);

    TaskResponse wideband(double cf_hz, double bw_hz, double sr_sps,
                          int duration_ms = 5000);

    TaskResponse triggered(double cf_hz, double bw_hz, double sr_sps,
                           double threshold_dbfs  = -60.0,
                           int    post_trigger_ms = 200,
                           int    max_captures    = 1,
                           int    duration_ms     = 10000);

    TaskResponse scan(const std::vector<ScanEntry>& entries,
                      bool repeat       = false,
                      int  duration_ms  = 30000);

    TaskResponse snapshot(double cf_hz, double bw_hz, double sr_sps,
                          int fft_size = 1024, int n_averages = 4);

    TaskResponse calibration(double cf_hz, double bw_hz, double sr_sps,
                             int    duration_ms = 2000,
                             const  std::vector<std::string>& devices = {});

    void stop  (const std::string& task_id, const std::string& reason = "");
    void cancel(const std::string& task_id, const std::string& reason = "");

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Config cfg_;
};

} // namespace sdr
