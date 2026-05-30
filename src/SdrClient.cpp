#include "sdrsdk/SdrClient.hpp"
#include <sdr/MessageCodec.hpp>
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

// ── JSON encoding for TaskRequest (client → controller) ──────────────────────
// MessageCodec::decode() handles the reverse (controller parsing incoming).
static json encode_request(const sdr::TaskRequest& req, const std::string& dest_ip) {
    // msg_type varies by task type
    std::string msg_type = "TASK_REQUEST";
    if (req.task_type == sdr::TaskType::SNAPSHOT)   msg_type = "TASK_REQUEST_SNAPSHOT";
    if (req.task_type == sdr::TaskType::SCAN)        msg_type = "TASK_REQUEST_SCAN";
    if (req.task_type == sdr::TaskType::CALIBRATION) msg_type = "TASK_REQUEST_CALIBRATION";

    json sched;
    sched["mode"] = sdr::scheduleModeToString(req.schedule_mode);
    if (req.schedule_mode == sdr::ScheduleMode::IMMEDIATE)
        sched["duration_ms"] = req.duration_ms;
    else if (req.schedule_mode == sdr::ScheduleMode::SCHEDULED) {
        sched["start_time_epoch_ms"] = req.start_time_ms;
        sched["end_time_epoch_ms"]   = req.end_time_ms;
    }

    json j = {
        {"msg_type",       msg_type},
        {"schema_version", sdr::SCHEMA_VERSION},
        {"request_id",     req.request_id},
        {"timestamp_ms",   now_ms()},
        {"task_type",      sdr::taskTypeToString(req.task_type)},
        {"rank",           req.rank},
        {"schedule",       sched},
        {"rf", {
            {"center_freq_hz",  req.rf.center_freq_hz},
            {"bandwidth_hz",    req.rf.bandwidth_hz},
            {"sample_rate_sps", req.rf.sample_rate_sps},
            {"rx_count",        req.rf.rx_count},
        }},
        {"streaming", {{"dest_ip", req.streaming.dest_ip.empty() ? dest_ip : req.streaming.dest_ip}}},
    };

    if (req.task_type == sdr::TaskType::WIDEBAND && req.wb_params)
        j["wideband"] = {{"record_raw_iq", req.wb_params->record_raw_iq},
                         {"fft_size", req.wb_params->fft_size}};
    else if (req.task_type == sdr::TaskType::WIDEBAND)
        j["wideband"] = {{"record_raw_iq", true}, {"fft_size", 2048}};

    if (req.snapshot_params) {
        j["snapshot"] = {
            {"center_freq_hz",  req.rf.center_freq_hz},
            {"bandwidth_hz",    req.rf.bandwidth_hz},
            {"sample_rate_sps", req.rf.sample_rate_sps},
            {"fft_size",        req.snapshot_params->fft_size},
            {"n_averages",      req.snapshot_params->n_averages},
        };
    }

    if (req.trigger_params) {
        auto& t = *req.trigger_params;
        j["trigger"] = {
            {"trigger_type",    t.trigger_type},
            {"threshold_dbfs",  t.threshold_dbfs},
            {"pre_trigger_ms",  t.pre_trigger_ms},
            {"post_trigger_ms", t.post_trigger_ms},
            {"max_captures",    t.max_captures},
        };
    }

    if (req.scan_params) {
        json entries = json::array();
        for (auto& e : req.scan_params->entries)
            entries.push_back({
                {"step",            e.step},
                {"center_freq_hz",  e.center_freq_hz},
                {"bandwidth_hz",    e.bandwidth_hz},
                {"sample_rate_sps", e.sample_rate_sps},
                {"dwell_ms",        e.dwell_ms},
            });
        j["scan_params"] = {{"repeat", req.scan_params->repeat}, {"entries", entries}};
    }

    if (req.cal_params) {
        auto& c = *req.cal_params;
        j["calibration"] = {
            {"center_freq_hz",      req.rf.center_freq_hz},
            {"bandwidth_hz",        req.rf.bandwidth_hz},
            {"sample_rate_sps",     req.rf.sample_rate_sps},
            {"duration_ms",         req.duration_ms},
            {"rx_count_per_device", req.rf.rx_count},
            {"coherency_group",     c.coherency_group},
        };
    }

    return j;
}

