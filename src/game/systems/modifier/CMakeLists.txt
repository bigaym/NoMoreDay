# NoMoreDayGameModifier: the modifier adapter / evaluator static library
# (middle layer, M4). Owns EquipmentModifierAdapter, MapModifierAdapter,
# ModifierEvaluator, ModifierRuntimeRegistry, ModifierSchemaV2Validation,
# MonsterModifierAdapter, SkillSpecModifierAdapter, TalentModifierAdapter.
# Depends only on the data sub-target below it (design §5.4 middle band).

add_library(NoMoreDayGameModifier STATIC
    EquipmentModifierAdapter.cpp
    MapModifierAdapter.cpp
    ModifierEvaluator.cpp
    ModifierRuntimeRegistry.cpp
    ModifierSchemaV2Validation.cpp
    MonsterModifierAdapter.cpp
    SkillSpecModifierAdapter.cpp
    TalentModifierAdapter.cpp
)

target_include_directories(NoMoreDayGameModifier PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayGameModifier SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayGameModifier
    PUBLIC
    NoMoreDayGameData
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)
target_link_libraries(NoMoreDayGameModifier PRIVATE NoMoreDayAnalyzeFlags)

if(WIN32)
    target_compile_definitions(NoMoreDayGameModifier PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayGameModifier PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayGameModifier PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayGameModifier PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayGameModifier PUBLIC COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayGameModifier PUBLIC COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayGameModifier PUBLIC COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayGameModifier PUBLIC COMBAT_TELEMETRY_ENABLED=0)
endif()

target_precompile_headers(NoMoreDayGameModifier PRIVATE ${CMAKE_SOURCE_DIR}/src/game/pch.hpp)

add_dependencies(NoMoreDayGameModifier GenerateTags)
