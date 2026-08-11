# NoMoreDayGameInput: the input static library (top layer, M4).
# Owns InputSystem (systems/input). InputSystem routes skill input to
# SkillSystem and consumes the UI input capture for gameplay gating.
# Depends on skill + ui (design §5.4 top band).

add_library(NoMoreDayGameInput STATIC
    InputSystem.cpp
)

target_include_directories(NoMoreDayGameInput PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayGameInput SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayGameInput
    PUBLIC
    NoMoreDayGameSkill
    NoMoreDayGameUi
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)
target_link_libraries(NoMoreDayGameInput PRIVATE NoMoreDayAnalyzeFlags)

if(WIN32)
    target_compile_definitions(NoMoreDayGameInput PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayGameInput PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayGameInput PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayGameInput PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayGameInput PUBLIC COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayGameInput PUBLIC COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayGameInput PUBLIC COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayGameInput PUBLIC COMBAT_TELEMETRY_ENABLED=0)
endif()

target_precompile_headers(NoMoreDayGameInput PRIVATE ${CMAKE_SOURCE_DIR}/src/game/pch.hpp)

add_dependencies(NoMoreDayGameInput GenerateTags)
