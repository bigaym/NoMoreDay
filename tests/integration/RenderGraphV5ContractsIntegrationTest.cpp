#include "doctest.h"

#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/core/DeviceCapabilityMatrix.hpp"
#include "engine/render/debug/ShaderReloadGovernance.hpp"
#include "engine/render/passes/CompositePass.hpp"
#include "engine/render/passes/FluidSimulationPass.hpp"
#include "engine/render/passes/GICompositePass.hpp"
#include "engine/render/passes/HeightShadowPass.hpp"
#include "engine/render/passes/JFAPass.hpp"
#include "engine/render/passes/LightingPass.hpp"
#include "engine/render/passes/OccluderExtractPass.hpp"
#include "engine/render/passes/PostProcessPass.hpp"
#include "engine/render/passes/RadianceCascadesPass.hpp"
#include "engine/render/passes/ScenePass.hpp"
#include "engine/render/passes/UIWorldPass.hpp"
#include "engine/render/passes/VFXPass.hpp"
#include "engine/render/RenderSystem.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using NoMoreDay::render::graph::RenderContext;
using NoMoreDay::render::graph::RenderGraphBuilder;
using NoMoreDay::render::graph::RenderPass;

class TestRenderPass final : public RenderPass {
public:
  using SetupCallback = std::function<void(RenderGraphBuilder &)>;

  TestRenderPass(std::string name, SetupCallback setupCallback)
      : m_name(std::move(name)), m_setupCallback(std::move(setupCallback)) {}

  void Setup(RenderGraphBuilder &builder) override {
    if (m_setupCallback) {
      m_setupCallback(builder);
    }
  }

  void Execute(RenderContext &) override {}

  const char *GetName() const override { return m_name.c_str(); }

private:
  std::string m_name;
  SetupCallback m_setupCallback;
};

bool HasErrorContaining(
    const std::vector<NoMoreDay::render::graph::RenderGraph::ValidationDiagnostic>
        &diagnostics,
    std::string_view messageSnippet) {
  for (const auto &diagnostic : diagnostics) {
    if (diagnostic.severity !=
        NoMoreDay::render::graph::RenderGraph::ValidationDiagnostic::Severity::Error) {
      continue;
    }
    if (diagnostic.message.find(messageSnippet) != std::string::npos) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST_CASE("[Integration] RenderGraph V5 Contracts - JFA chain stays valid") {
  using namespace NoMoreDay::render;

  graph::RenderGraph graph;
  graph.AddPass(std::make_shared<passes::ScenePass>());
  graph.AddPass(std::make_shared<passes::LightingPass>());
  graph.AddPass(std::make_shared<passes::HeightShadowPass>());
  graph.AddPass(std::make_shared<passes::OccluderExtractPass>());
  graph.AddPass(std::make_shared<passes::JFAPass>());
  graph.AddPass(std::make_shared<passes::VFXPass>());
  graph.AddPass(std::make_shared<passes::UIWorldPass>());
  graph.AddPass(std::make_shared<passes::CompositePass>(
      graph::RenderResourceTag::SceneHdrColor, graph::RenderOwnerTag::UIWorld));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
  CHECK(graph.GetPassCount() == 8);
}

TEST_CASE("[Integration] RenderGraph V5 Contracts - DistanceField read-before-write detection") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ScenePass", [](RenderGraphBuilder &builder) {
        builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
      }));
  graph.AddPass(std::make_shared<TestRenderPass>(
      "RadianceProbePass", [](RenderGraphBuilder &builder) {
        builder.Read(RenderResourceTag::DistanceField,
                     RenderOwnerTag::RadianceCascades);
      }));

#if defined(NDEBUG)
  CHECK_NOTHROW(graph.Build());
#else
  CHECK_THROWS_AS(graph.Build(), std::logic_error);
#endif
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(), "read-before-write"));
}

TEST_CASE("[Integration] RenderGraph V5 Contracts - Radiance and GI composite chain stays valid") {
  using namespace NoMoreDay::render;

  graph::RenderGraph graph;
  graph.AddPass(std::make_shared<passes::ScenePass>());
  graph.AddPass(std::make_shared<passes::LightingPass>());
  graph.AddPass(std::make_shared<passes::HeightShadowPass>());
  graph.AddPass(std::make_shared<passes::OccluderExtractPass>());
  graph.AddPass(std::make_shared<passes::JFAPass>());
  graph.AddPass(std::make_shared<passes::RadianceCascadesPass>());
  graph.AddPass(std::make_shared<passes::GICompositePass>());
  graph.AddPass(std::make_shared<passes::FluidSimulationPass>());
  graph.AddPass(std::make_shared<passes::VFXPass>());
  graph.AddPass(std::make_shared<passes::UIWorldPass>());
  graph.AddPass(std::make_shared<passes::CompositePass>(
      graph::RenderResourceTag::SceneHdrColor, graph::RenderOwnerTag::UIWorld));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
  CHECK(graph.GetPassCount() == 11);
}

