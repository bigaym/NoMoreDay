#include "doctest.h"

#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/core/DeviceCapabilityMatrix.hpp"
#include "engine/render/debug/ShaderReloadGovernance.hpp"

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

class TestRenderPass : public RenderPass {
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
    std::string_view passName, std::string_view resourceName,
    std::string_view messageSnippet) {
  for (const auto &diagnostic : diagnostics) {
    if (diagnostic.severity !=
        NoMoreDay::render::graph::RenderGraph::ValidationDiagnostic::Severity::Error) {
      continue;
    }
    if (diagnostic.passName == passName &&
        diagnostic.resourceName == resourceName &&
        diagnostic.message.find(messageSnippet) != std::string::npos) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST_CASE("[Unit] RenderGraph - Valid Ownership Contract") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ScenePass", [](RenderGraphBuilder &builder) {
        builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
      }));
  graph.AddPass(std::make_shared<TestRenderPass>(
      "VFXPass", [](RenderGraphBuilder &builder) {
        builder.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::VFX);
        builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::VFX);
      }));
  graph.AddPass(std::make_shared<TestRenderPass>(
      "CompositePass", [](RenderGraphBuilder &builder) {
        builder.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Composite);
        builder.Write(RenderResourceTag::FinalOutputColor,
                      RenderOwnerTag::Composite);
      }));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
  CHECK(graph.GetValidationDiagnostics().empty());
}

TEST_CASE("[Unit] RenderGraph - Detect Read Before Write") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "PostProcessPass", [](RenderGraphBuilder &builder) {
        builder.Read(RenderResourceTag::SceneHdrColor,
                     RenderOwnerTag::PostProcess);
        builder.Write(RenderResourceTag::PostProcessLdrColor,
                      RenderOwnerTag::PostProcess);
      }));

#if defined(NDEBUG)
  CHECK_NOTHROW(graph.Build());
#else
  CHECK_THROWS_AS(graph.Build(), std::logic_error);
#endif
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(), "PostProcessPass",
                           "SceneColor", "read-before-write"));
}

TEST_CASE("[Unit] RenderGraph - Reject Invalid First Writer Owner") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "PostProcessPass", [](RenderGraphBuilder &builder) {
        builder.Write(RenderResourceTag::SceneHdrColor,
                      RenderOwnerTag::PostProcess);
      }));

#if defined(NDEBUG)
  CHECK_NOTHROW(graph.Build());
#else
  CHECK_THROWS_AS(graph.Build(), std::logic_error);
#endif
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(), "PostProcessPass",
                           "SceneColor", "first writer must be 'Scene'"));
}

TEST_CASE("[Unit] RenderGraph - Typed Descriptors and Compiled Plan") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ScenePass", [](RenderGraphBuilder &builder) {
        TypedResourceDescriptor sceneDesc;
        sceneDesc.name = "SceneColor";
        sceneDesc.tag = RenderResourceTag::SceneHdrColor;
        sceneDesc.kind = ResourceKind::Texture2D;
        sceneDesc.format = ResourceFormat::RGBA16F;
        sceneDesc.lifetime = ResourceLifetime::Transient;
        sceneDesc.ownerTag = RenderOwnerTag::Scene;
        builder.DeclareResource(sceneDesc);

        builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene,
                      PipelineStage::FramebufferAttachment, ResourceUsage::ColorAttachment);
      }));

  graph.AddPass(std::make_shared<TestRenderPass>(
      "VFXPass", [](RenderGraphBuilder &builder) {
        builder.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::VFX,
                     PipelineStage::Fragment, ResourceUsage::ShaderRead);
        builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::VFX,
                      PipelineStage::FramebufferAttachment, ResourceUsage::ColorAttachment);
      }));

  graph.AddPass(std::make_shared<TestRenderPass>(
      "CompositePass", [](RenderGraphBuilder &builder) {
        builder.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Composite,
                     PipelineStage::Fragment, ResourceUsage::ShaderRead);
        builder.Write(RenderResourceTag::FinalOutputColor, RenderOwnerTag::Composite,
                      PipelineStage::FramebufferAttachment, ResourceUsage::ColorAttachment);
      }));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());

  const auto &plan = graph.GetCompiledPlan();
  CHECK(plan.isValid);
  CHECK_EQ(plan.passOrder.size(), 3);
  CHECK_EQ(plan.passOrder[0], "ScenePass");
  CHECK_EQ(plan.passOrder[1], "VFXPass");
  CHECK_EQ(plan.passOrder[2], "CompositePass");

  CHECK(!plan.edges.empty());
  bool foundSceneToVfx = false;
  bool foundSceneToComposite = false;
  for (const auto &edge : plan.edges) {
    if (edge.producerPassName == "ScenePass" && edge.consumerPassName == "VFXPass" && edge.resourceName == "SceneColor") {
      foundSceneToVfx = true;
    }
    if (edge.producerPassName == "ScenePass" && edge.consumerPassName == "CompositePass" && edge.resourceName == "SceneColor") {
      foundSceneToComposite = true;
    }
  }
  CHECK(foundSceneToVfx);
  CHECK(foundSceneToComposite);

  std::string dump = graph.DumpCompiledPlan();
  CHECK(dump.find("CompiledRenderPlan Dump") != std::string::npos);
  CHECK(dump.find("ScenePass") != std::string::npos);
  CHECK(dump.find("SceneColor") != std::string::npos);
}

