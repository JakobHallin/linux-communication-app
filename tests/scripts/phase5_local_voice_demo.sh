#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-voice}"
SERVER="${BUILD_DIR}/communication_server"
CLIENT="${BUILD_DIR}/communication_client"
CAPTURE_DEVICE="${CAPTURE_DEVICE:-default}"
PLAYBACK_DEVICE="${PLAYBACK_DEVICE:-default}"
ACTIVE_CLIENT="${ACTIVE_CLIENT:-Alice}"
LISTENER_CLIENT="${LISTENER_CLIENT:-Bob}"
SILENT_CLIENT="${SILENT_CLIENT:-Carol}"

if [[ "${ACTIVE_CLIENT}" != "Alice" && "${ACTIVE_CLIENT}" != "Bob" ]]; then
    echo "ACTIVE_CLIENT must be Alice or Bob" >&2
    exit 1
fi
if [[ "${ACTIVE_CLIENT}" == "${LISTENER_CLIENT}" ]]; then
    echo "ACTIVE_CLIENT and LISTENER_CLIENT must be different" >&2
    exit 1
fi
if [[ "${SILENT_CLIENT}" == "${ACTIVE_CLIENT}" || "${SILENT_CLIENT}" == "${LISTENER_CLIENT}" ]]; then
    echo "SILENT_CLIENT must be different from the sender and listener" >&2
    exit 1
fi

if [[ ! -x "${SERVER}" || ! -x "${CLIENT}" ]]; then
    echo "Missing voice binaries in ${BUILD_DIR}. Build them first:" >&2
    echo "  cmake --build ${BUILD_DIR} --target communication_server communication_client" >&2
    exit 1
fi

TEMP_DIR="$(mktemp -d)"
declare -a CLIENT_PIDS=()
declare -a CLIENT_FDS=()

cleanup() {
    for pid in "${CLIENT_PIDS[@]:-}"; do
        kill "${pid}" 2>/dev/null || true
    done
    if [[ -n "${SERVER_PID:-}" ]]; then
        kill "${SERVER_PID}" 2>/dev/null || true
    fi
    for fd in "${CLIENT_FDS[@]:-}"; do
        eval "exec ${fd}>&-" 2>/dev/null || true
    done
    wait 2>/dev/null || true
    rm -rf "${TEMP_DIR}"
}
trap cleanup EXIT INT TERM

stdbuf -oL -eL "${SERVER}" >"${TEMP_DIR}/server.log" 2>&1 &
SERVER_PID=$!

for _ in {1..100}; do
    if grep -q "io_uring server listening" "${TEMP_DIR}/server.log"; then
        break
    fi
    if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
        cat "${TEMP_DIR}/server.log" >&2
        exit 1
    fi
    sleep 0.05
done

start_client() {
    local name="$1"
    local capture_device="${CAPTURE_DEVICE}"
    local playback_device="${PLAYBACK_DEVICE}"
    local fifo="${TEMP_DIR}/${name}.fifo"
    if [[ "${name}" != "${ACTIVE_CLIENT}" ]]; then
        capture_device="none"
        playback_device="${PLAYBACK_DEVICE}"
    else
        playback_device="none"
    fi
    if [[ "${name}" == "${SILENT_CLIENT}" ]]; then
        playback_device="none"
    fi
    mkfifo "${fifo}"
    stdbuf -oL -eL "${CLIENT}" 127.0.0.1 8080 "${name}" --voice \
        "${capture_device}" "${playback_device}" \
        <"${fifo}" >"${TEMP_DIR}/${name}.log" 2>&1 &
    CLIENT_PIDS+=("$!")
    local fd
    exec {fd}<>"${fifo}"
    CLIENT_FDS+=("${fd}")
}

start_client Alice
start_client Bob
start_client Carol

echo "Local Phase 5 voice demo roles:"
echo "  sender:   ${ACTIVE_CLIENT} (microphone only)"
echo "  listener: ${LISTENER_CLIENT} (playback only)"
echo "  muted:    ${SILENT_CLIENT} (microphone and playback disabled)"
echo "Capture device: ${CAPTURE_DEVICE}"
echo "Playback device: ${PLAYBACK_DEVICE}"
echo "Speak into the microphone and listen through headphones."
echo "Set ACTIVE_CLIENT=Bob or ACTIVE_CLIENT=Carol to test another sender."
echo "Press Ctrl-C to stop all clients and the server."
echo
echo "Logs: ${TEMP_DIR}"

while kill -0 "${SERVER_PID}" 2>/dev/null; do
    sleep 1
done

echo "Server stopped unexpectedly:" >&2
cat "${TEMP_DIR}/server.log" >&2
exit 1
