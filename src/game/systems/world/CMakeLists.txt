# NoMoreDayGameWorld: the world / map static library (middle layer, M4).
# Owns BiomeMapGenerator, EnemySpawnSystem, FogOfWarSystem, LevelManager,
# MapAffixCalculator, MapAffixRegistry, MapSystem, MazeBiomeStrategy,
# MosaicMapGenerator, OpenBiomeStrategy, PortalSystem, SpecialBiomeStrategy,
# TilemapCollisionSystem. MovementStanceSystem lives in systems/combat.
# PortalSystem saves through NoMoreDayGamePersistence (ring 3 fix).
# Depends on data/utils/contracts/persistence + engine (design §5.4 middle band).

add_library(NoMoreDayGameWorld STATIC
    BiomeMapGenerator.cpp
    EnemySpawnSystem.cpp
    FogOfWarSystem.cpp
    LevelManager.cpp
    MapAffixCalculator.cpp
    MapAffixRegistry.cpp
    MapSystem.cpp
    MazeBiomeStrategy.cpp
    MosaicMapGenerator.cpp
    OpenBiomeStrategy.cpp
    PortalSystem.cpp
    SpecialBiomeStrategy.cpp
    TilemapCollisionSystem.cpp
)

target_include_directories(NoMoreDayGameWorld PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayGameWorld SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayGameWorld
    PUBLIC
    NoMoreDayGameData
    NoMoreDayGameUtils
    NoMoreDayGameContracts
    NoMoreDayGamePersistence
    NoMoreDayEngine
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)
target_link_libraries(NoMoreDayGameWorld PRIVATE NoMoreDayAnalyzeFlags)

if(WIN32)
    target_compile_definitions(NoMoreDayGameWorld PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayGameWorld PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayGameWorld PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayGameWorld PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayGameWorld PUBLIC COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayGameWorld PUBLIC COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayGameWorld PUBLIC COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayGameWorld PUBLIC COMBAT_TELEMETRY_ENABLED=0)
endif()

target_precompile_headers(NoMoreDayGameWorld PRIVATE ${CMAKE_SOURCE_DIR}/src/game/pch.hpp)

add_dependencies(NoMoreDayGameWorld GenerateTags)
