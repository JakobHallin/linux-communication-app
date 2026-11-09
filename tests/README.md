# Tests

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
