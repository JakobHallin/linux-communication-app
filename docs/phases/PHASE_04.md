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

For Docker's default seccomp policy, use the project profile that permits only
the server's required runtime calls and the three `io_uring` syscalls:

```bash
docker run --rm --security-opt \
  seccomp=./docker/seccomp-io-uring.json \
  -p 8080:8080 linux-communication-server:phase4

# In another terminal, with the container running:
COMMUNICATION_SERVER_HOST=127.0.0.1 \
COMMUNICATION_SERVER_PORT=8080 \
./build/phase4_container_connection_test

# With the same container running:
COMMUNICATION_SERVER_HOST=127.0.0.1 \
COMMUNICATION_SERVER_PORT=8080 \
./build/phase4_five_client_test
```

The five-client test connects five native Linux clients to the one server
container, completes five handshakes, and verifies that one message reaches
the other four clients with the correct sender and content.

### Why `seccomp-io-uring.json` is needed

Docker applies a seccomp system-call filter to containers by default. The
filter is an important security boundary: it limits which kernel operations a
process inside the container can request.

This server uses Linux `io_uring` for network I/O. Creating and operating an
`io_uring` instance requires these system calls:

- `io_uring_setup`
- `io_uring_enter`
- `io_uring_register`

Docker's default profile blocks them, so the server can build successfully but
exits at startup inside a normal container with `Operation not permitted`.
The project profile is based on Docker's normal allowlist and adds only those
three calls. It therefore keeps the container's normal syscall restrictions in
place.

Do not replace the profile with `seccomp=unconfined` for regular development or
deployment. That disables the container's syscall filter instead of granting
the server only what it needs.

Native Linux runs do not need this file. Every later phase that keeps the
server in Docker and continues using `io_uring` should reuse the same profile,
then re-test it when the base image, Docker version, kernel, or server I/O
model changes. If a future phase replaces `io_uring`, the profile should be
reviewed and the three exceptions removed if they are no longer required.

## Completion criteria

- Five real clients exchange text through one Dockerized server.
- Messages show the correct sender and arrive in consistent order.
- Invalid input and client disconnects are handled without crashing the
  server.
- The test leaves no child processes or containers behind.
- Earlier phase tests remain green.
