#include <gtest/gtest.h>
#include <sdrsdk/Types.hpp>

TEST(TaskResponse, UdpPortConvenience) {
    sdr::TaskResponse resp;
    EXPECT_EQ(resp.udp_port(), 0);
    resp.streams.push_back({30001, "stream-1"});
    EXPECT_EQ(resp.udp_port(), 30001);
}

TEST(TaskResponse, BoolConversion) {
    sdr::TaskResponse resp;
    resp.accepted = false;
    EXPECT_FALSE(resp);
    resp.accepted = true;
    EXPECT_TRUE(resp);
}

TEST(Schedule, DefaultIsImmediate) {
    sdr::Schedule s;
    EXPECT_EQ(s.mode, sdr::ScheduleMode::IMMEDIATE);
    EXPECT_EQ(s.duration_ms, 5000);
}

TEST(IqPacketHeader, Size) {
    EXPECT_EQ(sizeof(sdr::IqPacketHeader), 32u);
}
