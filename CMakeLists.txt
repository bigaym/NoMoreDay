cmake_minimum_required(VERSION 4.2)
project(NoMoreDay VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT MSVC)
    message(FATAL_ERROR "[Toolchain] NoMoreDay requires MSVC. GCC/MinGW/Clang toolchains are unsupported.")
endif()

# --- Project Options ---
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(ENABLE_ANALYZE "Enable MSVC Static Analysis" OFF)
option(ENABLE_SHOW_INCLUDES "Enable MSVC /showIncludes dependency tracing" OFF)
option(ENABLE_FAST_BUILD "Enable faster MSVC developer build settings" ON)
option(ENABLE_RUNTIME_OPT "Enable additional runtime optimization flags for MSVC" ON)
option(COMBAT_DEFENSE_DEBUG "Enable detailed defense-chain debug logs" OFF)
option(COMBAT_TELEMETRY_ENABLED "Enable combat telemetry instrumentation" ON)
option(NMD_ENABLE_COMPILER_CACHE "Enable compiler cache launcher auto-detection" ON)
option(ENABLE_LTO "Enable interprocedural optimization (LTO) for first-party Release builds" OFF)
# R10 (收尾, B-R0-1): Tracy client integration. Default OFF = zero behavior /
# zero artifact impact (the vendored target compiles to nothing). When ON, the
# vendored client (third_party/tracy, v0.13.1) is built with TRACY_ENABLE +
# TRACY_ENABLE_ALLOCATORS and linked into the UI/render layers; capture with
# the tools under %NMD_TRACY% (tracy-capture / tracy-profiler / tracy-csvexport).
option(TRACY_PROFILING "Enable Tracy profiling instrumentation" OFF)
set(NMD_COMPILER_CACHE_TOOL "AUTO" CACHE STRING "Compiler cache tool: AUTO/SCCACHE/CCACHE/CLCACHE")
set_property(CACHE NMD_COMPILER_CACHE_TOOL PROPERTY STRINGS AUTO SCCACHE CCACHE CLCACHE)
set(NMD_CCACHE_DEFAULT_PATH "" CACHE FILEPATH "Fallback path to ccache executable when PATH lookup fails")

add_library(NoMoreDayAnalyzeFlags INTERFACE)

# MS-1 establishes the shared-type ownership boundary only. It deliberately
# has no sources, PCH, or linked targets until a type passes the admission audit.
add_library(NoMoreDayTypes INTERFACE)
target_include_directories(NoMoreDayTypes INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(NMD_ENABLE_COMPILER_CACHE)
    set(_nmd_cache_program "")
    string(TOUPPER "${NMD_COMPILER_CACHE_TOOL}" _nmd_cache_tool)

    if(_nmd_cache_tool STREQUAL "AUTO")
        if(MSVC)
            find_program(_nmd_cache_program NAMES sccache clcache ccache)
        else()
            find_program(_nmd_cache_program NAMES ccache sccache clcache)
        endif()
    elseif(_nmd_cache_tool STREQUAL "SCCACHE")
        find_program(_nmd_cache_program NAMES sccache)
    elseif(_nmd_cache_tool STREQUAL "CCACHE")
        find_program(_nmd_cache_program NAMES ccache)
    elseif(_nmd_cache_tool STREQUAL "CLCACHE")
        find_program(_nmd_cache_program NAMES clcache)
    endif()

    if(NOT _nmd_cache_program)
        if((_nmd_cache_tool STREQUAL "AUTO" OR _nmd_cache_tool STREQUAL "CCACHE") AND
           NMD_CCACHE_DEFAULT_PATH AND EXISTS "${NMD_CCACHE_DEFAULT_PATH}")
            set(_nmd_cache_program "${NMD_CCACHE_DEFAULT_PATH}")
            message(STATUS "[Cache] PATH lookup missed; using fallback ccache: ${_nmd_cache_program}")
        endif()
    endif()

    if(_nmd_cache_program)
        set(CMAKE_C_COMPILER_LAUNCHER "${_nmd_cache_program}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${_nmd_cache_program}")
        message(STATUS "[Cache] Compiler launcher enabled: ${_nmd_cache_program}")
    else()
        message(STATUS "[Cache] Compiler cache requested but no launcher found (tool=${NMD_COMPILER_CACHE_TOOL})")
    endif()
endif()

# --- Address Sanitizer Configuration ---
if(ENABLE_ASAN)
    message(STATUS "[Security] Enabling Address Sanitizer for MSVC")
    # ASan requires /Zi (not /ZI) and disabling incremental linking.
    add_compile_options(/fsanitize=address /Zi)
    add_link_options(/INCREMENTAL:NO)
endif()

# --- MSVC Static Analysis ---
if(ENABLE_ANALYZE)
    message(STATUS "[Analysis] Enabling MSVC Static Analysis")
    target_compile_options(NoMoreDayAnalyzeFlags INTERFACE /analyze /external:anglebrackets /external:W0)
endif()

if(MSVC AND ENABLE_SHOW_INCLUDES)
    message(STATUS "[Analysis] Enabling /showIncludes output")
    add_compile_options(/showIncludes)
endif()

# --- Disable Deprecated Warnings ---
if(MSVC)
    add_compile_options(/wd4996 /wd4541 /Zc:__cplusplus /utf-8 /bigobj)
endif()

if(MSVC AND ENABLE_FAST_BUILD)
    if(ENABLE_ANALYZE)
        message(STATUS "[Optimization] Fast build enabled (UseMultiToolTask, /MP skipped for /analyze stability)")
    else()
        message(STATUS "[Optimization] Enabling fast MSVC build options (/MP + UseMultiToolTask)")
        add_compile_options(/MP)
    endif()
    set(CMAKE_VS_GLOBALS
        "UseMultiToolTask=true;EnforceProcessCountAcrossBuilds=true"
    )
endif()



# --- Release LTO (IPO) support probe ---
# ENABLE_LTO is declared OFF by default (see Project Options above). Only the
# isolated release-lto verification configuration turns it on; ordinary
# developer builds (build.bat default) pass -DENABLE_LTO=OFF explicitly. The
# support probe must run before target definitions. IPO is applied to required
# first-party targets for Release only in the "Release IPO Scope" section
# below; RelWithDebInfo/Debug/MinSizeRel never receive IPO, so a prior cache
# value cannot leak LTO into default developer builds.
if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error LANGUAGES C CXX)
    if(ipo_supported)
        message(STATUS "[Optimization] LTO (IPO) supported; Release-only IPO will be applied to required first-party targets")
    else()
        message(WARNING "[Optimization] LTO not supported: ${ipo_error}")
    endif()
