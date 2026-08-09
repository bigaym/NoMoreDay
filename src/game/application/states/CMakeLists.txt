# NoMoreDayGameStates: the game states static library (top layer, M4).
# Owns DimensionalLevelSelectState, GameplayState, HeirloomVaultState,
# InventoryState, LoadingState, MainMenuState, MosaicEditorState,
# NightmareFloorState, PauseState, SettingsState (states/).
# States has the widest dependency fan: combat, skill, world, scene
# (StateManager/SceneManager), persistence (SaveManager), input, render,
# ui, item, physics, ai, vfx, data, contracts. Depends on all top targets
# + data/contracts/stats + engine (design §5.4 top band).

add_library(NoMoreDayGameStates STATIC
    DimensionalLevelSelectState.cpp
    GameplayState.cpp
    HeirloomVaultState.cpp
    InventoryState.cpp
    LoadingState.cpp
    MainMenuState.cpp
    MosaicEditorState.cpp
    NightmareFloorState.cpp
    PauseState.cpp
    SettingsState.cpp
)

target_include_directories(NoMoreDayGameStates PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayGameStates SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayGameStates
    PUBLIC
    NoMoreDayGameCombat
    NoMoreDayGameSkill
    NoMoreDayGameWorld
    NoMoreDayGameScene
    NoMoreDayGamePersistence
    NoMoreDayGameInput
    NoMoreDayGameRender
    NoMoreDayGameUi
    NoMoreDayGameUiShared
    NoMoreDayGameItem
    NoMoreDayGamePhysics
    NoMoreDayGameAi
    NoMoreDayGameVfx
    NoMoreDayGameData
    NoMoreDayGameContracts
    NoMoreDayGameStats
    NoMoreDayEngine
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)
target_link_libraries(NoMoreDayGameStates PRIVATE NoMoreDayAnalyzeFlags)

if(WIN32)
    target_compile_definitions(NoMoreDayGameStates PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayGameStates PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayGameStates PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayGameStates PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayGameStates PUBLIC COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayGameStates PUBLIC COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayGameStates PUBLIC COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayGameStates PUBLIC COMBAT_TELEMETRY_ENABLED=0)
endif()

target_precompile_headers(NoMoreDayGameStates PRIVATE ${CMAKE_SOURCE_DIR}/src/game/pch.hpp)

add_dependencies(NoMoreDayGameStates GenerateTags)