TEST_CASE("[Unit] RenderGraph - Phase 2 Barriers, Lifecycle, and Aliasing Toggles") {
  using namespace NoMoreDay::render::graph;

  // Test MapGlBarrierBits
  uint32_t computeToFrag = MapGlBarrierBits(PipelineStage::Compute, PassAccessMode::Write,
                                            PipelineStage::Fragment, PassAccessMode::Read,
                                            ResourceKind::Texture2D);
  CHECK_NE(computeToFrag, 0u);
  CHECK_EQ(computeToFrag & 0x00000020u, 0x00000020u); // Image access bit

  uint32_t ssboBarrier = MapGlBarrierBits(PipelineStage::Compute, PassAccessMode::Write,
                                          PipelineStage::Compute, PassAccessMode::Read,
                                          ResourceKind::StorageBuffer);
  CHECK_EQ(ssboBarrier, 0x00002000u); // SSBO barrier bit

  uint32_t readToRead = MapGlBarrierBits(PipelineStage::Fragment, PassAccessMode::Read,
                                         PipelineStage::Fragment, PassAccessMode::Read,
                                         ResourceKind::Texture2D);
  CHECK_EQ(readToRead, 0u);

  // Test Transient Aliasing toggle
  RenderGraph::SetTransientAliasingEnabled(true);
  CHECK(RenderGraph::IsTransientAliasingEnabled());
  RenderGraph::SetTransientAliasingEnabled(false);
  CHECK(!RenderGraph::IsTransientAliasingEnabled());

  // Test OnResize fan-out
  bool resized = false;
  RenderGraph graph;
  class ResizeTestPass : public TestRenderPass {
  public:
    ResizeTestPass(bool &flag) : TestRenderPass("ResizePass", nullptr), m_flag(flag) {}
    void OnResize(int w, int h) override { if (w == 1920 && h == 1080) m_flag = true; }
  private:
    bool &m_flag;
  };

  graph.AddPass(std::make_shared<ResizeTestPass>(resized));
  graph.OnResize(1920, 1080);
  CHECK(resized);
}

TEST_CASE("[Unit] RenderGraph - Phase 3 Resource Registry & Timer Query Ring") {
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::debug;
  using namespace NoMoreDay::render::graph;

  // Test Resource Registry
  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  registry.RegisterResource(101, ResourceKind::Texture2D, RenderOwnerTag::Scene, 1024 * 1024 * 4, "TestSceneTex");
  registry.RegisterResource(201, ResourceKind::StorageBuffer, RenderOwnerTag::Lighting, 2048, "TestLightSSBO");

  auto stats = registry.GetStats();
  CHECK_EQ(stats.activeCount, 2);
  CHECK_EQ(stats.currentTotalBytes, (1024 * 1024 * 4) + 2048);
  CHECK_EQ(stats.peakTotalBytes, stats.currentTotalBytes);

  registry.UpdateResourceSize(201, ResourceKind::StorageBuffer, 4096);
  stats = registry.GetStats();
  CHECK_EQ(stats.currentTotalBytes, (1024 * 1024 * 4) + 4096);

  std::string jsonReport = registry.GenerateReportJson();
  CHECK(jsonReport.find("TestSceneTex") != std::string::npos);
  CHECK(jsonReport.find("TestLightSSBO") != std::string::npos);

  registry.UnregisterResource(101, ResourceKind::Texture2D);
  stats = registry.GetStats();
  CHECK_EQ(stats.activeCount, 1);
  CHECK_EQ(stats.currentTotalBytes, 4096);

  // Test Timer Query Ring
  auto &timerRing = GPUTimerQueryRing::Get();
  timerRing.Initialize();
  timerRing.BeginFrame();
  timerRing.BeginPass(1);
  timerRing.EndPass(1);
  timerRing.EndFrame();

  auto res = timerRing.GetPassResult(1);
  CHECK((res.state == QueryState::Valid || res.state == QueryState::CpuFallback || res.state == QueryState::Pending));

  timerRing.Shutdown();
}

TEST_CASE("[Unit] RenderGraph - Phase 4 Capability Matrix & Shader Reload Governance") {
  using namespace NoMoreDay::render::core;
  using namespace NoMoreDay::render::debug;

  // Test Device Capability Matrix
  auto &caps = DeviceCapabilityMatrix::Get();
  auto report = caps.ProbeCapabilities();
  CHECK_EQ(report.maxSSBOBindings, 16);
  std::string dump = report.DumpReport();
  CHECK(dump.find("Device Capability Matrix") != std::string::npos);

  // Test Shader Reload Governance
  auto &reloadGov = ShaderReloadGovernance::Get();
  std::vector<std::string> chain;
  uint64_t hash = reloadGov.ComputeIncludeHash("assets/shaders/lighting/v5_gi_composite.comp", chain);
  CHECK_NE(hash, 0u);
  CHECK(!chain.empty());

  reloadGov.RecordReloadAttempt("assets/shaders/lighting/v5_gi_composite.comp", true, hash, chain, "GICompositePass", "SceneColor", "");
  auto record = reloadGov.GetRecord("assets/shaders/lighting/v5_gi_composite.comp");
  CHECK(record.isLastReloadSuccess);
  CHECK_EQ(record.lastSuccessfulHash, hash);

  reloadGov.RecordReloadAttempt("assets/shaders/lighting/v5_gi_composite.comp", false, 99999u, chain, "GICompositePass", "SceneColor", "Syntax Error: missing semicolon");
  record = reloadGov.GetRecord("assets/shaders/lighting/v5_gi_composite.comp");
  CHECK(!record.isLastReloadSuccess);
  CHECK_EQ(record.lastSuccessfulHash, hash); // Retains last successful hash!

  std::string diag = reloadGov.GenerateDiagnosticReport("assets/shaders/lighting/v5_gi_composite.comp");
  CHECK(diag.find("Syntax Error") != std::string::npos);
  reloadGov.Reset();
}




