#include <gtest/gtest.h>
#include <optional>
#include "initiate_counter.hpp"

using mqtt_consumer::decide_initiate_counter;
using mqtt_consumer::InitiateCounterDecision;

TEST(InitiateCounter, FreshDeviceNullCurZeroCnt) {
    // cur unset, cnt = 0 -> fresh device, no offset needed.
    auto d = decide_initiate_counter(std::nullopt, 0);
    ASSERT_TRUE(d.new_pkg.has_value());
    EXPECT_EQ(*d.new_pkg, 0);
    EXPECT_EQ(d.created_cnt, 0);
}

TEST(InitiateCounter, FreshDeviceNullCurNonZeroCnt) {
    // cur unset, cnt = 928 (device has data already, e.g., DB was nullified
    // by a configure reset that didn't fully complete). Trust device.
    auto d = decide_initiate_counter(std::nullopt, 928);
    ASSERT_TRUE(d.new_pkg.has_value());
    EXPECT_EQ(*d.new_pkg, 912);  // cnt - 16
    EXPECT_EQ(d.created_cnt, 928);
}

TEST(InitiateCounter, TypicalReboot_OnePageAhead) {
    // Healthy device rebooting one page past server's last seen pkg.
    // cur = 912, cnt = 928. Trust branch (cnt <= cur + 16).
    auto d = decide_initiate_counter(912, 928);
    ASSERT_TRUE(d.new_pkg.has_value());
    EXPECT_EQ(*d.new_pkg, 912);  // cnt - 16 = 912 (coincidentally equal to cur)
    EXPECT_EQ(d.created_cnt, 928);
}

TEST(InitiateCounter, StuckRecovery_OffByOneCur) {
    // Production stuck-loop fingerprint: cur = 16k - 1 = 927 (off-by-one),
    // device reports cnt = 928. After fix this hits the trust branch
    // (cnt = cur + 1 <= cur + 16) and snaps cur to 912.
    auto d = decide_initiate_counter(927, 928);
    ASSERT_TRUE(d.new_pkg.has_value());
    EXPECT_EQ(*d.new_pkg, 912);
    EXPECT_EQ(d.created_cnt, 928);
}

TEST(InitiateCounter, ReInitiateSamePosition) {
    // cur = 0, cnt = 0 (device re-Initiates from the same position).
    auto d = decide_initiate_counter(0, 0);
    ASSERT_TRUE(d.new_pkg.has_value());
    EXPECT_EQ(*d.new_pkg, 0);
    EXPECT_EQ(d.created_cnt, 0);
}

TEST(InitiateCounter, DeviceFarAhead_RewindBranch) {
    // cur = 16, cnt = 176 (device claims to be 10 pages ahead - likely
    // QoS-0 broker drops accumulated across a session). Force rewind
    // safety net: don't update DB, reply with cnt = cur + 16.
    auto d = decide_initiate_counter(16, 176);
    EXPECT_FALSE(d.new_pkg.has_value());
    EXPECT_EQ(d.created_cnt, 32);  // 16 + 16
}

TEST(InitiateCounter, BoundaryDeviceExactlyAtCurPlus16) {
    // Boundary: cnt == cur + 16. Should still be trust branch
    // (cnt <= cur + 16 includes equality). Validates we don't slip
    // off the boundary into rewind by accident.
    auto d = decide_initiate_counter(100, 116);
    ASSERT_TRUE(d.new_pkg.has_value());
    EXPECT_EQ(*d.new_pkg, 100);  // 116 - 16
    EXPECT_EQ(d.created_cnt, 116);
}

TEST(InitiateCounter, BoundaryDeviceJustPastCurPlus16) {
    // Boundary: cnt == cur + 17 (one past the trust window). This is
    // the smallest cnt that must take the rewind branch. Pairs with
    // BoundaryDeviceExactlyAtCurPlus16 to fully cover the inequality.
    auto d = decide_initiate_counter(100, 117);
    EXPECT_FALSE(d.new_pkg.has_value());
    EXPECT_EQ(d.created_cnt, 116);  // cur + 16
}

TEST(InitiateCounter, WrapAround_LargeCurZeroCnt) {
    // Device has been running for a long time (large cur) and then
    // factory-resets so cnt comes back as 0. The cnt_int == 0 clause
    // in the predicate forces the trust branch and snaps pkg to 0,
    // matching what the device will start streaming from.
    auto d = decide_initiate_counter(65536, 0);
    ASSERT_TRUE(d.new_pkg.has_value());
    EXPECT_EQ(*d.new_pkg, 0);  // max(0, 0 - 16) = 0
    EXPECT_EQ(d.created_cnt, 0);
}

TEST(InitiateCounter, DM1_OffsetStripped_FreshDevice) {
    // DM1 path: parse_initiate_message strips DM1_COUNTER_OFFSET before
    // calling the helper. Helper itself sees the same shape of inputs
    // as the CAN path; this test documents the integration contract.
    // Fresh DM1 (cur_dm1 unset), device sent cntDM1 = 0x00200000
    // (offset-only, logically 0).
    auto d = decide_initiate_counter(std::nullopt, 0);
    ASSERT_TRUE(d.new_pkg.has_value());
    EXPECT_EQ(*d.new_pkg, 0);
    EXPECT_EQ(d.created_cnt, 0);
}

TEST(InitiateCounter, DM1_OffsetStripped_StuckRecovery) {
    // DM1 stuck-loop fingerprint mirrors the CAN one: cur_dm1 = 16k - 1
    // (off-by-one), device reports cnt_dm1_int = 16k. Trust branch
    // snaps to 16(k-1).
    auto d = decide_initiate_counter(927, 928);
    ASSERT_TRUE(d.new_pkg.has_value());
    EXPECT_EQ(*d.new_pkg, 912);
    EXPECT_EQ(d.created_cnt, 928);
}
