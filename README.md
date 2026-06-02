# SdrSdk

C++ SDK for the SDR controller stack. Wraps the AMQP task API and IQ UDP
streaming into a clean library so you can write applications without
dealing with proton internals or raw sockets.

## What it provides

| Class | What it does |
|---|---|
| `sdr::SdrClient` | Connects to the Artemis broker, submits tasks, gets responses |
| `sdr::IqStream` | Binds a UDP socket, collects IQ packets from the controller |
| `sdr::Spectrum` | Welch-averaged FFT, peak detection, signal classification |

## Quick start

```cpp
#include <sdrsdk/sdrsdk.hpp>

sdr::SdrClient client;   // defaults: amqp://localhost:5672, sdr_ctrl/sdr_hw_test
client.connect();

// Tune to 476.5 MHz, collect 3 seconds of IQ
auto resp = client.narrowband(476.5e6, 2e6, 2e6, /*duration_ms=*/3000);
if (!resp) { /* resp.reject_reason has details */ }

sdr::IqStream stream(resp.udp_port());
auto samples = stream.collect_complex(200'000);
client.stop(resp.task_id);

// Analyse the spectrum
sdr::Spectrum spectrum;
auto signals = spectrum.analyse(samples, 476.5e6, 2e6);
for (auto& s : signals) std::cout << s.str() << "\n";
```

## Task types

```cpp
client.narrowband (cf, bw, sr, duration_ms);           // NARROWBAND
client.wideband   (cf, bw, sr, duration_ms);           // WIDEBAND (raw IQ)
client.triggered  (cf, bw, sr, threshold_dbfs, ...);   // TRIGGERED burst capture
client.scan       (entries, repeat, duration_ms);       // SCAN frequency list
client.snapshot   (cf, bw, sr, fft_size, n_averages);  // SNAPSHOT (sync FFT)
client.calibration(cf, bw, sr, duration_ms);           // CALIBRATION
```

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build          # runs unit tests (no hardware needed)
```

**Dependencies**:
- `libqpid-proton-cpp-dev` / `qpid-proton-cpp-devel` — AMQP 1.0
- `libfftw3-dev` / `fftw-devel` — FFT
- nlohmann/json — fetched automatically via CMake FetchContent
- [Au units](https://github.com/aurora-opensource/au) 0.5.1 — zero-overhead physical units; fetched automatically via CMake FetchContent

## Use in your own project

SdrSdk is designed to be consumed via `add_subdirectory()`:

```cmake
add_subdirectory(SdrSdk)
target_link_libraries(my_app PRIVATE sdrsdk)
```

> **Note:** There is no installed CMake export set — `find_package(SdrSdk)` is not supported. FetchContent dependencies (nlohmann_json, spdlog) cannot be included in an export set, so SdrSdk is always embedded as a subdirectory.

## Examples

| File | Demonstrates |
|---|---|
| `examples/01_narrowband.cpp` | Submit task, collect IQ, compute power |
| `examples/02_scan_and_detect.cpp` | Sweep 80–200 MHz, classify signals |
| `examples/03_triggered_capture.cpp` | Burst capture, save to `.cf32` file |

## Stack this connects to

The SDK talks to an instance of
[SdrResourceManager](https://github.com/BMichaud7/SdrResourceManager)
via [ActiveMQ Artemis](https://activemq.apache.org/components/artemis/).
Use [SdrScripts](https://github.com/BMichaud7/SdrScripts) to bring up
the full stack:

```bash
git clone https://github.com/BMichaud7/SdrScripts
cd SdrScripts && ./sdr.sh start
```
