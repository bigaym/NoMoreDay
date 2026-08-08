# NoMoreDayGameCombat: the combat static library (upper layer, M4).
# Owns the remaining systems/combat sources: AilmentEngine,
# BossFrameworkSystem, CombatHistorySystem, CombatSystem,
# DamageMitigationService, DamagePipeline, EffectSystem, EliteModifierSystem,
# EndgameModifierContract, HazardSystem, MovementStanceSystem,
# ProgressionSystem, VisualFXSystem, XPAwardingSystem.
# The combat-domain hooks already sunk into contracts
# (CombatAntiMeta/CombatEventDispatcher/CombatTelemetry/ProcBudgetManager/
# StatsSystem/DamageResolutionHooks) are NOT compiled here — they live in
# NoMoreDayGameContracts (contracts/impl). DamagePipeline/CombatSystem go
# through skill via hooks; hazard damage resolves against world.
# Depends on skill/world/stats/combat_v2/contracts/data/utils + engine.

add_library(NoMoreDayGameCombat STATIC
    AilmentEngine.cpp
    BossFrameworkSystem.cpp
    CombatHistorySystem.cpp
    CombatSystem.cpp
    DamageMitigationService.cpp
    DamagePipeline.cpp
    EffectSystem.cpp
    EliteModifierSystem.cpp
    EndgameModifierContract.cpp
    HazardSystem.cpp
    MovementStanceSystem.cpp
    ProgressionSystem.cpp
    VisualFXSystem.cpp
    XPAwardingSystem.cpp
)

target_include_directories(NoMoreDayGameCombat PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayGameCombat SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayGameCombat
    PUBLIC
    NoMoreDayGameContractsCore
    NoMoreDayGameContracts
    NoMoreDayGameData
    NoMoreDayGameUtils
    NoMoreDayGameStats
    NoMoreDayGameCombatV2
    NoMoreDayGameSkill
    NoMoreDayGameWorld
    NoMoreDayEngine
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)
target_link_libraries(NoMoreDayGameCombat PRIVATE NoMoreDayAnalyzeFlags)

if(WIN32)
    target_compile_definitions(NoMoreDayGameCombat PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayGameCombat PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayGameCombat PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayGameCombat PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayGameCombat PUBLIC COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayGameCombat PUBLIC COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayGameCombat PUBLIC COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayGameCombat PUBLIC COMBAT_TELEMETRY_ENABLED=0)
endif()

target_precompile_headers(NoMoreDayGameCombat PRIVATE ${CMAKE_SOURCE_DIR}/src/game/pch.hpp)

add_dependencies(NoMoreDayGameCombat GenerateTags)
