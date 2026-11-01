#!/usr/bin/env bash
set -u

# Prerequisites:
#   make -C build
#   docker build -t linux-communication-server:phase4 .
# `make` builds local C++ targets; it does not build the Docker image.

container_name="linux-communication-server-phase4-cleanup-test"
image_name="linux-communication-server:phase4"
profile="docker/seccomp-io-uring.json"
host_port="18081"

cleanup() {
    docker rm -f "$container_name" >/dev/null 2>&1 || true
}

cleanup
trap cleanup EXIT INT TERM

docker run -d \
    --name "$container_name" \
    -p "${host_port}:8080" \
    --security-opt "seccomp=${profile}" \
    "$image_name" >/dev/null

# This failure is intentional: it exercises the test harness cleanup path.
if COMMUNICATION_SERVER_HOST=127.0.0.1 \
   COMMUNICATION_SERVER_PORT=18082 \
   ./build/phase4_container_connection_test; then
    echo "Expected the deliberately misconfigured client to fail" >&2
    exit 1
fi

cleanup
if docker ps -aq --filter "name=^${container_name}$" | grep -q .; then
    echo "Cleanup left the test container behind" >&2
    exit 1
fi

trap - EXIT INT TERM
echo "Phase 4 failure-cleanup test passed"
