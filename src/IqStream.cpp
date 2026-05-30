#include "sdrsdk/IqStream.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <chrono>

using namespace std::chrono;

namespace sdr {

IqStream::IqStream(int port, au::QuantityD<au::Seconds> timeout)
    : port_(port), timeout_s_(timeout.in(au::seconds))
{
    open_socket();
}

IqStream::~IqStream() { close_socket(); }

void IqStream::open_socket() {
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) throw std::runtime_error("IqStream: socket() failed");

    int rcvbuf = 16 * 1024 * 1024;
    ::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    struct timeval tv { 0, 400000 };   // 400 ms recv timeout
    ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port_);
    if (::bind(fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(fd_);
        throw std::runtime_error("IqStream: bind() failed on port " + std::to_string(port_));
    }
}

void IqStream::close_socket() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

void IqStream::stream(PacketCb cb, au::QuantityD<au::Seconds> timeout) {
    uint8_t buf[65536];
    auto deadline = steady_clock::now() + duration<double>(timeout.in(au::seconds));

    while (steady_clock::now() < deadline) {
        ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
        if (n < (ssize_t)sizeof(IqPacketHeader)) continue;

        IqPacketHeader hdr;
        std::memcpy(&hdr, buf, sizeof(hdr));
        if (hdr.magic != IQ_PACKET_MAGIC) continue;

        int payload_bytes = hdr.num_samples * 8;
        if (n < (ssize_t)(sizeof(hdr) + payload_bytes)) continue;

        auto* iq = reinterpret_cast<float*>(buf + sizeof(hdr));
        pkts_rx_++;
        samp_rx_ += hdr.num_samples;

        if (!cb(hdr, iq, hdr.num_samples)) break;
    }
}

std::vector<float> IqStream::collect(int n_samples) {
    std::vector<float> out;
    out.reserve(n_samples * 2);
    stream([&](const IqPacketHeader&, const float* iq, int n) -> bool {
        for (int i = 0; i < n * 2 && (int)out.size() < n_samples * 2; ++i)
            out.push_back(iq[i]);
        return (int)out.size() < n_samples * 2;
    }, sdrunit::s(timeout_s_));
    return out;
}

std::vector<float> IqStream::collect_for(au::QuantityD<au::Seconds> duration) {
    std::vector<float> out;
    stream([&](const IqPacketHeader&, const float* iq, int n) -> bool {
        for (int i = 0; i < n * 2; ++i) out.push_back(iq[i]);
        return true;
    }, duration);
    return out;
}

std::vector<std::complex<float>> IqStream::collect_complex(int n_samples) {
    auto raw = collect(n_samples);
    std::vector<std::complex<float>> out;
    out.reserve(raw.size() / 2);
    for (size_t i = 0; i + 1 < raw.size(); i += 2)
        out.push_back({raw[i], raw[i+1]});
    return out;
}

std::vector<std::complex<float>> IqStream::collect_complex_for(au::QuantityD<au::Seconds> duration) {
    auto raw = collect_for(duration);
    std::vector<std::complex<float>> out;
    out.reserve(raw.size() / 2);
    for (size_t i = 0; i + 1 < raw.size(); i += 2)
        out.push_back({raw[i], raw[i+1]});
    return out;
}

} // namespace sdr
