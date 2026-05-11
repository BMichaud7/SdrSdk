#include <gtest/gtest.h>
#include <sdrsdk/IqStream.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <chrono>

// Send a single synthetic IQ packet to the given port
static void send_packet(int port, int n_samples) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);

    sdr::IqPacketHeader hdr{};
    hdr.magic     = sdr::IQ_MAGIC;
    hdr.n_samples = (uint16_t)n_samples;
    hdr.format    = 0;

    std::vector<float> iq(n_samples * 2, 0.5f);

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port   = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &dst.sin_addr);

    // Send header + IQ
    std::vector<uint8_t> buf(sizeof(hdr) + iq.size() * sizeof(float));
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    std::memcpy(buf.data() + sizeof(hdr), iq.data(), iq.size() * sizeof(float));
    ::sendto(fd, buf.data(), buf.size(), 0, (sockaddr*)&dst, sizeof(dst));
    ::close(fd);
}

TEST(IqStream, ReceivesPacket) {
    int port = 34567;
    sdr::IqStream stream(port, 2.0);

    std::thread sender([port]{
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        send_packet(port, 256);
    });

    auto samples = stream.collect(256);
    sender.join();

    EXPECT_EQ(stream.packets_rx(), 1);
    EXPECT_GE((int)samples.size(), 256 * 2);
}

TEST(IqStream, IgnoresBadMagic) {
    int port = 34568;
    sdr::IqStream stream(port, 1.0);

    std::thread sender([port]{
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        uint8_t garbage[64] = {0xFF};
        sockaddr_in dst{};
        dst.sin_family = AF_INET;
        dst.sin_port   = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &dst.sin_addr);
        ::sendto(fd, garbage, sizeof(garbage), 0, (sockaddr*)&dst, sizeof(dst));
        ::close(fd);
    });

    auto samples = stream.collect_for(0.6);
    sender.join();

    EXPECT_EQ(stream.packets_rx(), 0);
    EXPECT_TRUE(samples.empty());
}
