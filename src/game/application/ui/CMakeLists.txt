# NoMoreDayGameUi: the UI static library (top layer, M4).
# Owns GameUiHost, AstrolabeController, AstrolabeRenderer, BladeMasteryUITheme,
# MonsterHealthBarSystem, PlayerHUD, SwordIntentWidget, UIAnimationSystem,
# UIPanelDragService, UIRenderer, UISkillHub, UISkillSpecRenderer,
# UISkillTalentTree, UISystem.
# (The legacy static panels UIStash/UICrafting/UICharacter/UIMinimap and the
# legacy UISystem::State / UIContext static context were removed by the U8
# final wave; every panel draws through its hosted instance controller.)
# UI is the widest dependency fan: combat (DamagePipeline/ProgressionSystem),
# item, skill, world, data, persistence (SharedStash), stats (AttributePipeline),
# contracts (CombatAntiMeta), ui_shared (UiShared: rarity colors). Depends on
# skill/item/world/combat/data/persistence/contracts/stats/ui_shared + engine
# (design §5.4 top band).
# The former ui<->render edges are both replaced by ui_shared (design §5.3
# ring 2 break): ui writes UiShared state, render reads it.

add_library(NoMoreDayGameUi STATIC
    AstrolabeController.cpp
    AstrolabeRenderer.cpp
    BladeMasteryUITheme.cpp
    GameUiCommandHandler.cpp
    GameUiHost.cpp
    GameUiSnapshotBuilder.cpp
    MonsterHealthBarController.cpp
    MonsterHealthBarSystem.cpp
    MinimapController.cpp
    OverlayController.cpp
    PlayerHUD.cpp
    PlayerHudController.cpp
    SkillHotbarController.cpp
    SkillTreeController.cpp
    SwordIntentWidget.cpp
    TooltipController.cpp
    UIAnimationSystem.cpp
    UICharacterController.cpp
    UICraftingController.cpp
    UIInventoryController.cpp
    UIPanelDragService.cpp
    UIRenderer.cpp
    UISkillHub.cpp
    UISkillSpecRenderer.cpp
    UISkillTalentTree.cpp
    UIStashController.cpp
    UISystem.cpp
    UiDrawList.cpp
    UiLayout.cpp
    UiRaylibBackend.cpp
    UiRuntime.cpp
    UiTooltipController.cpp
    UiViewport.cpp
    WorldUiFrame.cpp
)

target_include_directories(NoMoreDayGameUi PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayGameUi SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayGameUi
    PUBLIC
    NoMoreDayGameSkill
    NoMoreDayGameItem
    NoMoreDayGameUiShared
    NoMoreDayGameWorld
    NoMoreDayGameCombat
    NoMoreDayGameData
    NoMoreDayGamePersistence
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
target_link_libraries(NoMoreDayGameUi PRIVATE NoMoreDayAnalyzeFlags)

if(WIN32)
    target_compile_definitions(NoMoreDayGameUi PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayGameUi PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayGameUi PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayGameUi PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayGameUi PUBLIC COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayGameUi PUBLIC COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayGameUi PUBLIC COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayGameUi PUBLIC COMBAT_TELEMETRY_ENABLED=0)
endif()

target_precompile_headers(NoMoreDayGameUi PRIVATE ${CMAKE_SOURCE_DIR}/src/game/pch.hpp)

add_dependencies(NoMoreDayGameUi GenerateTags)
