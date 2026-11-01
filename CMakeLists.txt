cmake_minimum_required(VERSION 3.20)
project(linux_communication_app LANGUAGES CXX)

find_package(Threads REQUIRED)
find_path(LIBURING_INCLUDE_DIR liburing.h REQUIRED)
find_library(LIBURING_LIBRARY uring REQUIRED)
find_path(OPUS_INCLUDE_DIR opus/opus.h REQUIRED)
find_library(OPUS_LIBRARY opus REQUIRED)
include(CTest)
option(ENABLE_PHASE4_CONTAINER_TEST
    "Register the Phase 4 test that requires an externally running container"
    OFF
)
option(ENABLE_LIVE_AUDIO_DEMO
    "Build the manual ALSA microphone-to-speaker loopback demo"
    OFF
)
option(ENABLE_LIVE_AUDIO_CLIENT
    "Build communication_client with ALSA microphone and speaker support"
    OFF
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common/include)

add_library(audio_transport STATIC
    common/src/audio_packet.cpp
    common/src/audio_session.cpp
    common/src/audio_udp_transport.cpp
)

add_library(video_protocol STATIC
    common/src/video_frame.cpp
)

add_executable(communication_server
    server/src/main.cpp
    server/src/http_tcpServer.cpp
    server/src/chat_protocol.cpp
    common/src/video_udp_transport.cpp
    common/src/video_frame.cpp
)
add_executable(communication_client
    client/src/main.cpp
    client/src/text_client.cpp
    client/src/video_client.cpp
    client/src/video_capture_device.cpp
    common/src/video_frame.cpp
    common/src/mjpeg_frame.cpp
)

add_executable(phase6_video_capture_smoke
    tests/manual/phase6_video_capture_smoke.cpp
    client/src/video_capture_device.cpp
    common/src/mjpeg_frame.cpp
    common/src/video_frame.cpp
)
target_include_directories(phase6_video_capture_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/client/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include
)
if(ENABLE_LIVE_AUDIO_CLIENT)
    find_package(ALSA REQUIRED)
    target_sources(communication_client PRIVATE
        client/src/voice_client.cpp
        client/src/audio_client_session.cpp
        common/src/audio_codec.cpp
        common/src/audio_jitter_buffer.cpp
        common/src/audio_stream_mixer.cpp
    )
    target_include_directories(communication_client PRIVATE
        ${ALSA_INCLUDE_DIRS}
        ${OPUS_INCLUDE_DIR}
    )
    target_compile_definitions(communication_client PRIVATE ENABLE_LIVE_AUDIO_CLIENT)
    target_link_libraries(communication_client PRIVATE
        audio_transport
        ${ALSA_LIBRARIES}
        ${OPUS_LIBRARY}
    )
endif()

target_include_directories(communication_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/server/src
    ${LIBURING_INCLUDE_DIR}
)
target_include_directories(communication_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/client/include
)
target_link_libraries(communication_server PRIVATE
    Threads::Threads
    ${LIBURING_LIBRARY}
    audio_transport
)

