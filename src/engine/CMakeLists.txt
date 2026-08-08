# Engine layer: rendering, GPU, resource, physics, and VFX systems.
#
# NoMoreDayEngine is a source-free aggregate over the five Engine sub-projects.
# It PUBLIC-links every sub-target so the shared compile surface (src include
# root, SYSTEM third-party includes, GRAPHICS_API_OPENGL_43,
# WIN32_LEAN_AND_MEAN/NOGDI/NOUSER/NOMINMAX, /arch:AVX2 /Oi /Ot /Ob2 /GF,
# COMBAT_DEFENSE_DEBUG / COMBAT_TELEMETRY_ENABLED, winmm/opengl32/dbghelp)
# propagates up to the Game, App, and executable targets — the behavioral
# equivalent of the former single NoMoreDayEngine static library.
#
# Sub-project dependency order (bottom-up, acyclic):
#   NoMoreDayEngineRenderCore
#     <- NoMoreDayEngineResource
#       <- NoMoreDayEnginePasses
#         <- NoMoreDayEngineSystems   (R1 scan: RenderSystem drives passes)
#   NoMoreDayEngineVfx                (links RenderCore only)
#     <- NoMoreDayEngine (aggregate)
#
# The subdirectory order below mirrors that dependency order so each
# target_link_libraries references an already-defined target.

add_subdirectory(render)
add_subdirectory(resource)
add_subdirectory(render/passes)
add_subdirectory(render/systems)
add_subdirectory(vfx)

add_library(NoMoreDayEngine STATIC)

add_dependencies(NoMoreDayEngine GenerateTags)

target_precompile_headers(NoMoreDayEngine PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.hpp)

target_include_directories(NoMoreDayEngine PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayEngine SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayEngine
    PUBLIC
    NoMoreDayEngineRenderCore
    NoMoreDayEngineResource
    NoMoreDayEnginePasses
    NoMoreDayEngineSystems
    NoMoreDayEngineVfx
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)
target_link_libraries(NoMoreDayEngine PRIVATE NoMoreDayAnalyzeFlags)

if(WIN32)
    # winmm/opengl32 are also provided by raylib's interface; kernel32/gdi32/
    # user32/shell32 are MSVC default libraries auto-linked after raylib, so
    # listing user32 here would pull its CloseWindow before raylib's and cause
    # an LNK2005 duplicate.
    target_link_libraries(NoMoreDayEngine PUBLIC winmm opengl32 dbghelp)
    target_compile_definitions(NoMoreDayEngine PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayEngine PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayEngine PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayEngine PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayEngine PUBLIC COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayEngine PUBLIC COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayEngine PUBLIC COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayEngine PUBLIC COMBAT_TELEMETRY_ENABLED=0)
endif()
