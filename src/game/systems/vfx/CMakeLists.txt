# NoMoreDayGameVfx: the VFX static library (middle layer, M4).
# Owns GhostSystem, HoloBladeRenderSystem, SwordIntentVisualSystem,
# TrailSystem (systems/vfx) plus VFXSequencerSystem.cpp (systems/vfx).
# Depends on engine only (design §5.4).

add_library(NoMoreDayGameVfx STATIC
    GhostSystem.cpp
    HoloBladeRenderSystem.cpp
    SwordIntentVisualSystem.cpp
    TrailSystem.cpp
    VFXSequencerSystem.cpp
)

target_include_directories(NoMoreDayGameVfx PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayGameVfx SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayGameVfx
    PUBLIC
    NoMoreDayEngine
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)
target_link_libraries(NoMoreDayGameVfx PRIVATE NoMoreDayAnalyzeFlags)

if(WIN32)
    target_compile_definitions(NoMoreDayGameVfx PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayGameVfx PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayGameVfx PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayGameVfx PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayGameVfx PUBLIC COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayGameVfx PUBLIC COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayGameVfx PUBLIC COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayGameVfx PUBLIC COMBAT_TELEMETRY_ENABLED=0)
endif()

target_precompile_headers(NoMoreDayGameVfx PRIVATE ${CMAKE_SOURCE_DIR}/src/game/pch.hpp)

add_dependencies(NoMoreDayGameVfx GenerateTags)