endif()

# --- MSVC Performance Optimizations ---
# Note: RelWithDebInfo defaults to /Ob1. We upgrade it to /Ob2 to avoid D9025 warnings.
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob2 /DNDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob2 /DNDEBUG" CACHE STRING "" FORCE)
endif()

# --- External Dependencies ---

# 1. Spdlog
set(SPDLOG_BUILD_SHARED ON CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/spdlog)

# 2. Raylib
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # Build as Static Library to support rlgl.h usage
set(OPENGL_VERSION "4.3" CACHE STRING "Force OpenGL 4.3" FORCE)
set(GRAPHICS "GRAPHICS_API_OPENGL_43" CACHE STRING "Force OpenGL 4.3" FORCE)

add_subdirectory(third_party/raylib)

# 3. EnTT
add_subdirectory(third_party/entt)

# 4. Taskflow
set(TF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/taskflow)

# 5. XTL & XSIMD
add_subdirectory(third_party/xtl)
add_subdirectory(third_party/xsimd)

# 6. nlohmann/json

# 7. Tracy (R10): vendored client. With TRACY_PROFILING=OFF the target is an
#    include-only INTERFACE library (zero compile/link cost); with the option
#    ON the single-TU client is compiled into a static library carrying
#    TRACY_ENABLE / TRACY_ENABLE_ALLOCATORS.
add_subdirectory(third_party/tracy)

# --- Tag Generation ---
find_package(Python3 REQUIRED COMPONENTS Interpreter)
message(STATUS "Found Python3 interpreter at: ${Python3_EXECUTABLE}")

