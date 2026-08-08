# NoMoreDayGameContracts: the game contract-layer static library.
#
# Owns the contract implementations that gameplay domains depend on through
# headers only: CombatEventDispatcher, CombatTelemetry, ProcBudgetManager,
# CombatAntiMeta, StatsSystem, and DamageResolutionHooks (M3.2/M3.3). The
# contract headers themselves (CombatEvents.hpp, CombatFormula.hpp,
# DamagePipelineTypes.hpp, DamageResolutionHooks.hpp) are carried by the
# NoMoreDayGameContractsCore INTERFACE target in contracts/.
#
# This directory is added AFTER data/utils/stats/combat_v2 by the parent so it
# can PUBLIC-link those bottom sub-targets (design §5.3 Phase A, R3: the
# contract layer sits above data). StatsSystem.cpp additionally references
# engine render symbols (GPUParticleSystem) and registry/GroupRegistry.hpp from
# the aggregate; NoMoreDayEngine is linked PRIVATE so the symbol resolves at
# final link without exposing engine APIs through the contract interface.

add_library(NoMoreDayGameContracts STATIC
    ${CMAKE_SOURCE_DIR}/src/game/systems/combat/CombatAntiMeta.cpp
    ${CMAKE_SOURCE_DIR}/src/game/systems/combat/CombatEventDispatcher.cpp
    ${CMAKE_SOURCE_DIR}/src/game/systems/combat/CombatTelemetry.cpp
    ${CMAKE_SOURCE_DIR}/src/game/systems/combat/ProcBudgetManager.cpp
    ${CMAKE_SOURCE_DIR}/src/game/systems/combat/StatsSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/game/systems/combat/DamageResolutionHooks.cpp
)

target_include_directories(NoMoreDayGameContracts PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayGameContracts SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayGameContracts
    PUBLIC
    NoMoreDayGameContractsCore
    NoMoreDayGameData
    NoMoreDayGameUtils
    NoMoreDayGameStats
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)
target_link_libraries(NoMoreDayGameContracts PRIVATE NoMoreDayEngine)
target_link_libraries(NoMoreDayGameContracts PRIVATE NoMoreDayAnalyzeFlags)

if(WIN32)
    target_compile_definitions(NoMoreDayGameContracts PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayGameContracts PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayGameContracts PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayGameContracts PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayGameContracts PUBLIC COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayGameContracts PUBLIC COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayGameContracts PUBLIC COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayGameContracts PUBLIC COMBAT_TELEMETRY_ENABLED=0)
endif()

target_precompile_headers(NoMoreDayGameContracts PRIVATE ${CMAKE_SOURCE_DIR}/src/game/pch.hpp)

add_dependencies(NoMoreDayGameContracts GenerateTags)
