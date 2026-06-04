/*
========================================================================
Project: OpenRFStack
Author:  Brendan Michaud
Year:    2026
Part of OpenRFStack (https://github.com/OpenRFStack)

Licensed under the Personal Use License.
Do not use for commercial, organizational, or military purposes.
Contact author for permission: https://github.com/OpenRFStack
========================================================================
*/
#include <gtest/gtest.h>
#include <sdr/Types.hpp>   // canonical types from SdrTaskApi

TEST(TaskResponse, UdpPortViaStreams) {
    sdr::TaskResponse resp;
    EXPECT_TRUE(resp.streams.empty());
    sdr::AssignedStream s;
    s.udp_port = 30001;
    resp.streams.push_back(s);
    EXPECT_EQ(resp.streams[0].udp_port, 30001);
}

TEST(TaskResponse, AcceptedDefault) {
    sdr::TaskResponse resp;
    EXPECT_FALSE(resp.accepted);
}

TEST(TaskRequest, DefaultTaskType) {
    sdr::TaskRequest req;
    EXPECT_EQ(req.task_type, sdr::TaskType::UNKNOWN);
    EXPECT_EQ(req.schedule_mode, sdr::ScheduleMode::SCHEDULED);
}

TEST(IqPacketHeader, Size) {
    EXPECT_EQ(sizeof(sdr::IqPacketHeader), 32u);
}

TEST(IqMagic, Value) {
    EXPECT_EQ(sdr::IQ_PACKET_MAGIC, 0x49515030u);
}

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
