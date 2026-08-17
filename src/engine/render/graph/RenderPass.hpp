#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace NoMoreDay::render::graph {

class RenderGraphBuilder;
struct RenderContext;

// Single source of truth for render pass identity. Profiler, contract-stage
// validation, budget tables and stable-pass-id derivation all key off this
// enum; the string name tables below are the only remaining string surface.
enum class RenderPassType : uint8_t {
  Scene = 0,
  Lighting,
  HeightShadow,
  OccluderExtract,
  JFA,
  RadianceCascades,
  GIComposite,
  FluidSimulation,
  Volumetric,
  VFX,
  GPUText,
  GPULoot,
  UIWorld,
  PostProcess,
  Distortion,
  Composite,
  LightCulling,
  ShadowPrepare,
  ShadowBuild,
  ShadowResolve,
  Count,
};

struct RenderPassNameInfo {
  std::string_view display; // HUD / log short name.
  std::string_view full;    // Canonical "XxxPass" name (stable-pass-id input).
};

inline constexpr std::array<RenderPassNameInfo,
                            static_cast<size_t>(RenderPassType::Count)>
    kRenderPassNames = {{
        {"Scene", "ScenePass"},
        {"Lighting", "LightingPass"},
        {"HeightShadow", "HeightShadowPass"},
        {"OccluderExtract", "OccluderExtractPass"},
        {"JFA", "JFAPass"},
        {"RadianceCascades", "RadianceCascadesPass"},
        {"GIComposite", "GICompositePass"},
        {"FluidSimulation", "FluidSimulationPass"},
        {"Volumetric", "VolumetricLightPass"},
        {"VFX", "VFXPass"},
        {"GPUText", "GPUTextPass"},
        {"GPULoot", "GPULootPass"},
        {"UIWorld", "UIWorldPass"},
        {"PostProcess", "PostProcessPass"},
        {"Distortion", "DistortionPass"},
        {"Composite", "CompositePass"},
        {"LightCulling", "LightCullingPass"},
        {"ShadowPrepare", "ShadowPreparePass"},
        {"ShadowBuild", "ShadowBuildPass"},
        {"ShadowResolve", "ShadowResolvePass"},
    }};

class RenderPass {
public:
  virtual ~RenderPass() = default;
  virtual void Setup(RenderGraphBuilder &builder) = 0;
  virtual void Execute(RenderContext &context) = 0;
  virtual const char *GetName() const = 0;
  [[nodiscard]] virtual RenderPassType Type() const = 0;
  [[nodiscard]] virtual bool HasSideEffects() const { return false; }
  virtual void OnResize(int width, int height) {}
};

} // namespace NoMoreDay::render::graph
