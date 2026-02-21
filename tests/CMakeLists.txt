# === Test Runner ===

# Find all test source files
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "*.cpp"
)
list(FILTER TEST_SOURCES EXCLUDE REGEX "check_header.cpp$")

# Find all header files for IDE project view
file(GLOB_RECURSE TEST_HEADERS CONFIGURE_DEPENDS
    "*.hpp"
)

add_executable(NoMoreDayTests
    ${TEST_SOURCES}
    ${TEST_HEADERS}
)

# Keep new V5 JFA tests out of Unity amalgamation to avoid TU-local helper
# collisions with existing contract test helpers.
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/RenderGraphV5ContractsIntegrationTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/ReleaseGateIntegrationTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/ShadowPipelineTierFallbackIntegrationTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/JFADistanceFieldEvaluatorTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance/MaterialLightingBenchmark.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance/PostProcessBenchmark.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance/RadianceCascadesBenchmark.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
)

target_include_directories(NoMoreDayTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(NoMoreDayTests SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/doctest
    ${CMAKE_SOURCE_DIR}/third_party/entt
)

target_link_libraries(NoMoreDayTests PRIVATE NoMoreDayCore)

# Enable Unity Build for Tests
set_target_properties(NoMoreDayTests PROPERTIES
    UNITY_BUILD ON
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${OUTPUT_BIN_DIR}"
)

if(WIN32)
    target_compile_definitions(NoMoreDayTests PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(MSVC)
    target_compile_options(NoMoreDayTests PRIVATE /bigobj /GR- /FS)
endif()

if(MSVC AND ENABLE_RUNTIME_OPT)
    target_link_options(NoMoreDayTests PRIVATE
        $<$<CONFIG:Release>:/OPT:REF /OPT:ICF>
    )
endif()

option(NMD_ENABLE_CTEST_INTEGRATION "Register doctest groups into CTest" ON)
if(BUILD_TESTING AND NMD_ENABLE_CTEST_INTEGRATION)
    add_test(NAME nmd.tests.ci.nonperf
        COMMAND NoMoreDayTests --test-case-exclude=*Performance*
    )
    set_tests_properties(nmd.tests.ci.nonperf PROPERTIES
        LABELS "ci;nonperf"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 900
    )

    add_test(NAME nmd.tests.performance
        COMMAND NoMoreDayTests --test-case=*Performance*
    )
    set_tests_properties(nmd.tests.performance PROPERTIES
        LABELS "performance"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 1200
    )

    add_test(NAME nmd.tests.unit
        COMMAND NoMoreDayTests --test-case=[Unit]*
    )
    set_tests_properties(nmd.tests.unit PROPERTIES
        LABELS "unit"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 600
    )

    add_test(NAME nmd.tests.integration
        COMMAND NoMoreDayTests --test-case=[Integration]*
    )
    set_tests_properties(nmd.tests.integration PROPERTIES
        LABELS "integration"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 900
    )
endif()
