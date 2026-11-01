# Testing Plan

[Back to README](../README.md) · [Project plan](PLAN.md) · [Phase 1](phases/PHASE_01.md)

## Approach

Use **Test-Driven Development (Red → Green → Refactor)** for each behavior:

1. Write a focused test that expresses the expected behavior and confirm it
   fails for that reason.
2. Implement the smallest change that makes the test pass.
3. Refactor while keeping the test and existing tests green.

Tests should target real behavior. For example, verify message delivery over
real sockets; don't substitute a mock-based process harness for that. Deferred
features are not treated as tested or complete.

## Initial checks

Apply Red → Green → Refactor independently to behaviors in order:

1. One client connects to the local server.
2. A second client connects while the first remains connected.
3. The server handles either client disconnecting.
4. After the `io_uring` conversion, repeat the real connection tests and add
   asynchronous send/receive and shutdown coverage.
5. Once chat is implemented, verify message delivery between two real clients.

Build and run checks are useful setup checks, but a compiler error or missing
test infrastructure is not a meaningful failing behavior test.

## Definition of done

- The documented build and run steps work.
- One client and then two clients connect to the server.
- Disconnect behavior is handled and tested.
- Tests exercise actual behavior rather than placeholders or deferred features.

## Later roadmap checks

- **Phase 2:** `io_uring` lifecycle and error handling, async I/O, and cleanup.
- **Phase 3:** real-socket two-client message delivery, framing, order, and
  disconnect handling.
- **Phase 4:** Docker startup and real five-client text delivery.
- **Phase 5:** synthetic audio forwarding and stream lifecycle, followed by a
  manual local and LAN voice check with real devices.
- **Phases 6–7:** synthetic video/screen frames and stream lifecycle, followed
  by checks on supported cameras and Linux desktop environments.
- **Phase 8:** room membership and cross-room text/media isolation.
- **Phase 9 (optional):** isolated SQLite tests for persistence and failures.

Activate these checks as each phase begins; they are not expected to pass in
the current bootstrap.

## Phase 5 live audio check

The normal CTest suite deliberately uses synthetic samples. That makes codec
and packet behavior repeatable, but it cannot prove that a real microphone and
speaker work. The hardware check is therefore a separate manual executable.

Install the ALSA development package once:

```bash
sudo apt update
sudo apt install libasound2-dev
```

Build the loopback executable:

```bash
cmake -S . -B build -DENABLE_LIVE_AUDIO_DEMO=ON
cmake --build build --target live_audio_loopback
```

Run it with the default ALSA capture and playback devices:

```bash
./build/live_audio_loopback
```

The executable continuously captures 20 ms mono frames at 48 kHz, encodes
them with Opus, decodes them, and plays them back. Press `Ctrl-C` to stop. A
device name can be supplied when `default` is not appropriate:

```bash
./build/live_audio_loopback <capture-device> <playback-device>
```

List available ALSA devices with `arecord -l` and `aplay -l`. This is a local
capture/codec/playback check, not yet the Phase 5 LAN test; UDP forwarding and
multi-client playback still need to be implemented. Do not register this in
CTest because CI machines commonly have no sound hardware.
