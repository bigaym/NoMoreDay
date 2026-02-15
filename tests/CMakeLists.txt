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
