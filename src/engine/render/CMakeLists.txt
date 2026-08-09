# NoMoreDayEngineRenderCore: the render-core static library (GPU
# infrastructure, shared render state, graph, lighting, shadow, particle, and
# low-level helpers). Systems and Passes live in their own sub-project
# directories (see systems/ and passes/).
#
# This target is the PUBLIC source of the shared compile surface (src include
# root, third-party SYSTEM includes, GRAPHICS_API_OPENGL_43,
# WIN32_LEAN_AND_MEAN/NOGDI/NOUSER/NOMINMAX, /arch:AVX2 /Oi /Ot /Ob2 /GF,
# COMBAT_DEFENSE_DEBUG / COMBAT_TELEMETRY_ENABLED, and the platform link
# libraries winmm/opengl32/dbghelp). The other Engine sub-targets reach it by
# linking this target PUBLICly; the aggregate NoMoreDayEngine re-exports it so
# Game/App/tests receive the identical compile environment as before the split.

add_library(NoMoreDayEngineRenderCore STATIC
    SIMDSpatialGrid.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/core/AdaptiveQualityController.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/core/BindingRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/core/DeviceCapabilityMatrix.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/core/QualityTierManager.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/debug/GLDebugCallback.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/debug/GPUTimerQueryRing.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/debug/ProfilerHudRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/debug/RenderProfiler.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/debug/ShaderReloadGovernance.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/gi/JFADistanceFieldEvaluator.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/graph/RenderGraph.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/lighting/ClusteredLightingState.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/lighting/GlobalHeightField.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/lighting/LightManager.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/particle/ForceFieldManager.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/particle/ParticleTextureManager.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/resource/MSDFAtlasLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/resource/TextureArrayManager.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/resources/FramebufferManager.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/resources/FullscreenQuad.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/resources/GPUResourceRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/resources/TransientResourcePool.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/shadow/OccluderCollector.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/shadow/ShadowAtlasAllocator.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/shadow/ShadowSDFMath.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/shadow/StableLightIdTracker.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/validation/GPUHardwareValidationGate.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/GPUABIContract.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/GPUUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/MaterialManager.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/MDIRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/PersistentBuffer.cpp
)

target_include_directories(NoMoreDayEngineRenderCore PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(NoMoreDayEngineRenderCore SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src/external/glfw/include
)

target_link_libraries(NoMoreDayEngineRenderCore
    PUBLIC
    NoMoreDayCore
    raylib
    glfw
    spdlog::spdlog
    EnTT::EnTT
    Taskflow
    xsimd
)
target_link_libraries(NoMoreDayEngineRenderCore PRIVATE NoMoreDayAnalyzeFlags)

if(WIN32)
    # winmm/opengl32 are also provided by raylib's interface; kernel32/gdi32/
    # user32/shell32 are MSVC default libraries auto-linked after raylib, so
    # listing user32 here would pull its CloseWindow before raylib's and cause
    # an LNK2005 duplicate.
    target_link_libraries(NoMoreDayEngineRenderCore PUBLIC winmm opengl32 dbghelp)
    target_compile_definitions(NoMoreDayEngineRenderCore PUBLIC GRAPHICS_API_OPENGL_43)

    if(MSVC)
        # RelWithDebInfo defaults to /Ob1; upgrade to /Ob2 to avoid D9025 warnings.
        target_compile_options(NoMoreDayEngineRenderCore PUBLIC
            $<$<CONFIG:Release>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2 /Oi /Ot /Ob2 /GF>
        )
        if(ENABLE_RUNTIME_OPT)
            target_compile_options(NoMoreDayEngineRenderCore PUBLIC
                $<$<CONFIG:Release>:/Gw /Gy>
            )
        endif()
    endif()

    target_compile_definitions(NoMoreDayEngineRenderCore PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(COMBAT_DEFENSE_DEBUG)
    target_compile_definitions(NoMoreDayEngineRenderCore PUBLIC COMBAT_DEFENSE_DEBUG=1)
else()
    target_compile_definitions(NoMoreDayEngineRenderCore PUBLIC COMBAT_DEFENSE_DEBUG=0)
endif()

if(COMBAT_TELEMETRY_ENABLED)
    target_compile_definitions(NoMoreDayEngineRenderCore PUBLIC COMBAT_TELEMETRY_ENABLED=1)
else()
    target_compile_definitions(NoMoreDayEngineRenderCore PUBLIC COMBAT_TELEMETRY_ENABLED=0)
endif()

target_precompile_headers(NoMoreDayEngineRenderCore PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.hpp)

add_dependencies(NoMoreDayEngineRenderCore GenerateTags)
