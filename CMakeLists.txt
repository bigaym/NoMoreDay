cmake_minimum_required(VERSION 3.20)
project(NoMoreDay VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_UNITY_BUILD OFF)

if(NOT MSVC)
    message(FATAL_ERROR "[Toolchain] NoMoreDay requires MSVC. GCC/MinGW/Clang toolchains are unsupported.")
endif()

# --- Project Options ---
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(ENABLE_ANALYZE "Enable MSVC Static Analysis" OFF)
option(ENABLE_SHOW_INCLUDES "Enable MSVC /showIncludes dependency tracing" OFF)
option(ENABLE_FAST_BUILD "Enable faster MSVC developer build settings" ON)
option(ENABLE_RUNTIME_OPT "Enable additional runtime optimization flags for MSVC" ON)

add_library(NoMoreDayAnalyzeFlags INTERFACE)

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



# Global LTO (IPO) check - needs to be before target definitions
if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
        message(STATUS "[Optimization] LTO (IPO) enabled globally")
        
    else()
        message(WARNING "[Optimization] LTO not supported: ${ipo_error}")
    endif()
endif()

# --- External Dependencies ---

# 1. Spdlog
set(SPDLOG_BUILD_SHARED ON CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/spdlog)
set_target_properties(spdlog PROPERTIES UNITY_BUILD OFF)

# 2. Raylib
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # Build as Static Library to support rlgl.h usage
set(OPENGL_VERSION "4.3" CACHE STRING "Force OpenGL 4.3" FORCE)
set(GRAPHICS "GRAPHICS_API_OPENGL_43" CACHE STRING "Force OpenGL 4.3" FORCE)

add_subdirectory(third_party/raylib)

# Raylib's internal C files are not compatible with Unity Build due to macro and symbol conflicts.
# We must disable Unity Build specifically for the raylib target.
set_target_properties(raylib PROPERTIES UNITY_BUILD OFF)

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

# --- Tag Generation ---
find_package(Python3 REQUIRED COMPONENTS Interpreter)
message(STATUS "Found Python3 interpreter at: ${Python3_EXECUTABLE}")

set(TAG_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/gen_tags.py")
set(TAG_JSON "${CMAKE_SOURCE_DIR}/assets/data/tags.json")
set(TAG_HEADER "${CMAKE_SOURCE_DIR}/src/game/data/TagRegistry.hpp")

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
target_precompile_headers(SkillBehaviors PRIVATE src/pch.hpp)

# Enable Unity Build for Skills
set_target_properties(SkillBehaviors PROPERTIES UNITY_BUILD ON)

# --- Core Library (Static) ---
# 递归获取所有源文件，排除 app/main.cpp 和已在 SkillBehaviors 中的文件
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS 
    "src/*.cpp" 
    "src/*.hpp"
)

# 移除 main.cpp (由主程序目标使用)
list(REMOVE_ITEM CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/app/main.cpp")

# 移除 SkillBehaviors 的源文件，避免重复编译
foreach(SKILL_SRC ${SKILL_BEHAVIORS_SOURCES})
    list(REMOVE_ITEM CORE_SOURCES ${SKILL_SRC})
endforeach()

# 将核心逻辑打包为静态库
add_library(NoMoreDayCore STATIC ${CORE_SOURCES})

set_source_files_properties(src/engine/resource/ResourceManager.cpp PROPERTIES 
    SKIP_PRECOMPILE_HEADERS ON
    SKIP_UNITY_BUILD_INCLUSION ON
)

add_dependencies(NoMoreDayCore GenerateTags)

# Enable Precompiled Headers
target_precompile_headers(NoMoreDayCore PRIVATE src/pch.hpp)

# Link dependencies
target_link_libraries(NoMoreDayCore 
    PUBLIC 
    raylib 
    glfw
    spdlog::spdlog 
    EnTT::EnTT 
    Taskflow
    xsimd
    SkillBehaviors
)
target_link_libraries(NoMoreDayCore PRIVATE NoMoreDayAnalyzeFlags)

# Enable Unity Build for Core
set_target_properties(NoMoreDayCore PROPERTIES UNITY_BUILD OFF)

# Header-only libraries include paths
target_include_directories(NoMoreDayCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(NoMoreDayCore SYSTEM PUBLIC
    third_party/json/include
    third_party/raylib/src
    third_party/raylib/src/external
    third_party/raylib/src/external/glfw/include
)

if(WIN32)
    # Add windows specific libs
    target_link_libraries(NoMoreDayCore PUBLIC winmm kernel32 opengl32 gdi32 user32 shell32 dbghelp)
    
    target_compile_definitions(NoMoreDayCore PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # --- MSVC Performance Optimizations ---
        # Note: RelWithDebInfo defaults to /Ob1. We upgrade it to /Ob2 to avoid D9025 warnings.
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob2 /DNDEBUG" CACHE STRING "" FORCE)
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob2 /DNDEBUG" CACHE STRING "" FORCE)

        # /arch:AVX2  : Enable Advanced Vector Extensions 2 (Critical for xsimd/particles)
        # /Oi         : Generate intrinsic functions
        # /Ot         : Favor fast code
        # /Ob2        : Aggressive inlining
        # /GF         : Eliminate duplicate strings
        target_compile_options(NoMoreDayCore PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )

        if(ENABLE_RUNTIME_OPT)
            # Improve linker-level dead code/data elimination opportunities.
            target_compile_options(NoMoreDayCore PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()
    
    target_compile_definitions(NoMoreDayCore PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

# --- Main Executable ---
add_executable(NoMoreDay src/app/main.cpp)
target_link_libraries(NoMoreDay PRIVATE NoMoreDayCore)
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
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
    # Ensure tests also output to bin or their own directory if preferred, 
    # but the user asked for "bin" in root.
endif()

# Set compile output encoding to UTF-8
if(MSVC)
    target_compile_options(NoMoreDay PRIVATE /utf-8)
endif()
