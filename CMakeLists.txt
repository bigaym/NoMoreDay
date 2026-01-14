cmake_minimum_required(VERSION 3.20)
project(NoMoreDay VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_UNITY_BUILD OFF)  # Disable Unity Build globally

# --- Build Acceleration & Optimization Settings ---
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "[Optimization] Using ccache: ${CCACHE_PROGRAM}")
endif()

# Global LTO (IPO) check - needs to be before target definitions
if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
        message(STATUS "[Optimization] LTO (IPO) enabled globally")
        
        # On Windows with MinGW, we must use gcc-ar/gcc-ranlib for LTO static libs
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND WIN32)
            get_filename_component(COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
            find_program(GCC_AR "gcc-ar" PATHS "${COMPILER_DIR}" NO_DEFAULT_PATH)
            find_program(GCC_RANLIB "gcc-ranlib" PATHS "${COMPILER_DIR}" NO_DEFAULT_PATH)
            if(GCC_AR)
                set(CMAKE_AR "${GCC_AR}")
            endif()
            if(GCC_RANLIB)
                set(CMAKE_RANLIB "${GCC_RANLIB}")
            endif()
            # Use fat lto objects to allow non-LTO-aware tools to still work as fallback
            add_compile_options("$<$<CONFIG:Release>:-ffat-lto-objects>")
        endif()
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
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(raylib PRIVATE "-Wno-stringop-overflow")
endif()

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

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(SkillBehaviors PRIVATE "-Wno-stringop-overflow")
endif()

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
    src/game/states/MosaicEditorState.cpp
    src/game/states/PauseState.cpp
    src/game/states/SettingsState.cpp
    src/game/states/HeirloomVaultState.cpp
    src/game/states/NightmareFloorState.cpp
    src/game/systems/ai/AISystem.cpp
    src/game/systems/ai/EnemyBehavior.cpp
    src/game/systems/ai/EnemyAIBehaviors.cpp
    src/game/systems/combat/CombatEventDispatcher.cpp
    src/game/systems/combat/CombatSystem.cpp
    src/game/systems/combat/EliteModifierSystem.cpp
    src/game/systems/combat/DamagePipeline.cpp
    src/game/systems/combat/VisualFXSystem.cpp
    src/game/systems/combat/EffectSystem.cpp
    src/game/systems/combat/ProgressionSystem.cpp
    src/game/systems/combat/StatsSystem.cpp
    src/game/systems/combat/XPAwardingSystem.cpp
    src/game/systems/item/CraftingSystem.cpp
    src/game/systems/item/DropSystem.cpp
    src/game/systems/item/FragmentDropSystem.cpp
    src/game/systems/item/InventorySystem.cpp
    src/game/systems/item/ItemFactory.cpp
    src/game/systems/item/LootFilter.cpp
    src/game/systems/item/MaterialRegistry.cpp
    src/game/data/ResonanceCalculator.cpp
    src/game/systems/skill/AstrolabeSystem.cpp
    src/game/systems/skill/ProjectileSystem.cpp
    src/game/systems/skill/SkillSystem.cpp
    src/game/systems/skill/BehaviorInjectionRegistry.cpp
    src/game/systems/skill/ShadowSystem.cpp
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
    src/game/systems/world/MosaicMapGenerator.cpp
    src/game/systems/world/MovementStanceSystem.cpp
    src/game/systems/world/PortalSystem.cpp
    src/game/systems/nemesis/FactionAggroSystem.cpp
    src/game/systems/nemesis/NemesisGenerator.cpp
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

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # -Wno-stringop-overflow: Suppress false positives from spdlog/fmt in GCC 13/14
        target_compile_options(NoMoreDayCore PUBLIC "-Wa,-mbig-obj" "-Wno-stringop-overflow")
        
        # Force console subsystem to avoid "undefined reference to WinMain"
        # This is propagated to all targets linking NoMoreDayCore
        target_link_options(NoMoreDayCore PUBLIC "-mconsole")

        # --- Performance Optimizations ---
        # -O2: Safe and stable optimization level
        # -mtune=native: Tune instruction scheduling for current CPU (safe, no new instructions)
        # NOTE: -march=native disabled as it causes crashes (likely alignment or UB issues in third-party code)
        target_compile_options(NoMoreDayCore PUBLIC
            $<$<CONFIG:Release>:-O2 -mtune=native -fomit-frame-pointer>
            $<$<CONFIG:RelWithDebInfo>:-O2 -mtune=native>
        )
    endif()
    
    target_compile_definitions(NoMoreDayCore PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

# --- Main Executable ---
add_executable(NoMoreDay src/app/main.cpp)
target_link_libraries(NoMoreDay PRIVATE NoMoreDayCore)

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # STRIP symbols in Release mode to drastically reduce size
    # target_link_options(NoMoreDay PRIVATE $<$<CONFIG:Release>:-s>)
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

# 2. Copy Assets Folder (Robust Cross-Platform)
# Symlinks on Windows via CMake can be flaky due to permissions or shell differences.
# copy_directory is safe and effective for this scale.   

# 手动创建软链接
add_custom_command(TARGET NoMoreDay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets"
    COMMENT "Copying assets to output directory..."
)

add_custom_command(TARGET NoMoreDay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:spdlog>
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
    COMMENT "Copying dependency DLLs..."
)

# Set compile output encoding to UTF-8
if(MSVC)
    target_compile_options(NoMoreDay PRIVATE /utf-8)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # For GCC on Windows, ensure console output is UTF-8
    target_compile_definitions(NoMoreDay PRIVATE "_WIN32_WINNT=0x0600" "UNICODE" "_UNICODE")
endif()