TEST_CASE("[Integration] Gameplay Offscreen Target - Full HDR GI Pass Matrix") {
  using namespace NoMoreDay::render;

  graph::RenderGraph graph;
  graph.AddPass(std::make_shared<passes::ScenePass>());
  graph.AddPass(std::make_shared<passes::LightingPass>());
  graph.AddPass(std::make_shared<passes::HeightShadowPass>());
  graph.AddPass(std::make_shared<passes::OccluderExtractPass>());
  graph.AddPass(std::make_shared<passes::JFAPass>());
  graph.AddPass(std::make_shared<passes::RadianceCascadesPass>());
  graph.AddPass(std::make_shared<passes::GICompositePass>());
  graph.AddPass(std::make_shared<passes::VFXPass>());
  graph.AddPass(std::make_shared<passes::UIWorldPass>());
  graph.AddPass(std::make_shared<passes::PostProcessPass>());
  graph.AddPass(std::make_shared<passes::CompositePass>(
      graph::RenderResourceTag::PostProcessLdrColor, graph::RenderOwnerTag::PostProcess));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
  CHECK(graph.GetPassCount() == 11);
}

TEST_CASE("[Integration] GI History Invalidation & Occluder Cache Invalidation Key") {
  using namespace NoMoreDay::render;

  passes::OccluderExtractPass occluderPass;
  passes::GICompositePass giPass;
  giPass.SetOccluderExtractPass(&occluderPass);

  CHECK(occluderPass.GetMaskVersion() >= 1u);
  CHECK(occluderPass.GetCameraInvalidateCount() == 0u);

  giPass.InvalidateHistory();
}

TEST_CASE("[Integration] Gameplay Offscreen Target Descriptor & State Guard") {
  using namespace NoMoreDay::render;

  OffscreenTargetDescriptor state = {};
  state.framebuffer = 12u;
  state.viewportX = 0;
  state.viewportY = 0;
  state.viewportWidth = 1920;
  state.viewportHeight = 1080;
  state.renderExtentWidth = 1920;
  state.renderExtentHeight = 1080;
  state.flipY = true;

  CHECK(state.framebuffer == 12u);
  CHECK(state.renderExtentWidth == 1920);
  CHECK(state.flipY);
}

TEST_CASE("[Integration] RenderGraph Phase 5 Compiled Plan & Observability Gate Integration") {
  using namespace NoMoreDay::render;
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::debug;
  using namespace NoMoreDay::render::core;

  graph::RenderGraph graph;
  graph.AddPass(std::make_shared<passes::ScenePass>());
  graph.AddPass(std::make_shared<passes::LightingPass>());
  graph.AddPass(std::make_shared<passes::HeightShadowPass>());
  graph.AddPass(std::make_shared<passes::OccluderExtractPass>());
  graph.AddPass(std::make_shared<passes::JFAPass>());
  graph.AddPass(std::make_shared<passes::RadianceCascadesPass>());
  graph.AddPass(std::make_shared<passes::GICompositePass>());
  graph.AddPass(std::make_shared<passes::VFXPass>());
  graph.AddPass(std::make_shared<passes::UIWorldPass>());
  graph.AddPass(std::make_shared<passes::PostProcessPass>());
  graph.AddPass(std::make_shared<passes::CompositePass>(
      graph::RenderResourceTag::PostProcessLdrColor, graph::RenderOwnerTag::PostProcess));

  CHECK_NOTHROW(graph.Build());
  const auto &plan = graph.GetCompiledPlan();
  CHECK(plan.isValid);
  CHECK_EQ(plan.passOrder.size(), 11);

  std::string planDump = graph.DumpCompiledPlan();
  CHECK(planDump.find("=== CompiledRenderPlan Dump ===") != std::string::npos);

  // Test full subsystem integration
  auto &registry = GPUResourceRegistry::Get();
  auto stats = registry.GetStats();
  CHECK(stats.peakTotalBytes >= stats.currentTotalBytes);

  auto &timerRing = GPUTimerQueryRing::Get();
  timerRing.BeginFrame();
  timerRing.EndFrame();

  auto &caps = DeviceCapabilityMatrix::Get();
  auto report = caps.ProbeCapabilities();
  CHECK(report.maxSSBOBindings > 0);

  auto &reloadGov = ShaderReloadGovernance::Get();
  auto failedRecords = reloadGov.GetFailedReloadRecords();
  CHECK(failedRecords.empty()); // No failed reloads initially
}


