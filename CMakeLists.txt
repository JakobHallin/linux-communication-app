cmake_minimum_required(VERSION 3.20)
project(linux_communication_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(communication_server
    server/src/main.cpp
    server/src/http_tcpServer.cpp
)
add_executable(communication_client client/src/main.cpp)

target_include_directories(communication_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/server/src
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(communication_server PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(communication_client PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Milestone 1.2 will add real CTest targets and the selected test framework.
# Milestone 1.4 will add liburing linkage to the networking implementation.
