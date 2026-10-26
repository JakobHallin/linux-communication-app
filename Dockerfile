# syntax=docker/dockerfile:1
FROM ubuntu:24.04 AS builder
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake g++ make ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY CMakeLists.txt ./
COPY server/ server/
COPY client/ client/
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target communication_server -j2

FROM ubuntu:24.04
COPY --from=builder /app/build/communication_server /usr/local/bin/communication_server
USER 65534:65534
ENTRYPOINT ["/usr/local/bin/communication_server"]
