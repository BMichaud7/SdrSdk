#include "sdrsdk/SdrClient.hpp"
#include <nlohmann/json.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/container.hpp>
#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/sender.hpp>
#include <proton/sender_options.hpp>
#include <proton/receiver.hpp>
#include <proton/receiver_options.hpp>
#include <proton/source_options.hpp>
#include <proton/target_options.hpp>
#include <proton/message.hpp>
#include <proton/work_queue.hpp>
#include <proton/symbol.hpp>
#include <proton/transport.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <deque>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <atomic>

using json = nlohmann::json;
using namespace std::chrono;

// ── UUID helper ───────────────────────────────────────────────────────────────
static std::string make_uuid() {
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> d;
    auto a = d(rng), b = d(rng);
    std::ostringstream s;
    s << std::hex << std::setfill('0')
      << std::setw(8)  << (a >> 32) << '-'
      << std::setw(4)  << ((a >> 16) & 0xffff) << '-'
      << std::setw(4)  << (a & 0xffff) << '-'
      << std::setw(4)  << (b >> 48) << '-'
      << std::setw(12) << (b & 0xffffffffffff);
    return s.str();
}

static int64_t now_ms() {
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

// ── JSON encoding helpers ─────────────────────────────────────────────────────
static json encode_request(const sdr::TaskRequest& req, const std::string& rid,
                            const std::string& dest_ip) {
    auto mode_str = [](sdr::ScheduleMode m) -> std::string {
        switch (m) {
            case sdr::ScheduleMode::IMMEDIATE:  return "IMMEDIATE";
            case sdr::ScheduleMode::SCHEDULED:  return "SCHEDULED";
            case sdr::ScheduleMode::CONTINUOUS: return "CONTINUOUS";
        }
        return "IMMEDIATE";
    };
    auto type_str = [](sdr::TaskType t) -> std::string {
        switch (t) {
            case sdr::TaskType::NARROWBAND:  return "NARROWBAND";
            case sdr::TaskType::WIDEBAND:    return "WIDEBAND";
            case sdr::TaskType::TRIGGERED:   return "TRIGGERED";
            case sdr::TaskType::SCAN:        return "SCAN";
            case sdr::TaskType::SNAPSHOT:    return "SNAPSHOT";
            case sdr::TaskType::CALIBRATION: return "CALIBRATION";
            case sdr::TaskType::DF:          return "DF";
            default:                          return "NARROWBAND";
        }
    };

    std::string msg_type = "TASK_REQUEST";
    if (req.task_type == sdr::TaskType::SNAPSHOT)   msg_type = "TASK_REQUEST_SNAPSHOT";
    if (req.task_type == sdr::TaskType::SCAN)        msg_type = "TASK_REQUEST_SCAN";
    if (req.task_type == sdr::TaskType::CALIBRATION) msg_type = "TASK_REQUEST_CALIBRATION";

    json sched;
    sched["mode"] = mode_str(req.schedule.mode);
    if (req.schedule.mode == sdr::ScheduleMode::IMMEDIATE)
        sched["duration_ms"] = req.schedule.duration_ms;
    else if (req.schedule.mode == sdr::ScheduleMode::SCHEDULED) {
        sched["start_time_epoch_ms"] = req.schedule.start_epoch_ms;
        sched["end_time_epoch_ms"]   = req.schedule.end_epoch_ms;
    }

    json j = {
        {"msg_type",       msg_type},
        {"schema_version", "2.0"},
        {"request_id",     rid},
        {"timestamp_ms",   now_ms()},
        {"task_type",      type_str(req.task_type)},
        {"rank",           req.rank},
        {"schedule",       sched},
        {"rf", {
            {"center_freq_hz",  req.rf.center_freq_hz},
            {"bandwidth_hz",    req.rf.bandwidth_hz},
            {"sample_rate_sps", req.rf.sample_rate_sps},
            {"rx_count",        req.rf.rx_count},
        }},
        {"streaming", {{"dest_ip", dest_ip}}},
    };

    if (req.task_type == sdr::TaskType::WIDEBAND)
        j["wideband"] = {{"record_raw_iq", true}, {"fft_size", 2048}};

    if (req.task_type == sdr::TaskType::SNAPSHOT)
        j["snapshot"] = {
            {"center_freq_hz",  req.rf.center_freq_hz},
            {"bandwidth_hz",    req.rf.bandwidth_hz},
            {"sample_rate_sps", req.rf.sample_rate_sps},
            {"fft_size",        req.fft_size},
            {"n_averages",      req.n_averages},
        };

    if (req.trigger) {
        j["trigger"] = {
            {"trigger_type",     req.trigger->trigger_type},
            {"threshold_dbfs",   req.trigger->threshold_dbfs},
            {"pre_trigger_ms",   req.trigger->pre_trigger_ms},
            {"post_trigger_ms",  req.trigger->post_trigger_ms},
            {"max_captures",     req.trigger->max_captures},
        };
    }

    if (req.scan) {
        json entries = json::array();
        for (const auto& e : req.scan->entries)
            entries.push_back({
                {"step",            e.step},
                {"center_freq_hz",  e.center_freq_hz},
                {"bandwidth_hz",    e.bandwidth_hz},
                {"sample_rate_sps", e.sample_rate_sps},
                {"dwell_ms",        e.dwell_ms},
            });
        j["scan_params"] = {{"repeat", req.scan->repeat}, {"entries", entries}};
    }

    return j;
}

static sdr::TaskResponse decode_response(const json& j) {
    sdr::TaskResponse resp;
    resp.task_id      = j.value("task_id", "");
    resp.status       = j.value("status", "");
    resp.accepted     = (resp.status == "ACCEPTED") || j.value("accepted", false);
    resp.reject_reason= j.value("reject_reason", "");

    for (const auto& s : j.value("streams", json::array()))
        resp.streams.push_back({s.value("udp_port", 0), s.value("stream_id", "")});

    return resp;
}

// ── AMQP implementation ───────────────────────────────────────────────────────

namespace sdr {

struct SdrClient::Impl : proton::messaging_handler {
    Config cfg;

    proton::container  container_{*this};
    std::thread        loop_thread_;

    proton::connection conn_;
    proton::sender     sender_;
    proton::receiver   receiver_;
    std::atomic<bool>  connected_{false};

    std::mutex              conn_mu_;
    std::condition_variable conn_cv_;

    std::mutex              out_mu_;
    std::deque<std::string> out_q_;

    struct Waiter {
        std::mutex              mu;
        std::condition_variable cv;
        std::string             body;
        bool                    done = false;
    };
    std::mutex                                               waiters_mu_;
    std::unordered_map<std::string, std::shared_ptr<Waiter>> waiters_;

    // ── proton callbacks ─────────────────────────────────────────────────────
    void on_container_start(proton::container& c) override {
        proton::connection_options co;
        co.user(cfg.user).password(cfg.password)
          .sasl_enabled(true).sasl_allow_insecure_mechs(true);
        conn_ = c.connect(cfg.broker, co);

        sender_ = conn_.open_sender(cfg.req_queue,
            proton::sender_options().target(
                proton::target_options().capabilities({proton::symbol("queue")})));

        receiver_ = conn_.open_receiver(cfg.resp_queue,
            proton::receiver_options().source(
                proton::source_options().capabilities({proton::symbol("queue")})));
    }

    void flush() {
        std::lock_guard<std::mutex> g(out_mu_);
        while (!out_q_.empty() && sender_.credit() > 0) {
            proton::message m;
            m.body(out_q_.front());
            m.content_type("application/json");
            sender_.send(m);
            out_q_.pop_front();
        }
    }

    void on_sendable(proton::sender&) override {
        flush();
        if (!connected_.exchange(true))
            conn_cv_.notify_all();
    }

    void on_message(proton::delivery&, proton::message& msg) override {
        try {
            std::string body;
            auto v = msg.body();
            if (v.type() == proton::STRING)
                body = proton::get<std::string>(v);
            else if (v.type() == proton::BINARY) {
                auto b = proton::get<proton::binary>(v);
                body.assign(b.begin(), b.end());
            } else return;

            auto j   = json::parse(body);
            auto rid = j.value("request_id", std::string{});
            std::lock_guard<std::mutex> g(waiters_mu_);
            auto it = waiters_.find(rid);
            if (it != waiters_.end()) {
                std::lock_guard<std::mutex> wg(it->second->mu);
                it->second->body = std::move(body);
                it->second->done = true;
                it->second->cv.notify_one();
            }
        } catch (...) {}
    }

    void on_connection_error(proton::connection&) override { connected_ = false; }
    void on_transport_error (proton::transport&)  override { connected_ = false; }

    // ── Thread-safe send ─────────────────────────────────────────────────────
    void enqueue(const json& j) {
        std::string body = j.dump();
        {
            std::lock_guard<std::mutex> g(out_mu_);
            out_q_.push_back(std::move(body));
        }
        if (connected_.load())
            conn_.work_queue().schedule(proton::duration(0), [this]{ flush(); });
    }

    json rpc(const json& req, int timeout_ms) {
        std::string rid = req.value("request_id", make_uuid());
        auto waiter = std::make_shared<Waiter>();
        {
            std::lock_guard<std::mutex> g(waiters_mu_);
            waiters_[rid] = waiter;
        }
        enqueue(req);
        auto deadline = steady_clock::now() + milliseconds(timeout_ms);
        {
            std::unique_lock<std::mutex> lk(waiter->mu);
            waiter->cv.wait_until(lk, deadline, [&]{ return waiter->done; });
        }
        {
            std::lock_guard<std::mutex> g(waiters_mu_);
            waiters_.erase(rid);
        }
        if (!waiter->done) return {};
        return json::parse(waiter->body);
    }
};

// ── SdrClient public API ──────────────────────────────────────────────────────

SdrClient::SdrClient(const std::string& broker, const std::string& user,
                     const std::string& password, const std::string& dest_ip)
    : SdrClient(Config{broker, user, password,
                       "sdr.task.request", "sdr.task.response",
                       dest_ip, 20000}) {}

SdrClient::SdrClient(Config cfg)
    : impl_(std::make_unique<Impl>()), cfg_(std::move(cfg))
{
    impl_->cfg = cfg_;
}

SdrClient::~SdrClient() { disconnect(); }

void SdrClient::connect() {
    impl_->loop_thread_ = std::thread([this]{ impl_->container_.run(); });
    std::unique_lock<std::mutex> lk(impl_->conn_mu_);
    if (!impl_->conn_cv_.wait_for(lk, milliseconds(cfg_.timeout_ms),
                                   [this]{ return impl_->connected_.load(); }))
        throw SdrError("Cannot connect to broker at " + cfg_.broker);
}

void SdrClient::disconnect() {
    if (impl_->loop_thread_.joinable()) {
        impl_->container_.stop();
        impl_->loop_thread_.join();
    }
}

bool SdrClient::isConnected() const { return impl_->connected_.load(); }

TaskResponse SdrClient::submit(const TaskRequest& req) {
    std::string rid = make_uuid();
    json j = encode_request(req, rid, cfg_.dest_ip);
    json resp = impl_->rpc(j, cfg_.timeout_ms);
    if (resp.is_null())
        throw SdrError("No response from controller (timeout or disconnect)");
    return decode_response(resp);
}

// ── Convenience overloads ─────────────────────────────────────────────────────

TaskResponse SdrClient::narrowband(double cf, double bw, double sr, int dur_ms) {
    TaskRequest req;
    req.task_type    = TaskType::NARROWBAND;
    req.rf           = {cf, bw, sr};
    req.schedule     = {ScheduleMode::IMMEDIATE, dur_ms};
    return submit(req);
}

TaskResponse SdrClient::wideband(double cf, double bw, double sr, int dur_ms) {
    TaskRequest req;
    req.task_type = TaskType::WIDEBAND;
    req.rf        = {cf, bw, sr};
    req.schedule  = {ScheduleMode::IMMEDIATE, dur_ms};
    return submit(req);
}

TaskResponse SdrClient::triggered(double cf, double bw, double sr,
                                   double thr, int post_ms, int max_cap,
                                   int dur_ms) {
    TaskRequest req;
    req.task_type = TaskType::TRIGGERED;
    req.rf        = {cf, bw, sr};
    req.schedule  = {ScheduleMode::IMMEDIATE, dur_ms};
    req.trigger   = TriggerParams{"POWER_THRESHOLD", thr, 50, post_ms, max_cap};
    return submit(req);
}

TaskResponse SdrClient::scan(const std::vector<ScanEntry>& entries,
                              bool repeat, int dur_ms) {
    TaskRequest req;
    req.task_type   = TaskType::SCAN;
    req.rf          = {entries.empty() ? 0.0 : entries[0].center_freq_hz,
                       entries.empty() ? 10e6 : entries[0].bandwidth_hz,
                       entries.empty() ? 10e6 : entries[0].sample_rate_sps};
    req.schedule    = {ScheduleMode::IMMEDIATE, dur_ms};
    req.scan        = ScanParams{repeat, entries};
    return submit(req);
}

TaskResponse SdrClient::snapshot(double cf, double bw, double sr,
                                  int fft_size, int n_avg) {
    TaskRequest req;
    req.task_type = TaskType::SNAPSHOT;
    req.rf        = {cf, bw, sr};
    req.fft_size  = fft_size;
    req.n_averages= n_avg;
    return submit(req);
}

TaskResponse SdrClient::calibration(double cf, double bw, double sr,
                                     int dur_ms,
                                     const std::vector<std::string>& /*devices*/) {
    TaskRequest req;
    req.task_type = TaskType::CALIBRATION;
    req.rf        = {cf, bw, sr};
    req.schedule  = {ScheduleMode::IMMEDIATE, dur_ms};
    return submit(req);
}

void SdrClient::stop(const std::string& task_id, const std::string& reason) {
    impl_->enqueue({
        {"msg_type",    "TASK_STOP"},
        {"request_id",  make_uuid()},
        {"task_id",     task_id},
        {"timestamp_ms", now_ms()},
        {"reason",      reason.empty() ? "client stop" : reason},
    });
}

void SdrClient::cancel(const std::string& task_id, const std::string& reason) {
    impl_->enqueue({
        {"msg_type",    "TASK_CANCEL"},
        {"request_id",  make_uuid()},
        {"task_id",     task_id},
        {"timestamp_ms", now_ms()},
        {"reason",      reason.empty() ? "client cancel" : reason},
    });
}

} // namespace sdr
