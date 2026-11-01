# Phase 1 — Basic Connections

**Status:** Implemented · **Goal:** Prove one- and two-client connections

[Project plan](../PLAN.md) · [Testing](../TEST_PLAN.md) · [README](../../README.md)

## Goal

Run a simple local TCP server and terminal client. First verify one client can
connect; then verify two clients can connect to the same server at the same
time. No chat messages are required in this phase.

## Acceptance criteria

- The server starts locally and accepts a client connection.
- A second client can connect while the first remains connected.
- The server can distinguish active connections and report connection closure.
- Disconnecting either client does not crash the server or affect the other.
- A real-socket test verifies one-client and two-client connection behavior.
- Build and run instructions work on Linux without Docker.

## Suggested order

For each behavior, write a focused test first, confirm the expected failure,
implement the minimum to pass, then refactor. Start with one connection, then
add a second concurrent connection and clean disconnect handling.

Use real sockets for connection tests. Keep the implementation simple and
blocking for now; the next phase introduces `io_uring`. Do not add chat
protocols, rooms, or a large test harness yet.

## Boundaries

- Use ordinary TCP sockets; Phase 2 replaces the server I/O with `io_uring`.
- Run server and clients as local processes.
- Limit the acceptance target to two clients.
- Keep the UI in the terminal.
- Defer messages, rooms, voice, video, screen sharing, and persistence.

## Done means

The documented local workflow starts a server and two clients, both clients
connect concurrently, and tests verify connection and disconnect behavior.

## Local workflow

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Start `./build/communication_server` in one terminal, then start
`./build/communication_client` in two other terminals. Each client remains
connected until Enter is pressed. The server listens on `127.0.0.1:8080` and
reports active connections and disconnects.
