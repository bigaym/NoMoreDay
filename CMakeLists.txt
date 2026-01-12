cmake_minimum_required(VERSION 3.20)
project(NoMoreDay VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_UNITY_BUILD OFF)  # 禁用Unity Build，防止一些文件不编译

# --- Build Acceleration: ccache/sccache ---
find_program(CCACHE_PROGRAM ccache)
find_program(SCCACHE_PROGRAM sccache)
if(SCCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
    message(STATUS "Using sccache: ${SCCACHE_PROGRAM}")
elseif(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

# --- Optimization & Layout ---
set_property(GLOBAL PROPERTY USE_FOLDERS ON) # Organize projects in folders in VS
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON) # Enable Link Time Optimization (LTO) for Release

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
add_library(SkillBehaviors OBJECT
    src/game/systems/skill/behaviors/SkillBehaviorRegistry.cpp
    src/game/systems/skill/behaviors/FlowingThrust.cpp
    src/game/systems/skill/behaviors/RendingWave.cpp
    src/game/systems/skill/behaviors/BladeFormation.cpp
    src/game/systems/skill/behaviors/BladeWard.cpp
    src/game/systems/skill/behaviors/InfiniteBlades.cpp
    src/game/systems/skill/behaviors/SwordArray.cpp
    src/game/systems/skill/behaviors/MindBlade.cpp
    src/game/systems/skill/behaviors/BladeBoomerang.cpp
    src/game/systems/skill/behaviors/PhantomFlash.cpp
)
target_include_directories(SkillBehaviors PRIVATE 
    src
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
)
target_precompile_headers(SkillBehaviors PRIVATE src/pch.hpp)

# --- Core Library (Static) ---
# 将核心逻辑打包为静态库，供游戏主程序和测试程序共同链接
add_library(NoMoreDayCore STATIC
    src/app/Game.cpp
    src/core/logging/CrashHandler.cpp
    src/core/logging/Logger.cpp
    src/engine/input/InputSystem.cpp
    src/engine/physics/PhysicsSystem.cpp
    src/engine/render/GPUEntitySystem.cpp
    src/engine/render/GPUFlowFieldSystem.cpp
    src/engine/render/GPUParticleSystem.cpp
    src/engine/render/RenderSystem.cpp
    src/engine/render/UIRenderer.cpp
    src/engine/resource/AssetLoadingSystem.cpp
    src/engine/resource/ResourceManager.cpp
    src/engine/scene/SceneManager.cpp
    src/engine/scene/StateManager.cpp
    src/game/data/AstrolabeRegistry.cpp
    src/game/data/BiomeRegistry.cpp
    src/game/data/BuffRegistry.cpp
    src/game/data/SkillRegistry.cpp
    src/game/states/GameplayState.cpp
    src/game/states/InventoryState.cpp
    src/game/states/LoadingState.cpp
    src/game/states/MainMenuState.cpp
    src/game/states/PauseState.cpp
    src/game/states/SettingsState.cpp
    src/game/systems/ai/AISystem.cpp
    src/game/systems/ai/EnemyBehavior.cpp
    src/game/systems/combat/CombatEventDispatcher.cpp
    src/game/systems/combat/CombatSystem.cpp
    src/game/systems/combat/DamagePipeline.cpp
    src/game/systems/combat/EffectSystem.cpp
    src/game/systems/combat/ProgressionSystem.cpp
    src/game/systems/combat/StatsSystem.cpp
    src/game/systems/combat/XPAwardingSystem.cpp
    src/game/systems/item/CraftingSystem.cpp
    src/game/systems/item/DropSystem.cpp
    src/game/systems/item/InventorySystem.cpp
    src/game/systems/item/ItemFactory.cpp
    src/game/systems/item/LootFilter.cpp
    src/game/systems/skill/AstrolabeSystem.cpp
    src/game/systems/skill/ProjectileSystem.cpp
    src/game/systems/skill/SkillSystem.cpp
    src/game/systems/skill/SummonSystem.cpp
    src/game/systems/ui/MonsterHealthBarSystem.cpp
    src/game/systems/ui/PlayerHUD.cpp
    src/game/systems/ui/UIAnimationSystem.cpp
    src/game/systems/ui/UIAstrolabe.cpp
    src/game/systems/ui/UICharacter.cpp
    src/game/systems/ui/UICrafting.cpp
    src/game/systems/ui/UIInventory.cpp
    src/game/systems/ui/UIMinimap.cpp
    src/game/systems/ui/UISkillHub.cpp
    src/game/systems/ui/UISkillTalentTree.cpp
    src/game/systems/ui/UISystem.cpp
    src/game/systems/world/EnemySpawnSystem.cpp
    src/game/systems/world/FogOfWarSystem.cpp
    src/game/systems/world/LevelManager.cpp
    src/game/systems/world/MapSystem.cpp
    src/game/systems/world/MovementStanceSystem.cpp
    src/game/systems/world/PortalSystem.cpp
    src/systems/SerializationSystem.hpp
)

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

# Header-only libraries include paths
target_include_directories(NoMoreDayCore PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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
        # === MSVC Compiler Options ===
        target_compile_options(NoMoreDayCore PUBLIC 
            /MP                   # Multi-processor compilation
            /utf-8                # Force UTF-8 encoding for source and execution
            /W4                   # Warning Level 4
            /permissive-          # Strict conformance mode
            /Zc:__cplusplus       # Correct __cplusplus macro definition
            /Zc:inline            # Remove unreferenced COMDAT
            /Zc:forScope          # Stricter for loop scoping
            /Zc:preprocessor      # Modern conformant preprocessor
            /fp:fast              # Fast floating-point model (good for games)
            /Oi                   # Enable intrinsic functions
            /GR-                  # Disable RTTI (we don't use dynamic_cast/typeid)
        )

        # Release-only optimizations (maximum performance)
        target_compile_options(NoMoreDayCore PRIVATE $<$<CONFIG:Release>:
            /O2                   # Maximum optimization
            /Ob3                  # Aggressive inlining (MSVC 19.25+)
            /Gy                   # Enable function-level linking
            /GF                   # Enable string pooling (deduplicate strings)
            /Ot                   # Favor fast code over small code
            /GL                   # Whole program optimization
            /GA                   # Assume thread-local storage (single-thread main loop)
            /Qpar                 # Auto-parallelize loops where safe
        >)

        # RelWithDebInfo optimizations (performance + debugging)
        target_compile_options(NoMoreDayCore PRIVATE $<$<CONFIG:RelWithDebInfo>:
            /O2                   # Maximum optimization
            /Ob2                  # Normal inlining
            /Gy                   # Enable function-level linking
            /GF                   # Enable string pooling
            /Ot                   # Favor fast code
        >)

        # Debug-specific options for fast iteration
        target_compile_options(NoMoreDayCore PRIVATE $<$<CONFIG:Debug>:
            /Ob1                  # Inline marked functions (faster than /Ob0)
            /RTC1                 # Runtime checks
        >)

        # Enable AVX2 for modern CPUs (Intel Haswell+ / AMD Excavator+)
        # Comment out if you need to support older CPUs
        option(ENABLE_AVX2 "Enable AVX2 instruction set" ON)
        if(ENABLE_AVX2)
            target_compile_options(NoMoreDayCore PUBLIC /arch:AVX2)
        endif()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # === GCC Compiler Options ===
        target_compile_options(NoMoreDayCore PUBLIC 
            "-Wa,-mbig-obj"       # Large object file support
            -Wno-stringop-overflow # Suppress false positives from spdlog/fmt
            -fno-rtti             # Disable RTTI
            -ffast-math           # Fast floating-point (like /fp:fast)
        )

        # Release-only optimizations for GCC
        target_compile_options(NoMoreDayCore PRIVATE $<$<CONFIG:Release>:
            -O3                   # Maximum optimization
            -march=native         # Optimize for current CPU (includes AVX2 if available)
            -mtune=native         # Tune for current CPU
            -flto=auto            # Link-time optimization with auto thread detection
            -ffunction-sections   # Enable function-level linking
            -fdata-sections       # Enable data-level linking
            -fno-plt              # Avoid PLT for faster calls
        >)

        # RelWithDebInfo optimizations for GCC
        target_compile_options(NoMoreDayCore PRIVATE $<$<CONFIG:RelWithDebInfo>:
            -O2                   # Good optimization with debugging
            -march=native
            -mtune=native
            -ffunction-sections
            -fdata-sections
        >)

        # Debug-specific options for GCC
        target_compile_options(NoMoreDayCore PRIVATE $<$<CONFIG:Debug>:
            -Og                   # Optimize for debugging (better than -O0)
            -g3                   # Maximum debug info
        >)
    endif()
    
    target_compile_definitions(NoMoreDayCore PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

# --- Main Executable ---
add_executable(NoMoreDay src/app/main.cpp)
target_link_libraries(NoMoreDay PRIVATE NoMoreDayCore)

if(WIN32)
    if(MSVC)
        # === MSVC Linker Options ===
        # Release linker optimizations
        target_link_options(NoMoreDay PRIVATE $<$<CONFIG:Release>:
            /OPT:REF              # Eliminate unreferenced data/functions
            /OPT:ICF              # Identical COMDAT Folding
            /LTCG                 # Link-time code generation (pairs with /GL)
            /CGTHREADS:16         # Parallel code generation threads
        >)

        # RelWithDebInfo linker optimizations
        target_link_options(NoMoreDay PRIVATE $<$<CONFIG:RelWithDebInfo>:
            /OPT:REF
            /OPT:ICF
            /DEBUG:FASTLINK       # Fast incremental linking with debug info
            /PDBALTPATH:%_PDB%    # Portable PDB paths
        >)

        # Debug linker options
        target_link_options(NoMoreDay PRIVATE $<$<CONFIG:Debug>:
            /DEBUG:FASTLINK       # Much faster link times for Debug
            /INCREMENTAL          # Only for Debug (conflicts with /OPT:ICF)
        >)

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # === GCC Linker Options ===
        # Release linker optimizations
        target_link_options(NoMoreDay PRIVATE $<$<CONFIG:Release>:
            -flto=auto            # LTO at link time
            -Wl,--gc-sections     # Remove unused sections
            -Wl,-O2               # Linker optimization level
            -s                    # Strip symbols (smaller binary)
        >)

        # RelWithDebInfo linker optimizations
        target_link_options(NoMoreDay PRIVATE $<$<CONFIG:RelWithDebInfo>:
            -Wl,--gc-sections
            -Wl,-O1
        >)
    endif()
endif()

# --- Output & Assets ---

# 1. Set Output Directory to build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(NoMoreDay PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# --- Tests ---
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# 2. Assets are managed via symlinks (created manually or by setup script)
# No need to copy assets on every build.
# If symlinks are not set up, run: mklink /D build\bin\assets ..\..\assets

# 3. Copy spdlog DLL (still needed since it's a shared library)
add_custom_command(TARGET NoMoreDay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:spdlog>
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
    COMMENT "Copying spdlog DLL..."
)

# Set compile output encoding to UTF-8
# Moved to NoMoreDayCore PUBLIC options above for MSVC

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # For GCC on Windows, ensure console output is UTF-8
    target_compile_definitions(NoMoreDay PRIVATE "_WIN32_WINNT=0x0600" "UNICODE" "_UNICODE")
endif()