if(BUILD_TESTING)
    add_executable(phase1_connection_test
        tests/integration/phase1/phase1_connection_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
        common/src/video_udp_transport.cpp
        common/src/video_frame.cpp
    )
    target_include_directories(phase1_connection_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase1_connection_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
        audio_transport
    )
    add_test(NAME phase1_connection_test COMMAND phase1_connection_test)
    set_tests_properties(phase1_connection_test PROPERTIES LABELS "phase1;integration")

    add_executable(phase2_io_uring_test
        tests/integration/phase2/phase2_io_uring_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
        common/src/video_udp_transport.cpp
        common/src/video_frame.cpp
    )
    target_include_directories(phase2_io_uring_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase2_io_uring_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
        audio_transport
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

    add_executable(phase5_audio_packet_test
        tests/unit/phase5_audio_packet_test.cpp
        common/src/audio_packet.cpp
    )
    target_include_directories(phase5_audio_packet_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    )
    add_test(NAME phase5_audio_packet_test COMMAND phase5_audio_packet_test)
    set_tests_properties(phase5_audio_packet_test PROPERTIES LABELS "phase5;unit")

    add_executable(phase5_audio_session_test
        tests/unit/phase5_audio_session_test.cpp
        common/src/audio_session.cpp
    )
    target_include_directories(phase5_audio_session_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    )
    add_test(NAME phase5_audio_session_test COMMAND phase5_audio_session_test)
    set_tests_properties(phase5_audio_session_test PROPERTIES LABELS "phase5;unit")

    add_executable(phase5_multi_stream_mixer_test
        tests/unit/phase5_multi_stream_mixer_test.cpp
        common/src/audio_stream_mixer.cpp
    )
    target_include_directories(phase5_multi_stream_mixer_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
        ${OPUS_INCLUDE_DIR}
    )
    add_test(NAME phase5_multi_stream_mixer_test COMMAND phase5_multi_stream_mixer_test)
    set_tests_properties(phase5_multi_stream_mixer_test PROPERTIES LABELS "phase5;unit")

    add_executable(phase5_audio_resilience_test
        tests/unit/phase5_audio_resilience_test.cpp
        common/src/audio_jitter_buffer.cpp
        common/src/audio_stream_mixer.cpp
    )
    target_include_directories(phase5_audio_resilience_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
        ${OPUS_INCLUDE_DIR}
    )
    add_test(NAME phase5_audio_resilience_test COMMAND phase5_audio_resilience_test)
    set_tests_properties(phase5_audio_resilience_test PROPERTIES LABELS "phase5;unit")

    add_executable(phase5_audio_client_session_test
        tests/unit/phase5_audio_client_session_test.cpp
        client/src/audio_client_session.cpp
        common/src/audio_packet.cpp
    )
    target_include_directories(phase5_audio_client_session_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/client/include
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    )
    add_test(NAME phase5_audio_client_session_test COMMAND phase5_audio_client_session_test)
    set_tests_properties(phase5_audio_client_session_test PROPERTIES LABELS "phase5;unit")

    add_executable(phase5_voice_registration_test
        tests/unit/phase5_voice_registration_test.cpp
        server/src/chat_protocol.cpp
    )
    target_include_directories(phase5_voice_registration_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
    )
    add_test(NAME phase5_voice_registration_test COMMAND phase5_voice_registration_test)
    set_tests_properties(phase5_voice_registration_test PROPERTIES LABELS "phase5;unit")

    add_executable(phase5_voice_registration_integration_test
        tests/integration/phase5_voice_registration_integration_test.cpp
        client/src/text_client.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
        common/src/video_udp_transport.cpp
        common/src/video_frame.cpp
    )
    target_include_directories(phase5_voice_registration_integration_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/client/include
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase5_voice_registration_integration_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
        audio_transport
    )
    add_test(NAME phase5_voice_registration_integration_test
        COMMAND phase5_voice_registration_integration_test)
    set_tests_properties(phase5_voice_registration_integration_test PROPERTIES
        LABELS "phase5;integration")

    add_executable(phase5_unified_text_client_test
        tests/integration/phase5_unified_text_client_test.cpp
        client/src/text_client.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
        common/src/video_udp_transport.cpp
        common/src/video_frame.cpp
    )
    target_include_directories(phase5_unified_text_client_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/client/include
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase5_unified_text_client_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
        audio_transport
    )
    add_test(NAME phase5_unified_text_client_test COMMAND phase5_unified_text_client_test)
    set_tests_properties(phase5_unified_text_client_test PROPERTIES LABELS "phase5;integration")

    add_executable(phase5_udp_transport_test
        tests/integration/phase5_udp_transport_test.cpp
        common/src/audio_packet.cpp
        common/src/audio_session.cpp
        common/src/audio_udp_transport.cpp
    )
    target_include_directories(phase5_udp_transport_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    )
    add_test(NAME phase5_udp_transport_test COMMAND phase5_udp_transport_test)
    set_tests_properties(phase5_udp_transport_test PROPERTIES LABELS "phase5;integration")

    add_executable(phase5_voice_e2e_test
        tests/integration/phase5_voice_e2e_test.cpp
        common/src/audio_packet.cpp
        common/src/audio_session.cpp
        common/src/audio_udp_transport.cpp
        common/src/audio_stream_mixer.cpp
        common/src/audio_codec.cpp
    )
    target_include_directories(phase5_voice_e2e_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
        ${OPUS_INCLUDE_DIR}
    )
    target_link_libraries(phase5_voice_e2e_test PRIVATE ${OPUS_LIBRARY})
    add_test(NAME phase5_voice_e2e_test COMMAND phase5_voice_e2e_test)
    set_tests_properties(phase5_voice_e2e_test PROPERTIES LABELS "phase5;integration")

    add_executable(phase5_opus_codec_test
        tests/unit/phase5_opus_codec_test.cpp
        common/src/audio_codec.cpp
    )
    target_include_directories(phase5_opus_codec_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
        ${OPUS_INCLUDE_DIR}
    )
    target_link_libraries(phase5_opus_codec_test PRIVATE ${OPUS_LIBRARY})
    add_test(NAME phase5_opus_codec_test COMMAND phase5_opus_codec_test)
    set_tests_properties(phase5_opus_codec_test PROPERTIES LABELS "phase5;unit")

    add_executable(phase6_video_frame_test
        tests/unit/phase6_video_frame_test.cpp
        common/src/video_frame.cpp
    )
    target_include_directories(phase6_video_frame_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    )
    add_test(NAME phase6_video_frame_test COMMAND phase6_video_frame_test)
    set_tests_properties(phase6_video_frame_test PROPERTIES LABELS "phase6;unit")

    add_executable(phase6_video_capture_test
        tests/unit/phase6_video_capture_test.cpp
        client/src/video_capture_device.cpp
        common/src/mjpeg_frame.cpp
    )
    target_include_directories(phase6_video_capture_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/client/include
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    )
    add_test(NAME phase6_video_capture_test COMMAND phase6_video_capture_test)
    set_tests_properties(phase6_video_capture_test PROPERTIES LABELS "phase6;unit")

    add_executable(phase6_video_udp_transport_test
        tests/integration/phase6_video_udp_transport_test.cpp
        common/src/video_frame.cpp
        common/src/video_udp_transport.cpp
    )
    target_include_directories(phase6_video_udp_transport_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    )
    add_test(NAME phase6_video_udp_transport_test
        COMMAND phase6_video_udp_transport_test)
    set_tests_properties(phase6_video_udp_transport_test PROPERTIES
        LABELS "phase6;integration")

    add_executable(phase6_video_control_test
        tests/unit/phase6_video_control_test.cpp
        server/src/chat_protocol.cpp
    )
    target_include_directories(phase6_video_control_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
    )
    add_test(NAME phase6_video_control_test COMMAND phase6_video_control_test)
    set_tests_properties(phase6_video_control_test PROPERTIES LABELS "phase6;unit")

    add_executable(phase6_video_client_test
        tests/unit/phase6_video_client_test.cpp
        client/src/video_client.cpp
        client/src/video_capture_device.cpp
        common/src/video_frame.cpp
        common/src/mjpeg_frame.cpp
    )
    target_include_directories(phase6_video_client_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/client/include
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    )
    add_test(NAME phase6_video_client_test COMMAND phase6_video_client_test)
    set_tests_properties(phase6_video_client_test PROPERTIES LABELS "phase6;unit")

    add_executable(phase6_mjpeg_frame_test
        tests/unit/phase6_mjpeg_frame_test.cpp
        common/src/mjpeg_frame.cpp
    )
    target_include_directories(phase6_mjpeg_frame_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    )
    add_test(NAME phase6_mjpeg_frame_test COMMAND phase6_mjpeg_frame_test)
    set_tests_properties(phase6_mjpeg_frame_test PROPERTIES LABELS "phase6;unit")


    add_executable(phase3_handshake_test
        tests/integration/phase3_handshake_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
        common/src/video_udp_transport.cpp
        common/src/video_frame.cpp
    )
    target_include_directories(phase3_handshake_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase3_handshake_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
        audio_transport
    )
    add_test(NAME phase3_handshake_test COMMAND phase3_handshake_test)
    set_tests_properties(phase3_handshake_test PROPERTIES LABELS "phase3;integration")

    add_executable(phase3_message_relay_test
        tests/integration/phase3_message_relay_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
        common/src/video_udp_transport.cpp
        common/src/video_frame.cpp
    )
    target_include_directories(phase3_message_relay_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase3_message_relay_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
        audio_transport
    )
    add_test(NAME phase3_message_relay_test COMMAND phase3_message_relay_test)
    set_tests_properties(phase3_message_relay_test PROPERTIES LABELS "phase3;integration")

    add_executable(phase3_order_disconnect_test
        tests/integration/phase3_order_disconnect_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
        common/src/video_udp_transport.cpp
        common/src/video_frame.cpp
    )
    target_include_directories(phase3_order_disconnect_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase3_order_disconnect_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
        audio_transport
    )
    add_test(NAME phase3_order_disconnect_test COMMAND phase3_order_disconnect_test)
    set_tests_properties(phase3_order_disconnect_test PROPERTIES LABELS "phase3;integration")

    add_executable(phase3_invalid_input_test
        tests/integration/phase3_invalid_input_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
        common/src/video_udp_transport.cpp
        common/src/video_frame.cpp
    )
    target_include_directories(phase3_invalid_input_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase3_invalid_input_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
        audio_transport
    )
    add_test(NAME phase3_invalid_input_test COMMAND phase3_invalid_input_test)
    set_tests_properties(phase3_invalid_input_test PROPERTIES LABELS "phase3;integration")

    add_executable(phase3_slow_client_test
        tests/integration/phase3_slow_client_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
        common/src/video_udp_transport.cpp
        common/src/video_frame.cpp
    )
    target_include_directories(phase3_slow_client_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase3_slow_client_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
        audio_transport
    )
    add_test(NAME phase3_slow_client_test COMMAND phase3_slow_client_test)
    set_tests_properties(phase3_slow_client_test PROPERTIES LABELS "phase3;integration")

    add_executable(phase4_multi_client_test
        tests/integration/phase4_multi_client_test.cpp
        server/src/http_tcpServer.cpp
        server/src/chat_protocol.cpp
        common/src/video_udp_transport.cpp
        common/src/video_frame.cpp
    )
    target_include_directories(phase4_multi_client_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/server/src
        ${LIBURING_INCLUDE_DIR}
    )
    target_link_libraries(phase4_multi_client_test PRIVATE
        Threads::Threads
        ${LIBURING_LIBRARY}
        audio_transport
    )
    add_test(NAME phase4_multi_client_test COMMAND phase4_multi_client_test)
    set_tests_properties(phase4_multi_client_test PROPERTIES LABELS "phase4;integration")

    add_executable(phase4_container_connection_test
        tests/integration/phase4_container_connection_test.cpp
    )
    add_executable(phase4_five_client_test
        tests/integration/phase4_five_client_test.cpp
    )
    if(ENABLE_PHASE4_CONTAINER_TEST)
        add_test(NAME phase4_container_connection_test
            COMMAND phase4_container_connection_test
        )
        set_tests_properties(phase4_container_connection_test PROPERTIES
            LABELS "phase4;integration;container"
            ENVIRONMENT "COMMUNICATION_SERVER_HOST=127.0.0.1;COMMUNICATION_SERVER_PORT=8080"
        )
        add_test(NAME phase4_five_client_test COMMAND phase4_five_client_test)
        set_tests_properties(phase4_five_client_test PROPERTIES
            LABELS "phase4;integration;container"
            ENVIRONMENT "COMMUNICATION_SERVER_HOST=127.0.0.1;COMMUNICATION_SERVER_PORT=8080"
        )
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(communication_server PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(communication_client PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ENABLE_LIVE_AUDIO_DEMO)
    find_package(ALSA REQUIRED)
    add_executable(live_audio_loopback
        client/src/live_audio_loopback.cpp
        common/src/audio_codec.cpp
    )
    target_include_directories(live_audio_loopback PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
        ${OPUS_INCLUDE_DIR}
        ${ALSA_INCLUDE_DIRS}
    )
    target_link_libraries(live_audio_loopback PRIVATE
        ${OPUS_LIBRARY}
        ${ALSA_LIBRARIES}
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(live_audio_loopback PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

