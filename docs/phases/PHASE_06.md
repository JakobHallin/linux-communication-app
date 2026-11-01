# Phase 6 — Webcam Video

**Version:** 0.6 · **Status:** In Progress · **Prerequisite:** Phase 5

[Roadmap](../PLAN.md) · [Testing strategy](../TEST_PLAN.md) · [README](../../README.md)

## Audio environment note

The Phase 5 live voice check works in the current local setup, but the
microphone environment has noticeable background noise and occasional buzzing.
Treat that as an environment/audio-device limitation for now, not as proof that
the network voice path is failing. Use headphones, keep microphone boost low,
and disable ALSA loopback while testing Phase 6 video so audio feedback does
not obscure video results. Later audio-quality work should consider noise
suppression, echo cancellation, and microphone-level controls.

## Goal

Multiple LAN participants can share webcam video while maintaining text and
voice communication.

**In scope:** camera start/stop, capture, encode/decode, low-latency transport,
multi-stream display, and basic resolution/network adaptation. **Out of
scope:** advanced conferencing UI and recording.

## Planned flow

```mermaid
flowchart LR
    C[Webcam] --> E[Encode frames]
    E --> S[Server forwards authorized stream]
    S --> R[Other clients decode/display]
```

### Phase 6.1 progress

The first TDD increment defines a versioned `VideoFrame` envelope with session,
sender, sequence, dimensions, and encoded payload fields. Its unit test covers
round-trip serialization, invalid headers, truncation, zero dimensions, and the
maximum frame size. Webcam capture and codec selection remain separate steps;
this protocol layer is intentionally hardware-independent.

The initial Linux implementation will use V4L2 (`/dev/video*`) for webcam
discovery and capture. The first real capture format will be camera-provided
MJPEG, because it avoids adding a software encoder before device lifecycle and
transport behavior are proven. The encoded MJPEG bytes fit the `VideoFrame`
payload; decoding and display will be added after capture works. A later step
can introduce a software codec when measurements show that camera MJPEG is not
available or efficient enough.

The capture lifecycle test now verifies that an unavailable V4L2 device fails
cleanly, remains stopped, rejects frame reads, and can be stopped repeatedly.
The real-device path is intentionally not registered as a normal CTest because
CI machines may not contain `/dev/video*` hardware.

The next TDD increment validates camera-provided MJPEG boundaries before a
payload is wrapped in a `VideoFrame`. It requires the JPEG start/end markers,
rejects empty or oversized data, and rejects invalid dimensions. This keeps
bad camera data out of the transport layer without requiring a physical camera
or software decoder in automated tests.

The capture wrapper now exposes `readVideoFrame(...)`, which combines one
V4L2 capture read with MJPEG validation and the `VideoFrame` metadata. A
failed capture or invalid payload produces no frame, so callers do not have
to forward unchecked camera bytes.

For a real hardware check, build and run the manual
`phase6_video_capture_smoke` executable. It opens `/dev/video0` by default,
captures ten frames, validates each MJPEG payload, and serializes each video
envelope. Pass a different device path and frame count when needed, for
example `./build/phase6_video_capture_smoke /dev/video2 30`. This is a manual
check rather than a CTest because webcam devices are not guaranteed in CI.

To see the captured video live, install `ffplay` if necessary and pipe the
project's validated MJPEG payloads into it:

```bash
./build/phase6_video_capture_smoke /dev/video0 0 --stdout-mjpeg \
  | ffplay -f mjpeg -framerate 30 -i -
```

The `0` frame count means continuous capture. Press `q` in the `ffplay`
window, then stop the capture process with `Ctrl+C`. This displays the actual
frames captured by the project's V4L2 path; it is not a separate camera test.

The first transport increment adds an authorized video UDP relay. It accepts
only registered session/sender pairs, forwards a frame to other participants
in the same session, and never reflects a frame back to its sender. Its
integration test uses local UDP sockets and does not require a webcam.

The TCP control-plane increment now validates `VIDEO_PORT <port>` messages
separately from voice registration. Invalid, missing, zero, overflowing, and
mixed-content ports are rejected before endpoint registration is added to the
server.

The server and `TextClient` now use this control message to register video
UDP endpoints and return `VIDEO_READY <session> <sender> <media-port>`. The
server starts a separate video relay port and removes video registrations when
clients disconnect. Actual webcam capture is still supplied by the Phase 6
V4L2 client path; this increment connects that path to the authorized relay.

The client video session now opens a local UDP socket, starts the V4L2 capture
device only after successful registration, and sends validated serialized
frames to the server relay. If the camera cannot start, activation fails
explicitly instead of appearing to work without sending video.

The unified client can enable this path with
`./build/communication_client SERVER 8080 Alice --video /dev/video0`.
This sends webcam frames to the server; a display/receiver path is the next
increment, while the manual `ffplay` smoke command remains the direct way to
watch the local camera frames today.

## Build in this order (Red → Green → Refactor)

| Step | Behavior / implementation | New test first |
| --- | --- | --- |
| 6.1 | Choose capture/codec/render APIs and frame format | Synthetic frame round-trip acceptance |
| 6.2 | Webcam capture and enable/disable states | Fake capture lifecycle tests; manual device check |
| 6.3 | Encode and decode frames | Repeatable synthetic sample tests |
| 6.4 | Forward frames to authorized members | Mock forwarding / membership tests |
| 6.5 | Receive and display multiple streams | Fake multi-stream rendering/lifecycle tests |
| 6.6 | Respond to changing resolution/network conditions | Stream restart, slow receiver, and size-limit tests |
| 6.7 | Run LAN video + voice integration | Real clients and webcams, earlier tests green |

## Completion criteria

- Multiple participants can share webcam video while voice and text continue.
- Stream start/stop and slow receivers are handled without unbounded resource
  use.
- Synthetic-frame tests pass; real webcam behavior is checked manually.
- Earlier phase tests remain green.
