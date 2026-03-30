cmake_minimum_required(VERSION 3.16)
project(ares-t3d-tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror=return-type")

include_directories(include)

# Core library (no ares dependency — pure C, testable standalone)
add_library(t3d_hle_core STATIC
    src/t3d_hle.c
)

# Unit tests
add_executable(test_t3d_commands tests/test_t3d_commands.c)
target_link_libraries(test_t3d_commands t3d_hle_core)

add_executable(test_vertex_extract tests/test_vertex_extract.c)
target_link_libraries(test_vertex_extract t3d_hle_core)

add_executable(test_detection tests/test_detection.c)
target_link_libraries(test_detection t3d_hle_core)

# Fixture generator
add_executable(gen_fixture tests/gen_fixture.c)

# Standalone buffer decoder tool
add_executable(decode_buffer tools/decode_buffer.c)
target_link_libraries(decode_buffer t3d_hle_core)

# Run all tests
add_custom_target(check
    COMMAND test_t3d_commands
    COMMAND test_vertex_extract
    COMMAND test_detection
    DEPENDS test_t3d_commands test_vertex_extract test_detection
    COMMENT "Running ares-t3d unit tests"
)
