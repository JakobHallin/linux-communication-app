add_executable(phase1_connection_test
    phase1/phase1_connection_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/src/http_tcpServer.cpp
)

target_include_directories(phase1_connection_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/src
)

target_link_libraries(phase1_connection_test PRIVATE Threads::Threads)

add_test(
    NAME phase1_connection_test
    COMMAND phase1_connection_test
)
set_tests_properties(phase1_connection_test PROPERTIES LABELS "phase1;integration")
