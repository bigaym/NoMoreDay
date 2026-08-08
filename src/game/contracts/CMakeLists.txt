# NoMoreDayGameContractsCore: the game contract-headers INTERFACE target.
#
# Pure header contract surface: CombatEvents.hpp, CombatFormula.hpp,
# DamagePipelineTypes.hpp, and DamageResolutionHooks.hpp (all physically
# located under systems/combat per the design §5.1, reached via the src
# include root). It carries the shared compile surface (src include root,
# SYSTEM third-party includes, GRAPHICS_API_OPENGL_43,
# WIN32_LEAN_AND_MEAN/NOGDI/NOUSER/NOMINMAX, /arch:AVX2 /Oi /Ot /Ob2 /GF,
# COMBAT_DEFENSE_DEBUG / COMBAT_TELEMETRY_ENABLED) so bottom sub-targets and
# gameplay consumers compile identically to the pre-split game library.
#
# INTERFACE libraries cannot contain sources: the DamageResolutionHooks.cpp
# implementation lives in the NoMoreDayGameContracts STATIC library defined in
# contracts/impl (added after data/utils/stats/combat_v2 so it can link them).

add_library(NoMoreDayGameContractsCore INTERFACE)

target_include_directories(NoMoreDayGameContractsCore INTERFACE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayGameContractsCore SYSTEM INTERFACE
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayGameContractsCore INTERFACE
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)

if(WIN32)
    target_compile_definitions(NoMoreDayGameContractsCore INTERFACE GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayGameContractsCore INTERFACE
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayGameContractsCore INTERFACE
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayGameContractsCore INTERFACE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayGameContractsCore INTERFACE COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayGameContractsCore INTERFACE COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayGameContractsCore INTERFACE COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayGameContractsCore INTERFACE COMBAT_TELEMETRY_ENABLED=0)
endif()

add_dependencies(NoMoreDayGameContractsCore GenerateTags)
