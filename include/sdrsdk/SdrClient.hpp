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
//      using namespace sdrunit;
//      auto resp = client.narrowband(MHz(476.5), MHz(2.0), MHz(2.0), s(3.0));
//      if (resp.accepted) {
//          sdr::IqStream stream(resp.streams[0].udp_port);
//          auto samples = stream.collect_complex(100'000);
//          client.stop(resp.task_id);
//      }
// ═══════════════════════════════════════════════════════════════════════════
#include <sdr/Types.hpp>
#include "sdrsdk/units.hpp"
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
        au::QuantityD<au::Seconds> timeout = sdrunit::s(20.0);
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

    TaskResponse narrowband(au::QuantityD<au::Hertz> cf,
                            au::QuantityD<au::Hertz> bw,
                            au::QuantityD<au::Hertz> sr,
                            au::QuantityD<au::Seconds> duration = sdrunit::s(5.0));

    TaskResponse wideband(au::QuantityD<au::Hertz> cf,
                          au::QuantityD<au::Hertz> bw,
                          au::QuantityD<au::Hertz> sr,
                          au::QuantityD<au::Seconds> duration = sdrunit::s(5.0));

    TaskResponse triggered(au::QuantityD<au::Hertz>   cf,
                           au::QuantityD<au::Hertz>   bw,
                           au::QuantityD<au::Hertz>   sr,
                           double                     threshold_dbfs  = -60.0,
                           au::QuantityD<au::Seconds> post_trigger    = sdrunit::s(0.2),
                           int                        max_captures    = 1,
                           au::QuantityD<au::Seconds> duration        = sdrunit::s(10.0));

    TaskResponse scan(const std::vector<ScanEntry>& entries,
                      bool                       repeat   = false,
                      au::QuantityD<au::Seconds> duration = sdrunit::s(30.0));

    TaskResponse snapshot(au::QuantityD<au::Hertz> cf,
                          au::QuantityD<au::Hertz> bw,
                          au::QuantityD<au::Hertz> sr,
                          int fft_size = 1024, int n_averages = 4);

    TaskResponse calibration(au::QuantityD<au::Hertz>   cf,
                             au::QuantityD<au::Hertz>   bw,
                             au::QuantityD<au::Hertz>   sr,
                             au::QuantityD<au::Seconds> duration = sdrunit::s(2.0),
                             const std::vector<std::string>& devices = {});

    void stop  (const std::string& task_id, const std::string& reason = "");
    void cancel(const std::string& task_id, const std::string& reason = "");

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Config cfg_;
};

} // namespace sdr
