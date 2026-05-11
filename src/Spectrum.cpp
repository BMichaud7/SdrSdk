#include "sdrsdk/Spectrum.hpp"
#include "sdrsdk/Types.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>

namespace sdr {

std::string DetectedSignal::str() const {
    std::ostringstream s;
    s << std::fixed << std::setprecision(3) << freq_mhz << " MHz"
      << "  BW=" << std::setprecision(1) << bw_khz << " kHz"
      << "  +" << power_dbc << " dBc"
      << "  " << type;
    return s.str();
}

Spectrum::Spectrum(int frame_size) : frame_(frame_size) {
    in_  = fftwf_alloc_complex(frame_);
    out_ = fftwf_alloc_complex(frame_);
    plan_= fftwf_plan_dft_1d(frame_, in_, out_, FFTW_FORWARD, FFTW_ESTIMATE);

    win_.resize(frame_);
    for (int i = 0; i < frame_; ++i)
        win_[i] = 0.42f - 0.5f*std::cos(2*M_PI*i/(frame_-1))
                        + 0.08f*std::cos(4*M_PI*i/(frame_-1));
}

Spectrum::~Spectrum() {
    if (plan_) fftwf_destroy_plan(plan_);
    fftwf_free(in_); fftwf_free(out_);
}

std::tuple<std::vector<double>, std::vector<double>>
Spectrum::welch(const std::vector<std::complex<float>>& samples,
                double cf_hz, double sr_hz) const
{
    std::vector<float> iq;
    iq.reserve(samples.size() * 2);
    for (auto& s : samples) { iq.push_back(s.real()); iq.push_back(s.imag()); }
    return welch(iq, cf_hz, sr_hz);
}

std::tuple<std::vector<double>, std::vector<double>>
Spectrum::welch(const std::vector<float>& iq,
                double cf_hz, double sr_hz) const
{
    int n_samp = (int)iq.size() / 2;
    int step   = frame_ / 2;
    int frames = (n_samp - frame_) / step;

    std::vector<double> acc(frame_, 0.0);
    if (frames > 0) {
        for (int f = 0; f < frames; ++f) {
            int off = f * step * 2;
            for (int i = 0; i < frame_; ++i) {
                in_[i][0] = iq[off + 2*i]   * win_[i];
                in_[i][1] = iq[off + 2*i+1] * win_[i];
            }
            fftwf_execute(plan_);
            for (int i = 0; i < frame_; ++i)
                acc[i] += (double)out_[i][0]*out_[i][0] + (double)out_[i][1]*out_[i][1];
        }
        for (auto& v : acc) v /= frames;
    }

    double bin_hz = sr_hz / frame_;
    std::vector<double> freqs(frame_), psd(frame_);
    for (int i = 0; i < frame_; ++i) {
        freqs[i] = cf_hz + (i - frame_/2) * bin_hz;
        psd[i]   = 10.0 * std::log10(acc[(i + frame_/2) % frame_] + 1e-30);
    }
    return {freqs, psd};
}

std::vector<DetectedSignal>
Spectrum::find_signals(const std::vector<double>& freq_hz,
                        const std::vector<double>& psd_db,
                        double threshold_db) const
{
    int N = (int)freq_hz.size();
    std::vector<double> sorted_psd(psd_db);
    std::sort(sorted_psd.begin(), sorted_psd.end());
    double noise = sorted_psd[N/2];
    double thr   = noise + threshold_db;

    struct Reg { int lo, hi; };
    std::vector<Reg> regs;
    bool in_r = false; int r_lo = 0;
    for (int i = 0; i < N; ++i) {
        if      (psd_db[i] > thr  && !in_r) { in_r = true;  r_lo = i; }
        else if (psd_db[i] <= thr &&  in_r) { in_r = false; regs.push_back({r_lo, i-1}); }
    }
    if (in_r) regs.push_back({r_lo, N-1});

    std::vector<DetectedSignal> raw;
    for (auto& r : regs) {
        auto pk = (int)(std::max_element(psd_db.begin()+r.lo, psd_db.begin()+r.hi+1) - psd_db.begin());
        double f   = freq_hz[pk];
        double bw  = freq_hz[r.hi] - freq_hz[r.lo];
        double pwr = psd_db[pk] - noise;
        if (bw < 1e3 || bw > 19e6) continue;
        raw.push_back({f/1e6, bw/1e3, pwr, ""});
    }

    // Merge peaks within 50 kHz
    std::vector<DetectedSignal> merged;
    for (auto& s : raw) {
        if (!merged.empty() && std::abs(s.freq_mhz - merged.back().freq_mhz) < 0.05) {
            auto& m = merged.back();
            double lo = std::min(m.freq_mhz - m.bw_khz/2e3, s.freq_mhz - s.bw_khz/2e3);
            double hi = std::max(m.freq_mhz + m.bw_khz/2e3, s.freq_mhz + s.bw_khz/2e3);
            if (s.power_dbc > m.power_dbc) m.freq_mhz = s.freq_mhz;
            m.bw_khz    = (hi - lo) * 1e3;
            m.power_dbc = std::max(m.power_dbc, s.power_dbc);
        } else {
            merged.push_back(s);
        }
    }

    std::vector<DetectedSignal> out;
    for (auto& s : merged) {
        if (s.bw_khz < 2.0) continue;
        s.type = classify(s.freq_mhz * 1e6, s.bw_khz * 1e3);
        out.push_back(s);
    }
    return out;
}

std::vector<DetectedSignal>
Spectrum::analyse(const std::vector<std::complex<float>>& samples,
                   double cf_hz, double sr_hz, double threshold_db) const
{
    auto [freqs, psd] = welch(samples, cf_hz, sr_hz);
    return find_signals(freqs, psd, threshold_db);
}

std::string Spectrum::classify(double f, double bw) {
    if (f >= 87.5e6 && f <= 108e6  && bw >  80e3) return "WFM — Broadcast FM";
    if (f >= 87.5e6 && f <= 108e6)                 return "FM  — Low-power / distant";
    if (f >= 108e6  && f <  118e6  && bw <  30e3) return "AM  — VOR / ILS nav";
    if (f >= 108e6  && f <  118e6)                 return "AM  — Aviation nav";
    if (f >= 118e6  && f <  136e6)                 return "AM  — Aircraft voice";
    if (f >= 136e6  && f <  139e6  && bw >  30e3) return "APT / LRPT — Met satellite";
    if (f >= 136e6  && f <  139e6)                 return "FSK — LEO telemetry";
    if (f >= 144e6  && f <  148e6  && bw <  20e3) return "NFM — 2m amateur";
    if (f >= 144e6  && f <  148e6)                 return "WFM / SSB — 2m amateur";
    if (f >= 148e6  && f <  162e6)                 return "NFM — Public safety / APRS";
    if (f >= 162.3e6 && f <= 162.6e6)              return "NFM — NOAA weather radio";
    if (f >= 162e6  && f <  174e6)                 return "NFM — VHF public safety";
    if (f >= 174e6  && f <  200e6  && bw >   5e6) return "DVB-T — Digital TV";
    if (f >= 174e6  && f <  200e6)                 return "NFM / Digital — VHF hi";
    if (bw > 100e3) return "WFM — Wideband";
    if (bw >  20e3) return "NFM — Narrowband FM";
    return "AM / SSB — Narrowband";
}

} // namespace sdr
