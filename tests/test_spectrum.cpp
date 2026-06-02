#include <gtest/gtest.h>
#include <sdrsdk/Spectrum.hpp>
#include <sdrsdk/units.hpp>
#include <au/units/hertz.hh>
#include <au/prefix.hh>
#include <cmath>

// Generate a pure tone as complex IQ
static std::vector<std::complex<float>> tone(double freq_hz, double sr_hz, int n) {
    std::vector<std::complex<float>> out(n);
    for (int i = 0; i < n; ++i) {
        double phi = 2 * M_PI * freq_hz / sr_hz * i;
        out[i] = {(float)std::cos(phi), (float)std::sin(phi)};
    }
    return out;
}

TEST(Spectrum, WelchReturnsCorrectSize) {
    sdr::Spectrum spec(1024);
    auto s = tone(1e3, 1e6, 32768);
    auto [f, p] = spec.welch(s, sdrunit::MHz(100.0), sdrunit::MHz(1.0));
    EXPECT_EQ((int)f.size(), 1024);
    EXPECT_EQ((int)p.size(), 1024);
}

TEST(Spectrum, DetectsTone) {
    sdr::Spectrum spec(4096);
    double cf = 100e6, sr = 2e6, tone_offset = 200e3;
    auto s = tone(tone_offset, sr, 65536);
    auto signals = spec.analyse(s, sdrunit::Hz(cf), sdrunit::Hz(sr), /*threshold=*/8.0);
    ASSERT_FALSE(signals.empty());
    // Peak should be within 10 kHz of the injected frequency
    double expected = (cf + tone_offset) / 1e6;
    bool found = false;
    for (auto& sig : signals)
        if (std::abs(sig.freq.in(au::mega(au::hertz)) - expected) < 0.01) found = true;
    EXPECT_TRUE(found) << "Expected signal near " << expected << " MHz";
}

TEST(Spectrum, EmptyInputReturnsNoSignals) {
    sdr::Spectrum spec(4096);
    std::vector<std::complex<float>> empty;
    auto [f, p] = spec.welch(empty, sdrunit::MHz(100.0), sdrunit::MHz(2.0));
    auto sigs   = spec.find_signals(f, p, 10.0);
    EXPECT_TRUE(sigs.empty());
}
