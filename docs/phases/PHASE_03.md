# Phase 3 — Text Chat

**Version:** 0.3 · **Status:** Complete · **Prerequisite:** Phase 2

[Roadmap](../PLAN.md) · [Testing strategy](../TEST_PLAN.md) · [README](../../README.md)

## Goal

Let two connected terminal clients exchange text messages through the server.
The server identifies each sender and relays each message to the other client.

**In scope:** message framing, display names, input validation, message
broadcast, ordered delivery, and disconnect handling. More clients and rooms
can follow after this works.

## Build in this order (Red → Green → Refactor)

| Step | Behavior / implementation | New test first |
| --- | --- | --- |
| 3.1 | Define and parse framed chat messages | Valid, partial, combined, invalid, and oversized frames |
| 3.2 | Add a display-name handshake | Valid names and rejected invalid names |
| 3.3 | Relay a message between two clients | Real-socket test verifies sender and content |
| 3.4 | Preserve message order and handle disconnects | Ordered delivery and recipient remains connected |
| 3.5 | Add bounded handling for invalid input and slow clients | Limits and cleanup tests |

TCP is a byte stream: handle partial reads and writes rather than assuming one
operation corresponds to one message. Keep the protocol small and document its
wire format when implementing it.

## Completion criteria

- Two real clients exchange text through the `io_uring` server.
- Receivers see the correct sender and message content in order.
- Invalid input and disconnects do not crash the server or strand the other
  client.
- Phase 1 and Phase 2 tests remain green.
