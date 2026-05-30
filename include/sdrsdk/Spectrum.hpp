#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  SdrSdk/Spectrum.hpp
//  Welch-averaged FFT, peak detection, and heuristic signal classification.
//
//  Usage:
//      auto samples = stream.collect_complex(500'000);
//      sdr::Spectrum spec(8192);
//      auto [freqs, psd_db] = spec.welch(samples, sdrunit::MHz(98.5), sdrunit::MHz(2.4));
//      auto signals = spec.find_signals(freqs, psd_db, /*threshold_db=*/10.0);
//      for (auto& s : signals)
//          std::cout << s.freq_mhz << " MHz  " << s.type << "\n";
// ═══════════════════════════════════════════════════════════════════════════
#include "Types.hpp"    // DetectedSignal lives here (also re-exports sdr/Types.hpp)
#include "sdrsdk/units.hpp"
#include <vector>
#include <complex>
#include <string>
#include <tuple>
#include <fftw3.h>

namespace sdr {

class Spectrum {
public:
    explicit Spectrum(int frame_size = 8192);
    ~Spectrum();

    Spectrum(const Spectrum&)            = delete;
    Spectrum& operator=(const Spectrum&) = delete;

    // Welch-averaged PSD.
    // Returns {freq_hz_axis, power_db_axis}, both of length frame_size.
    std::tuple<std::vector<double>, std::vector<double>>
    welch(const std::vector<std::complex<float>>& samples,
          au::QuantityD<au::Hertz> cf, au::QuantityD<au::Hertz> sr) const;

    // Overload for interleaved float array.
    std::tuple<std::vector<double>, std::vector<double>>
    welch(const std::vector<float>& iq,
          au::QuantityD<au::Hertz> cf, au::QuantityD<au::Hertz> sr) const;

    // Find signals above noise_floor + threshold_db.
    // Merges peaks within 50 kHz (FM pilot / RDS tones) and classifies.
    std::vector<DetectedSignal>
    find_signals(const std::vector<double>& freq_hz,
                 const std::vector<double>& psd_db,
                 double threshold_db = 10.0) const;

    // One-shot: welch + find_signals
    std::vector<DetectedSignal>
    analyse(const std::vector<std::complex<float>>& samples,
            au::QuantityD<au::Hertz> cf, au::QuantityD<au::Hertz> sr,
            double threshold_db = 10.0) const;

    int frame_size() const { return frame_; }

private:
    int             frame_;
    fftwf_plan      plan_  = nullptr;
    fftwf_complex*  in_    = nullptr;
    fftwf_complex*  out_   = nullptr;
    std::vector<float> win_;

    static std::string classify(double freq_hz, double bw_hz);
};

} // namespace sdr
