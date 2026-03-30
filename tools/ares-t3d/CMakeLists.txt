cmake_minimum_required(VERSION 3.16)
project(ares-t3d-tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror=return-type")

include_directories(include)

# Core library (no ares/GL dependency — pure C, testable standalone)
add_library(t3d_hle_core STATIC
    src/t3d_hle.c
    src/matrix_convert.c
    src/gl_renderer.c
    src/rdpq_state.c
    src/shadow_render.c
)

# Optional: build GL renderer with OSMesa
find_library(OSMESA_LIB OSMesa)
if(OSMESA_LIB)
    target_compile_definitions(t3d_hle_core PUBLIC T3D_HLE_GL_ENABLED)
    target_link_libraries(t3d_hle_core ${OSMESA_LIB})
    message(STATUS "OSMesa found: GL renderer enabled")
else()
    message(STATUS "OSMesa not found: GL renderer disabled (stubs only)")
endif()

# Unit tests
add_executable(test_t3d_commands tests/test_t3d_commands.c)
target_link_libraries(test_t3d_commands t3d_hle_core)

add_executable(test_vertex_extract tests/test_vertex_extract.c)
target_link_libraries(test_vertex_extract t3d_hle_core)

add_executable(test_detection tests/test_detection.c)
target_link_libraries(test_detection t3d_hle_core)

add_executable(test_matrix tests/test_matrix.c)
target_link_libraries(test_matrix t3d_hle_core m)

add_executable(test_rdpq_state tests/test_rdpq_state.c)
target_link_libraries(test_rdpq_state t3d_hle_core)

add_executable(test_shadow tests/test_shadow.c)
target_link_libraries(test_shadow t3d_hle_core m)

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
    COMMAND test_matrix
    COMMAND test_rdpq_state
    COMMAND test_shadow
    DEPENDS test_t3d_commands test_vertex_extract test_detection test_matrix test_rdpq_state test_shadow
    COMMENT "Running ares-t3d unit tests"
)
