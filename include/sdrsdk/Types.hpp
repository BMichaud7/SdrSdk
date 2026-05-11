#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  SdrSdk/Types.hpp
//  Request/response types for the SDR controller AMQP API.
//  All types map 1-to-1 to JSON fields defined in SdrTaskApi.
// ═══════════════════════════════════════════════════════════════════════════
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace sdr {

// ── Enumerations ─────────────────────────────────────────────────────────────

enum class TaskType {
    NARROWBAND, WIDEBAND, TRIGGERED, SCAN,
    SNAPSHOT, CALIBRATION, DF, UNKNOWN
};

enum class ScheduleMode { IMMEDIATE, SCHEDULED, CONTINUOUS };

// ── RF parameters ─────────────────────────────────────────────────────────────

struct RfParams {
    double  center_freq_hz  = 0;
    double  bandwidth_hz    = 2e6;
    double  sample_rate_sps = 2e6;
    int     rx_count        = 1;
};

// ── Scan ─────────────────────────────────────────────────────────────────────

struct ScanEntry {
    int    step            = 0;
    double center_freq_hz  = 0;
    double bandwidth_hz    = 10e6;
    double sample_rate_sps = 10e6;
    int    dwell_ms        = 500;
};

struct ScanParams {
    bool                  repeat = false;
    std::vector<ScanEntry> entries;
};

// ── Trigger ───────────────────────────────────────────────────────────────────

struct TriggerParams {
    std::string trigger_type    = "POWER_THRESHOLD";
    double      threshold_dbfs  = -60.0;
    int         pre_trigger_ms  = 50;
    int         post_trigger_ms = 200;
    int         max_captures    = 1;
};

// ── Schedule ─────────────────────────────────────────────────────────────────

struct Schedule {
    ScheduleMode mode            = ScheduleMode::IMMEDIATE;
    int          duration_ms     = 5000;
    int64_t      start_epoch_ms  = 0;   // SCHEDULED mode only
    int64_t      end_epoch_ms    = 0;
};

// ── Task request ─────────────────────────────────────────────────────────────

struct TaskRequest {
    TaskType     task_type = TaskType::NARROWBAND;
    int          rank      = 2;
    RfParams     rf;
    Schedule     schedule;
    std::string  dest_ip   = "127.0.0.1";

    // Optional per-type extras
    std::optional<ScanParams>    scan;
    std::optional<TriggerParams> trigger;

    // Snapshot-specific
    int fft_size   = 1024;
    int n_averages = 4;
};

// ── Stream descriptor returned in the task response ─────────────────────────

struct StreamInfo {
    int         udp_port  = 0;
    std::string stream_id;
};

// ── Task response ─────────────────────────────────────────────────────────────

struct TaskResponse {
    std::string              task_id;
    bool                     accepted      = false;
    std::string              status;
    std::string              reject_reason;
    std::vector<StreamInfo>  streams;

    // Convenience: UDP port of first stream (0 if none)
    int udp_port() const {
        return streams.empty() ? 0 : streams[0].udp_port;
    }

    explicit operator bool() const { return accepted; }
};

// ── IQ packet header (matches sdr::IqPacketHeader in the controller) ─────────

struct IqPacketHeader {
    uint32_t magic;           // 0x49515030 ("IQP0")
    uint32_t seq_num;
    uint64_t timestamp_ns;
    uint64_t center_freq_hz;
    uint32_t sample_rate_sps;
    uint16_t n_samples;
    uint8_t  format;
    uint8_t  flags;
};
static_assert(sizeof(IqPacketHeader) == 32, "IQ header size mismatch");
static constexpr uint32_t IQ_MAGIC = 0x49515030;

} // namespace sdr
