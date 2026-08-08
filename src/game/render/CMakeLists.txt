# NoMoreDayGameRender: the rendering static library (top layer, M4).
# Owns EmissiveStampAdapter, GameplayRenderAdapter, GPUEntitySync,
# GPULootAdapter, HeightFieldAdapter, LightAdapter, OccluderProjector
# (render/) plus systems/render/AirWallRenderer.cpp (absolute path).
# GameplayRenderAdapter talks to combat (MonsterAffixSystem), item
# (LootFilter), ui_shared (UiShared: loot grid / font / rarity / hovered item),
# vfx (HoloBladeRenderSystem/SwordIntent/TrailSystem), world
# (FogOfWarSystem/LevelManager), stats (AttributePipeline) and data
# (BiomeRegistry). Depends on combat/item/ui_shared/vfx/world/stats/data
# + engine (design §5.4 top band). The former render->ui edge is replaced by
# render->ui_shared (design §5.3 ring 2 break).

add_library(NoMoreDayGameRender STATIC
    EmissiveStampAdapter.cpp
    GameplayRenderAdapter.cpp
    GPUEntitySync.cpp
    GPULootAdapter.cpp
    HeightFieldAdapter.cpp
    LightAdapter.cpp
    OccluderProjector.cpp
    ${CMAKE_SOURCE_DIR}/src/game/systems/render/AirWallRenderer.cpp
)

target_include_directories(NoMoreDayGameRender PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayGameRender SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayGameRender
    PUBLIC
    NoMoreDayGameCombat
    NoMoreDayGameItem
    NoMoreDayGameUiShared
    NoMoreDayGameVfx
    NoMoreDayGameWorld
    NoMoreDayGameStats
    NoMoreDayGameData
    NoMoreDayEngine
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)
target_link_libraries(NoMoreDayGameRender PRIVATE NoMoreDayAnalyzeFlags)

if(WIN32)
    target_compile_definitions(NoMoreDayGameRender PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayGameRender PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayGameRender PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayGameRender PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayGameRender PUBLIC COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayGameRender PUBLIC COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayGameRender PUBLIC COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayGameRender PUBLIC COMBAT_TELEMETRY_ENABLED=0)
endif()

target_precompile_headers(NoMoreDayGameRender PRIVATE ${CMAKE_SOURCE_DIR}/src/game/pch.hpp)

add_dependencies(NoMoreDayGameRender GenerateTags)
