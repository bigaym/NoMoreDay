# NoMoreDayGameSkill: the skill / summon static library (middle layer, M4).
# Owns AstrolabeSystem, BehaviorInjectionRegistry, BladeMasteryService,
# BladeResourceService, ProjectileSystem, ShadowSystem,
# SkillCastConstraintService, SkillDisplayPreviewService, SkillSystem,
# SummonAISystem, SummonCombatBridge, SummonLifecycleSystem, SummonSystem.
# TalentLayoutService lives in data. Skill talks to combat only through the
# contract layer (design §5.4 middle band, hook inversion).
# Depends on contracts-core/contracts/combat_v2/data/modifier + engine.

add_library(NoMoreDayGameSkill STATIC
    AstrolabeSystem.cpp
    BehaviorInjectionRegistry.cpp
    BladeMasteryService.cpp
    BladeResourceService.cpp
    ProjectileSystem.cpp
    ShadowSystem.cpp
    SkillCastConstraintService.cpp
    SkillDisplayPreviewService.cpp
    SkillSystem.cpp
    SummonAISystem.cpp
    SummonCombatBridge.cpp
    SummonLifecycleSystem.cpp
    SummonSystem.cpp
)

target_include_directories(NoMoreDayGameSkill PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayGameSkill SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayGameSkill
    PUBLIC
    NoMoreDayGameContractsCore
    NoMoreDayGameContracts
    NoMoreDayGameData
    NoMoreDayGameCombatV2
    NoMoreDayGameModifier
    NoMoreDayEngine
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)
target_link_libraries(NoMoreDayGameSkill PRIVATE NoMoreDayAnalyzeFlags)

if(WIN32)
    target_compile_definitions(NoMoreDayGameSkill PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayGameSkill PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayGameSkill PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayGameSkill PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayGameSkill PUBLIC COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayGameSkill PUBLIC COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayGameSkill PUBLIC COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayGameSkill PUBLIC COMBAT_TELEMETRY_ENABLED=0)
endif()

target_precompile_headers(NoMoreDayGameSkill PRIVATE ${CMAKE_SOURCE_DIR}/src/game/pch.hpp)

add_dependencies(NoMoreDayGameSkill GenerateTags)
