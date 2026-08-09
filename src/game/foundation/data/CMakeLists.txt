# NoMoreDayGameData: the game data-registry static library (bottom layer).
#
# Owns the data registries (Astrolabe, Biome, BladeMastery, Buff, MonsterAffix,
# Resonance, Skill, TalentLoader) plus TalentLayoutService, which was moved
# here from systems/skill in M3.5 to break the data<->skill cycle. Data has no
# contract-header dependency and does not reference combat or skill systems.

add_library(NoMoreDayGameData STATIC
    AstrolabeRegistry.cpp
    BiomeRegistry.cpp
    BladeMasteryRegistry.cpp
    BuffRegistry.cpp
    MonsterAffixRegistry.cpp
    ResonanceCalculator.cpp
    SkillRegistry.cpp
    TalentLayoutService.cpp
    TalentLoader.cpp
)

target_include_directories(NoMoreDayGameData PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayGameData SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayGameData
    PUBLIC
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)
target_link_libraries(NoMoreDayGameData PRIVATE NoMoreDayAnalyzeFlags)

if(WIN32)
    target_compile_definitions(NoMoreDayGameData PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayGameData PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayGameData PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayGameData PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayGameData PUBLIC COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayGameData PUBLIC COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayGameData PUBLIC COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayGameData PUBLIC COMBAT_TELEMETRY_ENABLED=0)
endif()

target_precompile_headers(NoMoreDayGameData PRIVATE ${CMAKE_SOURCE_DIR}/src/game/pch.hpp)

add_dependencies(NoMoreDayGameData GenerateTags)
