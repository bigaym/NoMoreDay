# NoMoreDayEngineSystems: RenderSystem (the render aggregation point) plus the
# render-root GPU entity/particle/loot/skill-effect/text systems and the trail
# renderer. Depends on RenderCore + Resource + Passes.
#
# R1 scan result (recorded at M2 acceptance): render/RenderSystem.cpp directly
# includes every render/passes/* header (CompositePass through VFXPass), so
# Systems DOES link NoMoreDayEnginePasses. Link graph stays acyclic:
# RenderCore <- Resource <- Passes <- Systems.

add_library(NoMoreDayEngineSystems STATIC
    ${CMAKE_SOURCE_DIR}/src/engine/render/GPUEntitySystem.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/GPUFlowFieldSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/GPULootSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/GPUParticleSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/GPUSkillEffectSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/GPUTextSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/GlyphCache.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/LootTextBatcher.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/PopupRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/RenderSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/trail/GPUTrailRenderer.cpp
)

target_link_libraries(NoMoreDayEngineSystems
    PUBLIC
    NoMoreDayEngineRenderCore
    NoMoreDayEngineResource
    NoMoreDayEnginePasses
)
target_link_libraries(NoMoreDayEngineSystems PRIVATE NoMoreDayAnalyzeFlags)

target_precompile_headers(NoMoreDayEngineSystems PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.hpp)

add_dependencies(NoMoreDayEngineSystems GenerateTags)
