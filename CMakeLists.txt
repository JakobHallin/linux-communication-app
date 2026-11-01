cmake_minimum_required(VERSION 3.20)
project(linux_communication_app LANGUAGES CXX)

find_package(Threads REQUIRED)
find_path(LIBURING_INCLUDE_DIR liburing.h REQUIRED)
find_library(LIBURING_LIBRARY uring REQUIRED)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(communication_server
    server/src/main.cpp
    server/src/http_tcpServer.cpp
    server/src/chat_protocol.cpp
)
add_executable(communication_client client/src/main.cpp)

target_include_directories(communication_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/server/src
    ${LIBURING_INCLUDE_DIR}
)
target_link_libraries(communication_server PRIVATE Threads::Threads ${LIBURING_LIBRARY})

if(BUILD_TESTING)
    add_executable(phase1_connection_test
        tests/integration/phase1/phase1_connection_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
    )
    target_include_directories(phase1_connection_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase1_connection_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
    )
    add_test(NAME phase1_connection_test COMMAND phase1_connection_test)
    set_tests_properties(phase1_connection_test PROPERTIES LABELS "phase1;integration")

    add_executable(phase2_io_uring_test
        tests/integration/phase2/phase2_io_uring_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
    )
    target_include_directories(phase2_io_uring_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase2_io_uring_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
    )
    add_test(NAME phase2_io_uring_test COMMAND phase2_io_uring_test)
    set_tests_properties(phase2_io_uring_test PROPERTIES LABELS "phase2;integration")

    add_executable(phase3_chat_frame_test
        tests/unit/phase3_chat_frame_test.cpp
        server/src/chat_protocol.cpp
    )
    target_include_directories(phase3_chat_frame_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
    )
    add_test(NAME phase3_chat_frame_test COMMAND phase3_chat_frame_test)
    set_tests_properties(phase3_chat_frame_test PROPERTIES LABELS "phase3;unit")

    add_executable(phase3_display_name_test
        tests/unit/phase3_display_name_test.cpp
        server/src/chat_protocol.cpp
    )
    target_include_directories(phase3_display_name_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
    )
    add_test(NAME phase3_display_name_test COMMAND phase3_display_name_test)
    set_tests_properties(phase3_display_name_test PROPERTIES LABELS "phase3;unit")

    add_executable(phase3_handshake_test
        tests/integration/phase3_handshake_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
    )
    target_include_directories(phase3_handshake_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase3_handshake_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
    )
    add_test(NAME phase3_handshake_test COMMAND phase3_handshake_test)
    set_tests_properties(phase3_handshake_test PROPERTIES LABELS "phase3;integration")

    add_executable(phase3_message_relay_test
        tests/integration/phase3_message_relay_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
    )
    target_include_directories(phase3_message_relay_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase3_message_relay_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
    )
    add_test(NAME phase3_message_relay_test COMMAND phase3_message_relay_test)
    set_tests_properties(phase3_message_relay_test PROPERTIES LABELS "phase3;integration")

    add_executable(phase3_order_disconnect_test
        tests/integration/phase3_order_disconnect_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
    )
    target_include_directories(phase3_order_disconnect_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase3_order_disconnect_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
    )
    add_test(NAME phase3_order_disconnect_test COMMAND phase3_order_disconnect_test)
    set_tests_properties(phase3_order_disconnect_test PROPERTIES LABELS "phase3;integration")

    add_executable(phase3_invalid_input_test
        tests/integration/phase3_invalid_input_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
    )
    target_include_directories(phase3_invalid_input_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase3_invalid_input_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
    )
    add_test(NAME phase3_invalid_input_test COMMAND phase3_invalid_input_test)
    set_tests_properties(phase3_invalid_input_test PROPERTIES LABELS "phase3;integration")

    add_executable(phase3_slow_client_test
        tests/integration/phase3_slow_client_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
    )
    target_include_directories(phase3_slow_client_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase3_slow_client_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
    )
    add_test(NAME phase3_slow_client_test COMMAND phase3_slow_client_test)
    set_tests_properties(phase3_slow_client_test PROPERTIES LABELS "phase3;integration")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(communication_server PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(communication_client PRIVATE -Wall -Wextra -Wpedantic)
endif()

