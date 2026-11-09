# Tests

## Build and test independently of an editor

The build and automated tests use CMake and CTest directly. VS Code and its
extensions are optional; no editor configuration is required.

Configure, then complete the default build with `BUILD_TESTING` enabled before
running tests; see the root [build instructions](../README.md#build-test-and-run-from-a-clean-checkout).
The default build compiles the server, client, and test executables together.
CTest only runs existing executables; it does not build them. A server-only or
client-only build is insufficient. If the build fails, stop and resolve its
first error before running tests. **Unable to find executable** means a test
could not start because its binary is missing.

The supported workflow must allow a developer to configure from a clean source
checkout, build, run automated tests, and launch the server and client entirely
from a terminal on a supported Linux machine. Selecting a VS Code preset is
not a project prerequisite. Keep this workflow independent of editor state and
machine-specific build caches.

### Reporting verification results

- An editor extension or agent tool failure is a tooling failure, not evidence
  that the project failed to build or that a test failed.
- If configuration or test execution never started, report the run as
  **not executed**, including the tooling error. Do not ask users to configure
  an editor as though it were required to build this project.
- A previous passing test log is historical evidence, not verification of the
  current source or a clean rebuild.
- After removing generated builds, report a clean rebuild as verified only
  after fresh configuration, compilation, and test execution succeed. Report
  program startup separately if it was checked.

### Automated-test preset

The root [CMakePresets.json](../CMakePresets.json) defines an `automated-tests`
workflow preset that configures, builds, and runs the automated suite in
`build-tests/`. It requires CMake 3.25 or newer, Make, a C++20 compiler, and the
liburing, Opus, and JPEG development libraries on Linux. The runtime environment
must permit `io_uring` and local TCP/UDP sockets for integration tests.

This preset disables hardware-dependent features and container tests. Manual
audio, camera, screen-capture, and container acceptance checks remain separate.

## Test organization

Tests are grouped by how they run. Phase prefixes connect each scenario to
the project roadmap; executable names and CTest labels retain those prefixes.

| Directory | Purpose |
| --- | --- |
| `unit/` | Protocol, codec, and component behavior without a running server |
| `integration/` | Real socket and application interactions |
| `manual/` | Hardware and desktop checks requiring a person or local devices |
| `scripts/` | Shell scenarios and local demos |
| `support/` | Shared checks and test infrastructure, with no production dependencies on it |

Use `phaseN_behavior_test.cpp` for automated scenarios, directly within their
test category. Keep manual smoke checks separate from automated CTest tests.
CTest labels identify the category and phase; container tests additionally
carry the `container` label and require explicit enablement. Automated tests
have a 30-second process timeout as a final guard against blocking calls.

## Checks and shared setup

Use [TEST_CHECK](support/check.h) for checks that report the source location
and failed expression. It evaluates the expression once in every build type,
including Release. Do not use standard `assert()` for test checks or setup:
`NDEBUG` removes the whole expression, including calls with side effects.
`TEST_CHECK` exits the test process on failure, matching the standalone test
executable model; it does not unwind the stack.

The local Phase 1–4 TCP scenarios use [TCP support](support/tcp_test_support.h) for
socket ownership, server shutdown, interrupted sends, complete-line reads,
and deadline-based waits. `require` and `expectLine` throw descriptive failures;
`run` catches them after local socket and server owners have cleaned up.
Prefer these helpers when adding TCP scenarios instead of copying low-level
socket loops. Keep scenario-specific behavior in the test itself.

Use ephemeral ports for local servers, wait for observable state rather than
fixed sleeps, and include the scenario step in failure messages. A receive
timeout applies to the whole line rather than restarting for every byte.

The [testing plan](../docs/TEST_PLAN.md) describes coverage and manual acceptance.
