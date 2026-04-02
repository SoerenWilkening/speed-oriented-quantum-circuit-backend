cmake_minimum_required(VERSION 3.16)
project(quantum_circuit C)

# ── C standard ───────────────────────────────────────────────────────────────
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── Compiler warnings ───────────────────────────────────────────────────────
add_compile_options(-Wall -Wextra -Wpedantic)

# ── Collect sources ──────────────────────────────────────────────────────────
file(GLOB QUANTUM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

# ── Shared library (libquantum.so / libquantum.dylib) ────────────────────────
add_library(quantum_shared SHARED ${QUANTUM_SOURCES})
set_target_properties(quantum_shared PROPERTIES
    OUTPUT_NAME quantum
    VERSION     0.1.0
    SOVERSION   0
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(quantum_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ── Static library (libquantum.a) ────────────────────────────────────────────
add_library(quantum_static STATIC ${QUANTUM_SOURCES})
set_target_properties(quantum_static PROPERTIES
    OUTPUT_NAME quantum
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(quantum_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ── Install targets ──────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS quantum_shared quantum_static
    EXPORT  quantum_circuit-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT quantum_circuit-targets
    FILE        quantum_circuit-targets.cmake
    NAMESPACE   quantum::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quantum_circuit
)

# ── Testing ──────────────────────────────────────────────────────────────────
enable_testing()

# Module 1.6: test_optimizer links directly against optimizer sources + stubs
# (avoids duplicate symbols with placeholder.c / other module stubs)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optimizer.c")
    add_executable(test_optimizer
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optimizer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/optimizer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/circuit_optimizer.c
    )
    target_include_directories(test_optimizer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_optimizer PRIVATE m)
    add_test(NAME test_optimizer COMMAND test_optimizer)
endif()

# Module 1.7: test_output_stats links against output/stats sources + stubs
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_output_stats.c")
    add_executable(test_output_stats
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_output_stats.c
    )
    target_include_directories(test_output_stats PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_output_stats PRIVATE quantum_static m)
    add_test(NAME test_output_stats COMMAND test_output_stats)
endif()

# Module 1.3: test_context — circuit context lifecycle, gates, allocations
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_context.c")
    add_executable(test_context
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_context.c
    )
    target_include_directories(test_context PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_context PRIVATE quantum_static m)
    add_test(NAME test_context COMMAND test_context)
endif()

# Module 1.4: test_qubit_allocator — qubit allocator lifecycle, alloc/free, reuse
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_qubit_allocator.c")
    add_executable(test_qubit_allocator
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_qubit_allocator.c
    )
    target_include_directories(test_qubit_allocator PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_qubit_allocator PRIVATE quantum_static m)
    add_test(NAME test_qubit_allocator COMMAND test_qubit_allocator)
endif()

# Module 1.9: test_toffoli_cdkm — CDKM Toffoli addition for various widths
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_toffoli_cdkm.c")
    add_executable(test_toffoli_cdkm
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_toffoli_cdkm.c
    )
    target_include_directories(test_toffoli_cdkm PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_toffoli_cdkm PRIVATE quantum_static m)
    add_test(NAME test_toffoli_cdkm COMMAND test_toffoli_cdkm)
endif()

# Module 1.10: test_toffoli_cla — BK CLA Toffoli addition for various widths
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_toffoli_cla.c")
    add_executable(test_toffoli_cla
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_toffoli_cla.c
    )
    target_include_directories(test_toffoli_cla PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_toffoli_cla PRIVATE quantum_static m)
    add_test(NAME test_toffoli_cla COMMAND test_toffoli_cla)
endif()

# Module 1.12: test_remaining — integer, comparison, logic, hot path ops
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_remaining.c")
    add_executable(test_remaining
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_remaining.c
    )
    target_include_directories(test_remaining PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_remaining PRIVATE quantum_static m)
    add_test(NAME test_remaining COMMAND test_remaining)
endif()

# Module 1.8: test_qft_arithmetic — QFT addition and multiplication
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_qft_arithmetic.c")
    add_executable(test_qft_arithmetic
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_qft_arithmetic.c
    )
    target_include_directories(test_qft_arithmetic PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_qft_arithmetic PRIVATE quantum_static m)
    add_test(NAME test_qft_arithmetic COMMAND test_qft_arithmetic)
endif()

# Module 1.11: test_toffoli_mult_div — multiplication, division, modular reduction
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_toffoli_mult_div.c")
    add_executable(test_toffoli_mult_div
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_toffoli_mult_div.c
    )
    target_include_directories(test_toffoli_mult_div PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_toffoli_mult_div PRIVATE quantum_static m)
    add_test(NAME test_toffoli_mult_div COMMAND test_toffoli_mult_div)
endif()

# Module 1.13: test_dispatch — arithmetic dispatch routing
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dispatch.c")
    add_executable(test_dispatch
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dispatch.c
    )
    target_include_directories(test_dispatch PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_dispatch PRIVATE quantum_static m)
    add_test(NAME test_dispatch COMMAND test_dispatch)
endif()

# Module 1.14: test_integration — comprehensive end-to-end integration tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_integration.c")
    add_executable(test_integration
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_integration.c
    )
    target_include_directories(test_integration PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_integration PRIVATE quantum_static m)
    add_test(NAME test_integration COMMAND test_integration)
endif()

# Comparison fix regression tests: ancilla allocation in CQ wrappers
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_comparison_fix.c")
    add_executable(test_comparison_fix
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_comparison_fix.c
    )
    target_include_directories(test_comparison_fix PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_comparison_fix PRIVATE quantum_static m)
    add_test(NAME test_comparison_fix COMMAND test_comparison_fix)
endif()

# Visualization overlap: sub-column splitting for overlapping gates
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_visualize_overlap.c")
    add_executable(test_visualize_overlap
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_visualize_overlap.c
    )
    target_include_directories(test_visualize_overlap PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_visualize_overlap PRIVATE quantum_static m)
    add_test(NAME test_visualize_overlap COMMAND test_visualize_overlap)
endif()

# Sequence API: test public sequence lifecycle, builders, and replay
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sequence_api.c")
    add_executable(test_sequence_api
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sequence_api.c
    )
    target_include_directories(test_sequence_api PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_sequence_api PRIVATE quantum_static m)
    add_test(NAME test_sequence_api COMMAND test_sequence_api)
endif()

# Controlled multiplication and division tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ctrl_mul_div.c")
    add_executable(test_ctrl_mul_div
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ctrl_mul_div.c
    )
    target_include_directories(test_ctrl_mul_div PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_ctrl_mul_div PRIVATE quantum_static m)
    add_test(NAME test_ctrl_mul_div COMMAND test_ctrl_mul_div)
endif()

# Divmod sequence builder tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_divmod_seq.c")
    add_executable(test_divmod_seq
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_divmod_seq.c
    )
    target_include_directories(test_divmod_seq PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_divmod_seq PRIVATE quantum_static m)
    add_test(NAME test_divmod_seq COMMAND test_divmod_seq)
endif()

# QQ comparison sequence builder tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_qq_comparison.c")
    add_executable(test_qq_comparison
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_qq_comparison.c
    )
    target_include_directories(test_qq_comparison PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_qq_comparison PRIVATE quantum_static m)
    add_test(NAME test_qq_comparison COMMAND test_qq_comparison)
endif()

# Sequence qubit count metadata tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sequence_qubit_count.c")
    add_executable(test_sequence_qubit_count
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sequence_qubit_count.c
    )
    target_include_directories(test_sequence_qubit_count PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_sequence_qubit_count PRIVATE quantum_static m)
    add_test(NAME test_sequence_qubit_count COMMAND test_sequence_qubit_count)
endif()

# ── Benchmarks ─────────────────────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/benchmark_runner.c")
    add_executable(benchmark_runner
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/benchmark_runner.c
    )
    target_include_directories(benchmark_runner PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(benchmark_runner PRIVATE quantum_static m)
    # Link rt on Linux for clock_gettime (not needed on macOS)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(benchmark_runner PRIVATE rt)
    endif()
    # Not added to ctest — invoked by benchmarks/run.py
endif()

# Generic test pattern for future modules that link against the full library:
# file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c")
# foreach(test_src ${TEST_SOURCES})
#     get_filename_component(test_name ${test_src} NAME_WE)
#     add_executable(${test_name} ${test_src})
#     target_link_libraries(${test_name} PRIVATE quantum_static)
#     target_include_directories(${test_name} PRIVATE
#         ${CMAKE_CURRENT_SOURCE_DIR}/include
#         ${CMAKE_CURRENT_SOURCE_DIR}/src
#     )
#     add_test(NAME ${test_name} COMMAND ${test_name})
# endforeach()
