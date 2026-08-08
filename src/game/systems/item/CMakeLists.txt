# NoMoreDayGameItem: the item / inventory static library (middle layer, M4).
# Owns CraftingSystem, DropSystem, FragmentDropSystem, InventorySystem,
# ItemEquipValidationService, ItemFactory, LootFilter, LootGridSystem,
# MaterialRegistry, RunewordSystem, SalvageSystem, StashSystem, plus
# SharedStash (moved from persistence/ in M4 ring 1: breaks item<->persistence,
# design §5.3 Phase C). Depends on contracts/data + engine (design §5.4 middle
# band). Reverse edges to ui/render/world are intentionally omitted; those
# symbols resolve through the aggregate.

add_library(NoMoreDayGameItem STATIC
    CraftingSystem.cpp
    DropSystem.cpp
    FragmentDropSystem.cpp
    InventorySystem.cpp
    ItemEquipValidationService.cpp
    ItemFactory.cpp
    LootFilter.cpp
    LootGridSystem.cpp
    MaterialRegistry.cpp
    RunewordSystem.cpp
    SalvageSystem.cpp
    SharedStash.cpp
    StashSystem.cpp
)

target_include_directories(NoMoreDayGameItem PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayGameItem SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayGameItem
    PUBLIC
    NoMoreDayGameContracts
    NoMoreDayGameData
    NoMoreDayGameUiShared
    NoMoreDayEngine
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)
target_link_libraries(NoMoreDayGameItem PRIVATE NoMoreDayAnalyzeFlags)

if(WIN32)
    target_compile_definitions(NoMoreDayGameItem PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayGameItem PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayGameItem PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayGameItem PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayGameItem PUBLIC COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayGameItem PUBLIC COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayGameItem PUBLIC COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayGameItem PUBLIC COMBAT_TELEMETRY_ENABLED=0)
endif()

target_precompile_headers(NoMoreDayGameItem PRIVATE ${CMAKE_SOURCE_DIR}/src/game/pch.hpp)

add_dependencies(NoMoreDayGameItem GenerateTags)
