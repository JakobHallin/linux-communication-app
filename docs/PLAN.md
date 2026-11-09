# Project Plan

[Back to README](../README.md) · [Testing plan](TEST_PLAN.md)

## Goal and scope

Build up from a simple local TCP server: first connect one client, then two;
next use `io_uring` for server network I/O, then add chat. Continue toward the
planned multi-client LAN application with voice, video, screen sharing, and
multiple rooms.

## Initial choices

| Area | Choice |
| --- | --- |
| Language / build | C++20 / CMake |
| Initial transport | TCP sockets |
| Server I/O | Blocking sockets first; Linux `io_uring` in Phase 2 |
| Interface | Terminal |
| Development | Test-Driven Development (Red → Green → Refactor) |

## First phases

See [Phase 1](phases/PHASE_01.md) and [Phase 2](phases/PHASE_02.md). First
prove one-client and then two-client connectivity, then replace the server's
blocking I/O with `io_uring`. Only after that, add text chat in Phase 3.
Apply Red → Green → Refactor to each behavior, using real sockets for
connection and delivery acceptance tests.

## Roadmap

| Phase | Goal | Status |
| --- | --- | --- |
| 1 — Basic connections | Connect one client, then two, to a local TCP server | Complete |
| 2 — `io_uring` server | Move server network I/O to Linux `io_uring`; retain connection behavior | Complete |
| 3 — Text chat | Two clients exchange messages through the server | Complete |
| 4 — LAN text chat | Dockerized server and five native clients exchange text | Complete |
| 5 — Group voice | Build real voice clients and let three to five LAN users speak and listen together | In progress |
| 6 — Webcam video | Share webcam streams alongside voice and text | Complete |
| 7 — Screen sharing | Share a desktop or supported application window | Complete |
| 8 — Multiple rooms | Isolate text and media among independently managed rooms | Planned |
| 9 — Persistence | Add SQLite only if retained data is wanted | Optional |

Later phases describe the intended direction, not completed functionality.
Use TDD within each phase, and refine implementation details when that phase
is near; the immediate commitment is to finish and test the current phase.

## Phase 5 client milestone

The Phase 5 automated tests currently validate the media protocol, UDP
authorization, forwarding, mixing, jitter handling, and Opus processing. Those
tests use synthetic frames or test sockets; they are not a substitute for the
real hardware check.

Phase 5 now has an opt-in real unified client implementation, but the phase
cannot be marked complete until the live LAN acceptance check is performed.
The client retains the existing LAN text connection and adds a separate UDP
voice path. Each client must be able to:

1. establish the TCP application connection and complete the text handshake;
2. send and receive LAN text messages over that TCP connection;
3. establish or reuse its application session for media;
4. register its session and sender identity for media authorization;
5. capture microphone frames through the supported Linux audio API;
6. encode and send Opus frames over UDP;
7. receive forwarded frames from other participants;
8. decode, reorder, mix, and play received audio; and
9. shut down both channels cleanly, including mute and disconnect behavior.

The final Phase 5 acceptance check requires at least two real clients with
microphones and speakers on the same LAN. The automated localhost voice test
is a prerequisite, but it does not replace this real-client test.