set(TAG_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/gen_tags.py")
set(TAG_JSON "${CMAKE_SOURCE_DIR}/assets/data/tags.json")
set(TAG_HEADER "${CMAKE_SOURCE_DIR}/src/game/foundation/data/TagRegistry.hpp")

add_custom_command(
    OUTPUT ${TAG_HEADER}
    COMMAND Python3::Interpreter ${TAG_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS ${TAG_SCRIPT} ${TAG_JSON}
    COMMENT "Generating TagRegistry.hpp from tags.json..."
)

add_custom_target(GenerateTags DEPENDS ${TAG_HEADER})

# --- Skill Behaviors (Object Library) ---
# Using OBJECT library ensures all behaviors are linked even if they have no explicit references,
# which is necessary for the static registration pattern to work.
file(GLOB_RECURSE SKILL_BEHAVIORS_SOURCES CONFIGURE_DEPENDS "src/game/systems/skill/behaviors/*.cpp")

add_library(SkillBehaviors OBJECT ${SKILL_BEHAVIORS_SOURCES})
target_include_directories(SkillBehaviors PRIVATE src)
target_include_directories(SkillBehaviors SYSTEM PRIVATE
    third_party/json/include
    third_party/raylib/src
    third_party/raylib/src/external
    third_party/raylib/src/external/glfw/include
)
target_link_libraries(SkillBehaviors PRIVATE 
    raylib 
    glfw 
    spdlog::spdlog 
    EnTT::EnTT 
    Taskflow
    xsimd
    NoMoreDayAnalyzeFlags
)
target_precompile_headers(SkillBehaviors PRIVATE src/game/pch.hpp)

# --- Layered Static Libraries ---
# Each layer owns an explicit source manifest (no GLOB_RECURSE). The PUBLIC
# dependency chain propagates include paths, compile definitions, and link
# libraries upward so the executable and the test binary receive the full
# transitive closure:
#   NoMoreDay -> NoMoreDayApp -> NoMoreDayGame -> NoMoreDayEngine
#             -> NoMoreDayCore -> NoMoreDayTypes
add_subdirectory(src/core)
add_subdirectory(src/engine)
add_subdirectory(src/game)
add_subdirectory(src/app)

# --- Main Executable ---
add_executable(NoMoreDay src/app/main.cpp)
target_link_libraries(NoMoreDay PRIVATE NoMoreDayApp)
target_link_libraries(NoMoreDay PRIVATE NoMoreDayAnalyzeFlags)

if(MSVC AND ENABLE_RUNTIME_OPT)
    target_link_options(NoMoreDay PRIVATE
        $<$<CONFIG:Release>:/OPT:REF /OPT:ICF>
    )
endif()

# --- Output & Assets ---

# 1. Set Output Directory to project root / bin
set(OUTPUT_BIN_DIR "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_BIN_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_BIN_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_BIN_DIR}")
foreach(OUTPUT_CONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${OUTPUT_CONFIG}" OUTPUT_CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUT_CONFIG_UPPER} "${OUTPUT_BIN_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUT_CONFIG_UPPER} "${OUTPUT_BIN_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUT_CONFIG_UPPER} "${OUTPUT_BIN_DIR}")
endforeach()

# Set Target specific properties
set_target_properties(NoMoreDay PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${OUTPUT_BIN_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# 2. Generate a script in bin/ to create symlinks manually (requires Admin)
set(LINK_SCRIPT "${OUTPUT_BIN_DIR}/link_assets.bat")
file(WRITE ${LINK_SCRIPT} 
    "@echo off\n"
    "echo [Assets] This script requires Administrative privileges to create symlinks.\n"
    "cd /d \"%~dp0\"\n"
    "if exist assets (\n"
    "    echo [Assets] 'assets' already exists (folder or link). Skipping.\n"
    ") else (\n"
    "    echo [Assets] Creating symlink: bin\\assets -> ..\\assets\n"
    "    mklink /d assets \"..\\assets\"\n"
    ")\n"
    "pause\n"
)

# 3. Copy dependency DLLs
add_custom_command(TARGET NoMoreDay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:spdlog>
    "${OUTPUT_BIN_DIR}/"
    COMMENT "Copying DLLs to bin directory..."
)

# --- Tests ---
function(_nmd_ms1_types_boundary_final_guard_7f3c9a)
    cmake_language(
        DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
        GET_CALL_IDS _nmd_types_pending_deferred_calls
    )
    get_target_property(_nmd_types_kind NoMoreDayTypes TYPE)
    get_target_property(_nmd_types_sources NoMoreDayTypes SOURCES)
    get_target_property(_nmd_types_interface_sources NoMoreDayTypes INTERFACE_SOURCES)
    get_target_property(_nmd_types_direct_links NoMoreDayTypes LINK_LIBRARIES)
    get_target_property(_nmd_types_links NoMoreDayTypes INTERFACE_LINK_LIBRARIES)
    get_target_property(_nmd_types_direct_pch NoMoreDayTypes PRECOMPILE_HEADERS)
    get_target_property(_nmd_types_pch NoMoreDayTypes INTERFACE_PRECOMPILE_HEADERS)
    get_target_property(_nmd_types_includes NoMoreDayTypes INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(
        _nmd_types_system_includes
        NoMoreDayTypes
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
    )
    get_filename_component(
        _nmd_types_expected_include_root
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        REALPATH
    )

    if(NOT _nmd_types_kind STREQUAL "INTERFACE_LIBRARY")
        message(FATAL_ERROR "[Modules] NoMoreDayTypes must remain an INTERFACE library")
    endif()
    if(_nmd_types_sources)
        message(FATAL_ERROR "[Modules] NoMoreDayTypes must remain source-free during MS-1")
    endif()
    if(_nmd_types_interface_sources)
        message(FATAL_ERROR "[Modules] NoMoreDayTypes must not export interface sources")
    endif()
    if(_nmd_types_direct_links OR _nmd_types_links)
        message(FATAL_ERROR "[Modules] NoMoreDayTypes must not link Engine, Game, App, or other targets")
    endif()
    if(_nmd_types_direct_pch OR _nmd_types_pch)
        message(FATAL_ERROR "[Modules] NoMoreDayTypes must not use a precompiled header")
    endif()
    set(_nmd_types_include_count 0)
    if(_nmd_types_includes)
        list(LENGTH _nmd_types_includes _nmd_types_include_count)
    endif()
    if(NOT _nmd_types_include_count EQUAL 1 OR
       NOT "${_nmd_types_includes}" STREQUAL
           "${_nmd_types_expected_include_root}")
        message(FATAL_ERROR
            "[Modules] NoMoreDayTypes public include root must be exactly src"
        )
    endif()
    if(_nmd_types_system_includes)
        message(FATAL_ERROR
            "[Modules] NoMoreDayTypes must not export system include directories"
        )
    endif()
    if(_nmd_types_pending_deferred_calls)
        message(FATAL_ERROR
            "[Modules] NoMoreDayTypes final guard found root deferred calls after itself"
        )
    endif()
endfunction()

# DEFER attaches this root-scope final check after every child directory and
# included CMake file have configured. The pre-CMake contract checker reserves
# this command and this as the only first-party DEFER call, preventing a later
# guard redefinition or a child-scheduled root-scope mutation.
cmake_language(DEFER CALL _nmd_ms1_types_boundary_final_guard_7f3c9a)

option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
    # Ensure tests also output to bin or their own directory if preferred, 
    # but the user asked for "bin" in root.
endif()

# --- Release IPO Scope (required first-party deliverables, Release only) ---
# Required deliverable targets: the layered first-party chain (NoMoreDayCore ->
# NoMoreDayEngine -> NoMoreDayGame -> NoMoreDayApp), the main executable
# NoMoreDay, the skill behavior OBJECT library, and (when tests are enabled)
# the NoMoreDayTests binary. Only the per-config
# INTERPROCEDURAL_OPTIMIZATION_RELEASE property is touched, so RelWithDebInfo
# never receives IPO; third-party libraries keep their default (no IPO) state.
# check_ipo_supported alone is not proof: the report/assertion below pins the
# exact Release IPO state per target on every configure. This section runs
# after add_subdirectory(tests) so every required target already exists.
set(_NMD_IPO_REQUIRED_TARGETS
    NoMoreDay
    NoMoreDayApp
    NoMoreDayGame
    NoMoreDayEngine
    NoMoreDayCore
    SkillBehaviors
)
if(BUILD_TESTING)
    list(APPEND _NMD_IPO_REQUIRED_TARGETS NoMoreDayTests)
endif()
if(ENABLE_LTO AND ipo_supported)
    foreach(_nmd_ipo_tgt IN LISTS _NMD_IPO_REQUIRED_TARGETS)
        set_target_properties(${_nmd_ipo_tgt} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        )
    endforeach()
else()
    foreach(_nmd_ipo_tgt IN LISTS _NMD_IPO_REQUIRED_TARGETS)
        set_target_properties(${_nmd_ipo_tgt} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE
        )
    endforeach()
endif()

foreach(_nmd_ipo_tgt IN LISTS _NMD_IPO_REQUIRED_TARGETS)
    get_target_property(_nmd_ipo_release ${_nmd_ipo_tgt} INTERPROCEDURAL_OPTIMIZATION_RELEASE)
    if(_nmd_ipo_release STREQUAL "INTERPROCEDURAL_OPTIMIZATION_RELEASE-NOTFOUND")
        set(_nmd_ipo_release FALSE)
    endif()
    if(ENABLE_LTO AND ipo_supported)
        if(NOT _nmd_ipo_release STREQUAL "TRUE")
            message(FATAL_ERROR
                "[Optimization] ${_nmd_ipo_tgt} must have Release IPO enabled when ENABLE_LTO=ON"
            )
        endif()
        message(STATUS "[Optimization] Release IPO [${_nmd_ipo_tgt}] = TRUE")
    else()
        if(NOT _nmd_ipo_release STREQUAL "FALSE")
            message(FATAL_ERROR
                "[Optimization] ${_nmd_ipo_tgt} must not retain Release IPO when ENABLE_LTO is OFF"
            )
        endif()
        message(STATUS "[Optimization] Release IPO [${_nmd_ipo_tgt}] = disabled")
    endif()
endforeach()
unset(_nmd_ipo_tgt)
unset(_nmd_ipo_release)

# Set compile output encoding to UTF-8
if(MSVC)
    target_compile_options(NoMoreDay PRIVATE /utf-8)
endif()
