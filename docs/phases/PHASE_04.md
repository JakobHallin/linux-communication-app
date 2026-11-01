# Phase 4 — LAN Text Chat

**Version:** 0.4 · **Status:** In Progress · **Prerequisite:** Phase 3

[Roadmap](../PLAN.md) · [Testing strategy](../TEST_PLAN.md) · [README](../../README.md)

## Goal

Run the server in Docker and verify five native Linux clients can exchange
text messages over the LAN.

## Build in this order (Red → Green → Refactor)

| Step | Behavior / implementation | New test first |
| --- | --- | --- |
| 4.1 | Validate chat input and bound message sizes | Empty, invalid, and oversized messages are rejected |
| 4.2 | Extend the real-client chat test beyond two clients | Three or more clients exchange messages correctly |
| 4.3 | Build and run the server image | Container starts and accepts a real client connection |
| 4.4 | Run five native clients against one server container | All five exchange messages and disconnect cleanly |
| 4.5 | Exercise failures and cleanup | Server/container and client processes stop after test failure |

Keep clients as native processes; only the server runs in Docker. Check the
container's `io_uring` support and permissions when testing the image. Do not
use broad security bypasses as a substitute for a suitable container setup.

## Completion criteria

- Five real clients exchange text through one Dockerized server.
- Messages show the correct sender and arrive in consistent order.
- Invalid input and client disconnects are handled without crashing the
  server.
- The test leaves no child processes or containers behind.
- Earlier phase tests remain green.
