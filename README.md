# MQTT telemetry counter recovery

A small C++17 excerpt from an MQTT telemetry consumer for an embedded vehicle gateway. The original decision helper and its GoogleTest tests are included with permission. CMake packaging was added for standalone builds.

The gateway and consumer use different counter conventions during the reconnect handshake and during data transfer. Storing the handshake counter as if it were a data counter can leave the device and server stuck at different positions. The helper computes what to store and what to acknowledge so the next data page is accepted.

For example, when the server has stored 927 and the gateway reconnects at page counter 928, the helper stores 912 and acknowledges 928. When the gateway reports a position more than one page ahead, the helper keeps the server's saved position and acknowledges a lower counter, prompting the gateway to replay from the missing page.

The helper performs no MQTT or database I/O. It expects nonnegative logical counters after protocol decoding, including removal of any stream-specific offset. The saved counter, when present, must be at most `INT_MAX - 16` so the one-page offset fits in `int`. The zero-counter case models a restart or reset. The helper does not implement modular sequence comparison or validate raw network input.

## Run the tests

With a C++17 compiler, CMake and GoogleTest installed:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests cover a fresh device, recovery from an off-by-one counter, rewind on a gap, the boundary between accepting and rewinding, and reset to zero. Fixtures contain counter values only.
