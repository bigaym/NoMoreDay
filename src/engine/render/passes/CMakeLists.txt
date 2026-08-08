# NoMoreDayEnginePasses: the 21 render passes. Depends on RenderCore (graph,
# resources, lighting, GPU utils) and Resource (asset/loading managers); both
# are reached via the PUBLIC link surface of NoMoreDayEngineRenderCore and
# NoMoreDayEngineResource. The shared compile surface (src include root,
# GRAPHICS_API_OPENGL_43, AVX2 flags, COMBAT_* definitions, platform link
# libraries) is inherited from NoMoreDayEngineRenderCore.

add_library(NoMoreDayEnginePasses STATIC
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/CompositePass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/DistortionPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/FluidSimulationPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/GICompositePass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/GPULootPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/GPUTextPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/HeightShadowPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/JFAPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/LightCullingPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/LightingPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/OccluderExtractPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/PostProcessPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/RadianceCascadesPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/ScenePass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/ShadowBuildPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/ShadowPreparePass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/ShadowResolvePass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/UIWorldPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/VFXEmissionSnapshotPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/VFXPass.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/passes/VolumetricLightPass.cpp
)

target_link_libraries(NoMoreDayEnginePasses
    PUBLIC
    NoMoreDayEngineRenderCore
    NoMoreDayEngineResource
)
target_link_libraries(NoMoreDayEnginePasses PRIVATE NoMoreDayAnalyzeFlags)

target_precompile_headers(NoMoreDayEnginePasses PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.hpp)

add_dependencies(NoMoreDayEnginePasses GenerateTags)
