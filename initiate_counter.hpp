// Pure helper for the C++ MQTT consumer's Initiate-counter policy.
// Header-only so it can be included by the consumer without changing
// include paths, and exercised by GoogleTest with no Postgres or MQTT
// dependencies.
//
// Background: the device firmware sends two different cnt formats -
// Initiate uses `queueCanGetRead() / 16` (always 16k), Data uses
// `(mqttStreamReadIndex / 16) + 1` (always 16k+1). The historical
// consumer logic stored `pkg = cnt - 1` for both, which produced an
// off-by-one for Initiate and led to an unrecoverable stuck-loop.
// This helper produces the correct pair of values for the Initiate
// path: a new `pkg` that satisfies the existing `cur + 17 == next_cnt`
// gap rule for the first data packet, and a `created_cnt` to echo
// back so the device floors to the same page it reported.
//
// The helper preserves the existing rewind safety net: when the device
// claims to be more than one page ahead of the server's bookkeeping,
// we do NOT update DB and we reply with `cur + 16`, forcing the device
// to retransmit any pages dropped by the broker under QoS 0.

#pragma once

#include <algorithm>
#include <optional>

namespace mqtt_consumer {

struct InitiateCounterDecision {
    std::optional<int> new_pkg;   // nullopt => leave devices.current_package unchanged
    int created_cnt;              // value to echo back in the Created reply
};

inline InitiateCounterDecision decide_initiate_counter(std::optional<int> cur, int cnt_int) {
    if (!cur.has_value() || cnt_int == 0 || cnt_int <= cur.value() + 16) {
        // Trust the device's claimed page. Initiate cnt is always 16k,
        // so storing `cnt - 16 = 16(k-1)` makes the next data cnt
        // (which will be 16k + 1) match the gap rule cur + 17.
        return InitiateCounterDecision{std::max<int>(0, cnt_int - 16), cnt_int};
    }
    // Device claims to be more than one page ahead of server's bookkeeping.
    // Keep the existing rewind safety net: leave DB at cur, force device
    // back to cur + 16 so any pages dropped by the broker under QoS 0
    // are retransmitted from there.
    return InitiateCounterDecision{std::nullopt, cur.value() + 16};
}

}  // namespace mqtt_consumer