// ── Decode controller response → TaskResponse ─────────────────────────────────
static sdr::TaskResponse decode_response(const json& j) {
    sdr::TaskResponse resp;
    resp.request_id    = j.value("request_id", "");
    resp.task_id       = j.value("task_id", "");
    resp.accepted      = (j.value("status", "") == "ACCEPTED") || j.value("accepted", false);
    resp.reject_reason = j.value("reject_reason", "");
    auto rc = j.value("reject_code", std::string{});
    // Map common reject codes
    if      (rc == "FREQ_OUT_OF_RANGE")  resp.reject_code = sdr::RejectCode::FREQ_OUT_OF_RANGE;
    else if (rc == "TASK_LIMIT_REACHED") resp.reject_code = sdr::RejectCode::TASK_LIMIT_REACHED;
    else if (rc == "PORT_POOL_EXHAUSTED")resp.reject_code = sdr::RejectCode::PORT_POOL_EXHAUSTED;

    for (const auto& s : j.value("streams", json::array())) {
        sdr::AssignedStream as;
        as.udp_port    = s.value("udp_port", 0);
        as.stream_id   = s.value("stream_id", "");
        as.udp_ip      = s.value("dest_ip", "");
        resp.streams.push_back(std::move(as));
    }
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

    void on_connection_open(proton::connection&) override { flush(); }

    void on_message(proton::delivery&, proton::message& msg) override {
        try {
            std::string body;
            auto v = msg.body();
            if      (v.type() == proton::STRING) body = proton::get<std::string>(v);
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

    void enqueue(const json& j) {
        std::string body = j.dump();
        { std::lock_guard<std::mutex> g(out_mu_); out_q_.push_back(std::move(body)); }
        if (connected_.load())
            conn_.work_queue().schedule(proton::duration(0), [this]{ flush(); });
    }

    json rpc(const json& req, int timeout_ms) {
        std::string rid = req.value("request_id", make_uuid());
        auto waiter = std::make_shared<Waiter>();
        { std::lock_guard<std::mutex> g(waiters_mu_); waiters_[rid] = waiter; }
        enqueue(req);
        auto deadline = steady_clock::now() + milliseconds(timeout_ms);
        {
            std::unique_lock<std::mutex> lk(waiter->mu);
            waiter->cv.wait_until(lk, deadline, [&]{ return waiter->done; });
        }
        { std::lock_guard<std::mutex> g(waiters_mu_); waiters_.erase(rid); }
        if (!waiter->done) return {};
        return json::parse(waiter->body);
    }
};

// ── SdrClient public API ──────────────────────────────────────────────────────

SdrClient::SdrClient(const std::string& broker, const std::string& user,
                     const std::string& password, const std::string& dest_ip)
    : SdrClient(Config{broker, user, password,
                       "sdr.task.request", "sdr.task.response", dest_ip, 20000}) {}

SdrClient::SdrClient(Config cfg)
    : impl_(std::make_unique<Impl>()), cfg_(std::move(cfg))
{ impl_->cfg = cfg_; }

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
    json j = encode_request(req, cfg_.dest_ip);
    json resp = impl_->rpc(j, cfg_.timeout_ms);
    if (resp.is_null())
        throw SdrError("No response from controller (timeout or disconnect)");
    return decode_response(resp);
}

// ── Convenience methods ───────────────────────────────────────────────────────

TaskResponse SdrClient::narrowband(au::QuantityD<au::Hertz> cf,
                                    au::QuantityD<au::Hertz> bw,
                                    au::QuantityD<au::Hertz> sr,
                                    au::QuantityD<au::Seconds> duration) {
    double cf_hz  = cf.in(au::hertz);
    double bw_hz  = bw.in(au::hertz);
    double sr_hz  = sr.in(au::hertz);
    int    dur_ms = static_cast<int>(duration.in(au::milli(au::seconds)) + 0.5);
    TaskRequest req;
    req.request_id    = make_uuid();
    req.task_type     = TaskType::NARROWBAND;
    req.schedule_mode = ScheduleMode::IMMEDIATE;
    req.duration_ms   = dur_ms;
    req.rank          = 2;
    req.rf            = {cf_hz, bw_hz, sr_hz, 1};
    req.streaming     = {cfg_.dest_ip};
    return submit(req);
}

TaskResponse SdrClient::wideband(au::QuantityD<au::Hertz> cf,
                                  au::QuantityD<au::Hertz> bw,
                                  au::QuantityD<au::Hertz> sr,
                                  au::QuantityD<au::Seconds> duration) {
    double cf_hz  = cf.in(au::hertz);
    double bw_hz  = bw.in(au::hertz);
    double sr_hz  = sr.in(au::hertz);
    int    dur_ms = static_cast<int>(duration.in(au::milli(au::seconds)) + 0.5);
    TaskRequest req;
    req.request_id    = make_uuid();
    req.task_type     = TaskType::WIDEBAND;
    req.schedule_mode = ScheduleMode::IMMEDIATE;
    req.duration_ms   = dur_ms;
    req.rank          = 2;
    req.rf            = {cf_hz, bw_hz, sr_hz, 1};
    req.streaming     = {cfg_.dest_ip};
    req.wb_params = WidebandParams{true, -60.0, 2048};
    return submit(req);
}

TaskResponse SdrClient::triggered(au::QuantityD<au::Hertz>   cf,
                                   au::QuantityD<au::Hertz>   bw,
                                   au::QuantityD<au::Hertz>   sr,
                                   double                     thr,
                                   au::QuantityD<au::Seconds> post_trigger,
                                   int                        max_cap,
                                   au::QuantityD<au::Seconds> duration) {
    double cf_hz    = cf.in(au::hertz);
    double bw_hz    = bw.in(au::hertz);
    double sr_hz    = sr.in(au::hertz);
    int    post_ms  = static_cast<int>(post_trigger.in(au::milli(au::seconds)) + 0.5);
    int    dur_ms   = static_cast<int>(duration.in(au::milli(au::seconds)) + 0.5);
    TaskRequest req;
    req.request_id    = make_uuid();
    req.task_type     = TaskType::TRIGGERED;
    req.schedule_mode = ScheduleMode::IMMEDIATE;
    req.duration_ms   = dur_ms;
    req.rank          = 2;
    req.rf            = {cf_hz, bw_hz, sr_hz, 1};
    req.streaming     = {cfg_.dest_ip};
    req.trigger_params = TriggerParams{"POWER_THRESHOLD", thr, 50, post_ms, max_cap};
    return submit(req);
}

TaskResponse SdrClient::scan(const std::vector<ScanEntry>& entries,
                              bool repeat, au::QuantityD<au::Seconds> duration) {
    int dur_ms = static_cast<int>(duration.in(au::milli(au::seconds)) + 0.5);
    TaskRequest req;
    req.request_id    = make_uuid();
    req.task_type     = TaskType::SCAN;
    req.schedule_mode = ScheduleMode::IMMEDIATE;
    req.duration_ms   = dur_ms;
    req.rank          = 2;
    if (!entries.empty())
        req.rf = {entries[0].center_freq_hz, entries[0].bandwidth_hz,
                  entries[0].sample_rate_sps, 1};
    req.streaming  = {cfg_.dest_ip};
    req.scan_params = ScanParams{repeat, entries};
    return submit(req);
}

TaskResponse SdrClient::snapshot(au::QuantityD<au::Hertz> cf,
                                  au::QuantityD<au::Hertz> bw,
                                  au::QuantityD<au::Hertz> sr,
                                  int fft_size, int n_avg) {
    double cf_hz = cf.in(au::hertz);
    double bw_hz = bw.in(au::hertz);
    double sr_hz = sr.in(au::hertz);
    TaskRequest req;
    req.request_id      = make_uuid();
    req.task_type       = TaskType::SNAPSHOT;
    req.schedule_mode   = ScheduleMode::IMMEDIATE;
    req.rank            = 2;
    req.rf              = {cf_hz, bw_hz, sr_hz, 1};
    req.snapshot_params = SnapshotParams{cf_hz, bw_hz, sr_hz, fft_size, n_avg};
    return submit(req);
}

TaskResponse SdrClient::calibration(au::QuantityD<au::Hertz>   cf,
                                     au::QuantityD<au::Hertz>   bw,
                                     au::QuantityD<au::Hertz>   sr,
                                     au::QuantityD<au::Seconds> duration,
                                     const std::vector<std::string>& devices) {
    double cf_hz  = cf.in(au::hertz);
    double bw_hz  = bw.in(au::hertz);
    double sr_hz  = sr.in(au::hertz);
    int    dur_ms = static_cast<int>(duration.in(au::milli(au::seconds)) + 0.5);
    TaskRequest req;
    req.request_id    = make_uuid();
    req.task_type     = TaskType::CALIBRATION;
    req.schedule_mode = ScheduleMode::IMMEDIATE;
    req.duration_ms   = dur_ms;
    req.rank          = 2;
    req.rf            = {cf_hz, bw_hz, sr_hz, 1};
    req.streaming     = {cfg_.dest_ip};
    req.cal_params    = CalibrationParams{cf_hz, bw_hz, sr_hz, dur_ms, 1, "", devices};
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
