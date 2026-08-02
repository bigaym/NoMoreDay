# Engine layer: rendering, GPU, resource, physics, and VFX systems.
# PUBLIC definitions, include paths, and system libraries propagate up to the
# Game, App, and executable targets (behavioral equivalent of the former
# aggregate NoMoreDayCore target).
add_library(NoMoreDayEngine STATIC
    physics/SIMDSpatialGrid.cpp
    render/core/AdaptiveQualityController.cpp
    render/core/BindingRegistry.cpp
    render/core/DeviceCapabilityMatrix.cpp
    render/core/QualityTierManager.cpp
    render/debug/GPUTimerQueryRing.cpp
    render/debug/ProfilerHudRenderer.cpp
    render/debug/RenderProfiler.cpp
    render/debug/ShaderReloadGovernance.cpp
    render/dev/ShaderHotReloadManager.cpp
    render/gi/JFADistanceFieldEvaluator.cpp
    render/GPUABIContract.cpp
    render/GPUEntitySystem.cpp
    render/GPUFlowFieldSystem.cpp
    render/GPULootSystem.cpp
    render/GPUParticleSystem.cpp
    render/GPUSkillEffectSystem.cpp
    render/GPUTextSystem.cpp
    render/GPUUtils.cpp
    render/graph/RenderGraph.cpp
    render/lighting/ClusteredLightingState.cpp
    render/lighting/GlobalHeightField.cpp
    render/lighting/LightManager.cpp
    render/LootTextBatcher.cpp
    render/MaterialManager.cpp
    render/MDIRenderer.cpp
    render/particle/ForceFieldManager.cpp
    render/particle/ParticleTextureManager.cpp
    render/passes/CompositePass.cpp
    render/passes/DistortionPass.cpp
    render/passes/FluidSimulationPass.cpp
    render/passes/GICompositePass.cpp
    render/passes/GPULootPass.cpp
    render/passes/GPUTextPass.cpp
    render/passes/HeightShadowPass.cpp
    render/passes/JFAPass.cpp
    render/passes/LightCullingPass.cpp
    render/passes/LightingPass.cpp
    render/passes/OccluderExtractPass.cpp
    render/passes/PostProcessPass.cpp
    render/passes/RadianceCascadesPass.cpp
    render/passes/ScenePass.cpp
    render/passes/ShadowBuildPass.cpp
    render/passes/ShadowPreparePass.cpp
    render/passes/ShadowResolvePass.cpp
    render/passes/UIWorldPass.cpp
    render/passes/VFXEmissionSnapshotPass.cpp
    render/passes/VFXPass.cpp
    render/passes/VolumetricLightPass.cpp
    render/PersistentBuffer.cpp
    render/PopupRenderer.cpp
    render/RenderSystem.cpp
    render/resource/MSDFAtlasLoader.cpp
    render/resource/TextureArrayManager.cpp
    render/resources/FramebufferManager.cpp
    render/resources/FullscreenQuad.cpp
    render/resources/GPUResourceRegistry.cpp
    render/resources/TransientResourcePool.cpp
    render/shadow/OccluderCollector.cpp
    render/shadow/ShadowAtlasAllocator.cpp
    render/shadow/ShadowSDFMath.cpp
    render/trail/GPUTrailRenderer.cpp
    render/validation/GPUHardwareValidationGate.cpp
    resource/AssetLoadingSystem.cpp
    resource/ResourceManager.cpp
    vfx/VFXBudgetEstimator.cpp
    vfx/VFXSequenceManager.cpp
)

# ResourceManager.cpp performs raw OS file I/O and must not enter the PCH or
# Unity build units.
set_source_files_properties(resource/ResourceManager.cpp PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
    SKIP_UNITY_BUILD_INCLUSION ON
)

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
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)
target_link_libraries(NoMoreDayEngine PRIVATE NoMoreDayAnalyzeFlags)

set_target_properties(NoMoreDayEngine PROPERTIES UNITY_BUILD OFF)

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
