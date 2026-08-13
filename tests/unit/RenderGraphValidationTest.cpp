#include "doctest.h"

#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/passes/FluidSimulationPass.hpp"
#include "engine/render/passes/GICompositePass.hpp"
#include "engine/render/passes/JFAPass.hpp"
#include "engine/render/passes/LightCullingPass.hpp"
#include "engine/render/passes/LightingPass.hpp"
#include "engine/render/passes/OccluderExtractPass.hpp"
#include "engine/render/passes/RadianceCascadesPass.hpp"
#include "engine/render/passes/ScenePass.hpp"
#include "engine/render/passes/ShadowBuildPass.hpp"
#include "engine/render/passes/ShadowPreparePass.hpp"
#include "engine/render/passes/ShadowResolvePass.hpp"
#include "engine/render/passes/VFXEmissionSnapshotPass.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/core/DeviceCapabilityMatrix.hpp"
#include "engine/render/debug/ShaderReloadGovernance.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
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

  // Name-driven simulation: a test pass reusing a canonical table name must
  // present the matching type so the name/type identity contract holds;
  // non-table names keep the generic Scene type.
  NoMoreDay::render::graph::RenderPassType Type() const override {
    using NoMoreDay::render::graph::kRenderPassNames;
    for (size_t i = 0; i < kRenderPassNames.size(); ++i) {
      if (m_name == kRenderPassNames[i].full) {
        return static_cast<NoMoreDay::render::graph::RenderPassType>(i);
      }
    }
    return NoMoreDay::render::graph::RenderPassType::Scene;
  }

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

TEST_CASE("[Unit] RenderGraph - legacy string-based access is denied by default") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ScenePass", [](RenderGraphBuilder &builder) {
        builder.Write("CustomColor");
      }));
  graph.AddPass(std::make_shared<TestRenderPass>(
      "CompositePass", [](RenderGraphBuilder &builder) {
        builder.Read("CustomColor");
      }));

  CHECK_THROWS_AS(graph.Build(), std::logic_error);
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(), "ScenePass",
                           "CustomColor", "string-based access is denied"));
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(), "CompositePass",
                           "CustomColor", "string-based access is denied"));
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

TEST_CASE("[Unit] RenderGraph - Compiled transitions retain identity and prior access") {
  using namespace NoMoreDay::render::graph;

  constexpr uint64_t resourceId = 0x12345678u;
  constexpr bool includeConditionalPass = true;
  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "WritePass", [resourceId](RenderGraphBuilder &builder) {
        TypedResourceDescriptor descriptor;
        descriptor.name = "BarrierResource";
        descriptor.kind = ResourceKind::StorageBuffer;
        descriptor.stableResourceId = resourceId;
        builder.DeclareResource(descriptor);

        TypedPassAccess access;
        access.resourceName = "BarrierResource";
        access.mode = PassAccessMode::Write;
        access.stage = PipelineStage::Compute;
        access.usageFlags = ResourceUsage::StorageWrite;
        access.stableResourceId = resourceId;
        builder.Write(access);
      }));
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ConditionalReadPass", [includeConditionalPass, resourceId](RenderGraphBuilder &builder) {
        if (includeConditionalPass) {
          TypedPassAccess access;
          access.resourceName = "BarrierResource";
          access.mode = PassAccessMode::Read;
          access.stage = PipelineStage::Fragment;
          access.usageFlags = ResourceUsage::StorageRead;
          access.stableResourceId = resourceId;
          builder.Read(access);
        }
      }));
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ReadWritePass", [resourceId](RenderGraphBuilder &builder) {
        TypedPassAccess read;
        read.resourceName = "BarrierResource";
        read.mode = PassAccessMode::Read;
        read.stage = PipelineStage::Fragment;
        read.usageFlags = ResourceUsage::StorageRead;
        read.stableResourceId = resourceId;
        builder.Read(read);

        TypedPassAccess write = read;
        write.mode = PassAccessMode::Write;
        write.stage = PipelineStage::Compute;
        write.usageFlags = ResourceUsage::StorageWrite;
        builder.Write(write);
      }));

  CHECK_NOTHROW(graph.Build());
  const auto &plan = graph.GetCompiledPlan();
  REQUIRE(plan.isValid);
  REQUIRE_EQ(plan.resources.size(), 1);
  CHECK_EQ(plan.resources[0].stableResourceId, resourceId);
  REQUIRE_EQ(plan.transitions.size(), 2);

  CHECK_EQ(plan.transitions[0].stableResourceId, resourceId);
  CHECK_EQ(plan.transitions[0].previousMode, PassAccessMode::Write);
  CHECK_EQ(plan.transitions[0].nextMode, PassAccessMode::Read);
  CHECK_EQ(plan.transitions[0].previousStage, PipelineStage::Compute);
  CHECK_EQ(plan.transitions[0].nextStage, PipelineStage::Fragment);
  CHECK_NE(plan.transitions[0].barrierBits, 0u);

  CHECK_EQ(plan.transitions[1].previousMode, PassAccessMode::Read);
  CHECK_EQ(plan.transitions[1].nextMode, PassAccessMode::Write);
  CHECK_EQ(plan.transitions[1].consumerPassIndex, 2);
  CHECK_EQ(plan.transitions[1].barrierBits, 0u);
  CHECK_EQ(plan.passOrder[1], "ConditionalReadPass");
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

TEST_CASE("[Unit] RenderGraph - S0 stable pass id is deterministic across reordering") {
  using namespace NoMoreDay::render::graph;

  auto buildIds = [](const std::vector<std::string> &order) {
    RenderGraph graph;
    for (const std::string &name : order) {
      graph.AddPass(std::make_shared<TestRenderPass>(
          name, [name](RenderGraphBuilder &builder) {
            TypedPassAccess access;
            access.resourceName = name + "Color";
            access.mode = PassAccessMode::Write;
            access.stage = PipelineStage::FramebufferAttachment;
            access.usageFlags = ResourceUsage::ColorAttachment;
            access.stableResourceId = StableResourceId(name + "Color");
            builder.Write(access);
          }));
    }
    CHECK_NOTHROW(graph.Build());
    const auto &plan = graph.GetCompiledPlan();
    std::map<std::string, uint32_t> ids;
    for (const auto &pass : plan.passes) {
      ids[pass.passName] = pass.stablePassId;
    }
    return ids;
  };

  const std::vector<std::string> orderA = {"AlphaPass", "BetaPass", "GammaPass"};
  const std::vector<std::string> orderB = {"GammaPass", "BetaPass", "AlphaPass"};

  const auto idsA = buildIds(orderA);
  const auto idsB = buildIds(orderB);

  REQUIRE_EQ(idsA.size(), 3);
  CHECK_EQ(idsA.at("AlphaPass"), idsB.at("AlphaPass"));
  CHECK_EQ(idsA.at("BetaPass"), idsB.at("BetaPass"));
  CHECK_EQ(idsA.at("GammaPass"), idsB.at("GammaPass"));
  for (const auto &[name, id] : idsA) {
    CHECK_NE(id, kInvalidStablePassId);
    CHECK_NE(id, kFrameLevelStablePassId);
  }
}

TEST_CASE("[Unit] RenderGraph - S0 conditional pass retains stable id without samples") {
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::render::debug;

  constexpr uint64_t resourceId = 0x12345678u;

  auto buildGraph = [resourceId](bool includeConditional) {
    RenderGraph graph;
    graph.AddPass(std::make_shared<TestRenderPass>(
        "WritePass", [resourceId](RenderGraphBuilder &builder) {
          TypedResourceDescriptor descriptor;
          descriptor.name = "BarrierResource";
          descriptor.kind = ResourceKind::StorageBuffer;
          descriptor.stableResourceId = resourceId;
          builder.DeclareResource(descriptor);

          TypedPassAccess access;
          access.resourceName = "BarrierResource";
          access.mode = PassAccessMode::Write;
          access.stage = PipelineStage::Compute;
          access.usageFlags = ResourceUsage::StorageWrite;
          access.stableResourceId = resourceId;
          builder.Write(access);
        }));
    graph.AddPass(std::make_shared<TestRenderPass>(
        "ConditionalReadPass",
        [includeConditional, resourceId](RenderGraphBuilder &builder) {
          if (includeConditional) {
            TypedPassAccess access;
            access.resourceName = "BarrierResource";
            access.mode = PassAccessMode::Read;
            access.stage = PipelineStage::Fragment;
            access.usageFlags = ResourceUsage::StorageRead;
            access.stableResourceId = resourceId;
            builder.Read(access);
          }
        }));
    graph.AddPass(std::make_shared<TestRenderPass>(
        "ReadWritePass", [resourceId](RenderGraphBuilder &builder) {
          TypedPassAccess read;
          read.resourceName = "BarrierResource";
          read.mode = PassAccessMode::Read;
          read.stage = PipelineStage::Fragment;
          read.usageFlags = ResourceUsage::StorageRead;
          read.stableResourceId = resourceId;
          builder.Read(read);

          TypedPassAccess write = read;
          write.mode = PassAccessMode::Write;
          write.stage = PipelineStage::Compute;
          write.usageFlags = ResourceUsage::StorageWrite;
          builder.Write(write);
        }));
    return graph;
  };

  RenderGraph included = buildGraph(true);
  CHECK_NOTHROW(included.Build());
  const auto &planIncluded = included.GetCompiledPlan();
  REQUIRE(planIncluded.isValid);

  uint32_t conditionalId = 0;
  uint32_t writePassId = 0;
  for (const auto &pass : planIncluded.passes) {
    if (pass.passName == "ConditionalReadPass") {
      conditionalId = pass.stablePassId;
    } else if (pass.passName == "WritePass") {
      writePassId = pass.stablePassId;
    }
  }
  CHECK_NE(conditionalId, 0u);
  CHECK_NE(conditionalId, kFrameLevelStablePassId);
  CHECK_NE(writePassId, conditionalId);

  RenderGraph excluded = buildGraph(false);
  CHECK_NOTHROW(excluded.Build());
  const auto &planExcluded = excluded.GetCompiledPlan();
  REQUIRE(planExcluded.isValid);
  uint32_t conditionalIdExcluded = 0;
  for (const auto &pass : planExcluded.passes) {
    if (pass.passName == "ConditionalReadPass") {
      conditionalIdExcluded = pass.stablePassId;
    }
  }
  CHECK_EQ(conditionalIdExcluded, conditionalId);

  auto &ring = GPUTimerQueryRing::Get();
  ring.Initialize();
  ring.BeginFrame();
  ring.BeginPass(writePassId);
  ring.EndPass(writePassId);
  ring.EndFrame();
  ring.PollReadyQueries();
  GPUTimerResult unexecuted = ring.GetPassResult(conditionalId);
  CHECK_EQ(unexecuted.state, QueryState::Pending);
  ring.Shutdown();
}

TEST_CASE("[Unit] RenderGraph - S0 duplicate canonical pass name fails closed") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>("Scene Pass", nullptr));
  graph.AddPass(std::make_shared<TestRenderPass>("ScenePass", nullptr));

  CHECK_THROWS_AS(graph.Build(), std::logic_error);
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(), "ScenePass",
                           "(identity)", "duplicate canonical pass name"));
}

TEST_CASE("[Unit] RenderGraph - S0 stable pass id hash collision fails closed") {
  using namespace NoMoreDay::render::graph;

  // "7cwukcfqenxf" and "2c0zx1a45" canonicalize to themselves and both hash
  // (FNV-1a 64 over salt "NMD-STABLEPASS-V1" ++ name) to 0x5541C207.
  CHECK_EQ(StablePassId("7cwukcfqenxf"), StablePassId("2c0zx1a45"));
  CHECK_EQ(StablePassId("7cwukcfqenxf"), 0x5541C207u);

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>("7cwukcfqenxf", nullptr));
  graph.AddPass(std::make_shared<TestRenderPass>("2c0zx1a45", nullptr));

  CHECK_THROWS_AS(graph.Build(), std::logic_error);
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(), "2c0zx1a45",
                           "(identity)", "hash collision"));
}

TEST_CASE("[Unit] RenderGraph - S0 reserved stable pass id fails closed") {
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::render::debug;

  // "cw4db1u" canonicalizes to itself and hashes to 0xFFFFFFFF, the reserved
  // frame-level id which MUST equal GPUTimerQueryRing::kFramePassId.
  CHECK_EQ(StablePassId("cw4db1u"), kFrameLevelStablePassId);
  CHECK_EQ(kFrameLevelStablePassId, GPUTimerQueryRing::kFramePassId);

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>("cw4db1u", nullptr));

  CHECK_THROWS_AS(graph.Build(), std::logic_error);
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(), "cw4db1u",
                           "(identity)", "reserved frame-level id"));
}

TEST_CASE("[Unit] RenderGraph - Phase B typed shadow/cluster access plan") {
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::render::passes;

  RenderGraph graph;
  graph.AddPass(std::make_shared<ScenePass>());
  graph.AddPass(std::make_shared<ShadowPreparePass>());
  graph.AddPass(std::make_shared<ShadowBuildPass>());
  graph.AddPass(std::make_shared<ShadowResolvePass>());
  graph.AddPass(std::make_shared<LightCullingPass>());
  // Test-only consumer: verifies the graph emits the cross-pass
  // Compute->Fragment transitions for every cluster SSBO written by
  // LightCullingPass (production consumer is LightingPass, migrated in
  // Phase C).
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ClusterConsumerPass", [](RenderGraphBuilder &builder) {
        for (const auto tag : {RenderResourceTag::ClusterHeaderSSBO,
                               RenderResourceTag::ClusterLightIndexSSBO,
                               RenderResourceTag::ClusterPackedLightSSBO,
                               RenderResourceTag::ClusterCounterSSBO}) {
          builder.Read(tag, RenderOwnerTag::LightCulling,
                       PipelineStage::Fragment, ResourceUsage::ShaderRead);
        }
      }));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
  const auto &plan = graph.GetCompiledPlan();
  REQUIRE(plan.isValid);

  // Every shadow/cluster resource must be present in the compiled plan.
  bool hasShadowAtlas = false;
  bool hasSdf = false;
  bool hasShadowMask = false;
  bool hasOccluder = false;
  bool hasClusterHeader = false;
  bool hasClusterLightIndex = false;
  bool hasClusterPackedLight = false;
  bool hasClusterCounter = false;
  for (const auto &res : plan.resources) {
    if (res.resourceName == "ShadowAtlas") {
      hasShadowAtlas = true;
    }
    if (res.resourceName == "ShadowDistanceField") {
      hasSdf = true;
    }
    if (res.resourceName == "ShadowMask") {
      hasShadowMask = true;
    }
    if (res.resourceName == "ShadowOccluderSSBO") {
      hasOccluder = true;
    }
    if (res.resourceName == "ClusterHeaderSSBO") {
      hasClusterHeader = true;
    }
    if (res.resourceName == "ClusterLightIndexSSBO") {
      hasClusterLightIndex = true;
    }
    if (res.resourceName == "ClusterPackedLightSSBO") {
      hasClusterPackedLight = true;
    }
    if (res.resourceName == "ClusterCounterSSBO") {
      hasClusterCounter = true;
    }
  }
  CHECK(hasShadowAtlas);
  CHECK(hasSdf);
  CHECK(hasShadowMask);
  CHECK(hasOccluder);
  CHECK(hasClusterHeader);
  CHECK(hasClusterLightIndex);
  CHECK(hasClusterPackedLight);
  CHECK(hasClusterCounter);

  // Expected producer->consumer edges.
  bool edgePrepareToBuild = false;
  bool edgeBuildToResolve = false;
  bool edgeCullingToHeader = false;
  bool edgeCullingToLightIndex = false;
  bool edgeCullingToPackedLight = false;
  bool edgeCullingToCounter = false;
  for (const auto &edge : plan.edges) {
    if (edge.producerPassName == "ShadowPreparePass" &&
        edge.consumerPassName == "ShadowBuildPass" &&
        edge.resourceName == "ShadowOccluderSSBO") {
      edgePrepareToBuild = true;
    }
    if (edge.producerPassName == "ShadowBuildPass" &&
        edge.consumerPassName == "ShadowResolvePass" &&
        edge.resourceName == "ShadowDistanceField") {
      edgeBuildToResolve = true;
    }
    if (edge.producerPassName == "LightCullingPass" &&
        edge.consumerPassName == "ClusterConsumerPass") {
      if (edge.resourceName == "ClusterHeaderSSBO") {
        edgeCullingToHeader = true;
      }
      if (edge.resourceName == "ClusterLightIndexSSBO") {
        edgeCullingToLightIndex = true;
      }
      if (edge.resourceName == "ClusterPackedLightSSBO") {
        edgeCullingToPackedLight = true;
      }
      if (edge.resourceName == "ClusterCounterSSBO") {
        edgeCullingToCounter = true;
      }
    }
  }
  CHECK(edgePrepareToBuild);
  CHECK(edgeBuildToResolve);
  CHECK(edgeCullingToHeader);
  CHECK(edgeCullingToLightIndex);
  CHECK(edgeCullingToPackedLight);
  CHECK(edgeCullingToCounter);

  // Expected cross-pass transitions replace the removed manual barriers.
  bool transitionOccluder = false;
  bool transitionSdf = false;
  bool transitionHeader = false;
  bool transitionLightIndex = false;
  bool transitionPackedLight = false;
  bool transitionCounter = false;
  for (const auto &transition : plan.transitions) {
    if (transition.resourceName == "ShadowOccluderSSBO") {
      CHECK_EQ(transition.previousStage, PipelineStage::Host);
      CHECK_EQ(transition.nextStage, PipelineStage::Compute);
      CHECK_NE(transition.barrierBits, 0u);
      transitionOccluder = true;
    }
    if (transition.resourceName == "ShadowDistanceField") {
      CHECK_EQ(transition.previousStage, PipelineStage::Compute);
      CHECK_EQ(transition.nextStage, PipelineStage::Fragment);
      CHECK_NE(transition.barrierBits, 0u);
      transitionSdf = true;
    }
    // Every cluster SSBO written by LightCullingPass must carry an explicit
    // Compute->Fragment GL_SHADER_STORAGE_BARRIER_BIT transition so the
    // fragment-stage consumer reads settled data.
    const bool clusterResource =
        transition.resourceName == "ClusterHeaderSSBO" ||
        transition.resourceName == "ClusterLightIndexSSBO" ||
        transition.resourceName == "ClusterPackedLightSSBO" ||
        transition.resourceName == "ClusterCounterSSBO";
    if (clusterResource) {
      CHECK_EQ(transition.previousStage, PipelineStage::Compute);
      CHECK_EQ(transition.nextStage, PipelineStage::Fragment);
      CHECK_EQ(transition.barrierBits,
               0x00002000u); // GL_SHADER_STORAGE_BARRIER_BIT
    }
    if (transition.resourceName == "ClusterHeaderSSBO") {
      transitionHeader = true;
    }
    if (transition.resourceName == "ClusterLightIndexSSBO") {
      transitionLightIndex = true;
    }
    if (transition.resourceName == "ClusterPackedLightSSBO") {
      transitionPackedLight = true;
    }
    if (transition.resourceName == "ClusterCounterSSBO") {
      transitionCounter = true;
    }
  }
  CHECK(transitionOccluder);
  CHECK(transitionSdf);
  CHECK(transitionHeader);
  CHECK(transitionLightIndex);
  CHECK(transitionPackedLight);
  CHECK(transitionCounter);

  // B7 contract: no missing producer, no multiple writers, no dependency
  // cycle must be reported for this shadow/cluster plan. This is an explicit
  // negative assertion over the compiled plan diagnostics (not just the
  // aggregate HasValidationErrors() check above).
  for (const auto &diag : plan.diagnostics) {
    CAPTURE(diag.passName);
    CAPTURE(diag.resourceName);
    CAPTURE(diag.message);
    CHECK(diag.message.find("read-before-write") == std::string::npos);
    CHECK(diag.message.find("multiple write owners") == std::string::npos);
    CHECK(diag.message.find("cycle detected") == std::string::npos);
  }
}

TEST_CASE("[Unit] RenderGraph - Phase B same-pass phase barrier + binding observations") {
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::render::passes;
  using namespace NoMoreDay::RenderConstants;

  RenderGraph graph;
  graph.AddPass(std::make_shared<ShadowPreparePass>());
  graph.AddPass(std::make_shared<ShadowBuildPass>());
  graph.AddPass(std::make_shared<ShadowResolvePass>());

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
  const auto &plan = graph.GetCompiledPlan();
  REQUIRE(plan.isValid);

  // The same-pass SDF compute -> hybrid tile fragment barrier must be declared
  // with the correct phase pair and the legacy Image|Buffer bits. It must NOT
  // be represented by a pass-entry local barrier (which fires before Execute).
  bool foundPhaseBarrier = false;
  for (const auto &barrier : plan.phaseBarriers) {
    if (barrier.passName == "ShadowBuildPass") {
      CHECK_EQ(barrier.sourcePhase, PipelineStage::Compute);
      CHECK_EQ(barrier.targetPhase, PipelineStage::Fragment);
      CHECK_EQ(barrier.barrierBits,
               static_cast<uint32_t>(Barrier::Image) |
                   static_cast<uint32_t>(Barrier::Buffer));
      foundPhaseBarrier = true;
    }
  }
  CHECK(foundPhaseBarrier);

  // Observer-only binding declarations must be visible in the compiled plan.
  bool foundOccluderBinding = false;
  bool foundSdfImage = false;
  for (const auto &binding : plan.bindings) {
    if (binding.passName == "ShadowBuildPass" &&
        binding.resourceName == "ShadowOccluderSSBO" &&
        binding.kind == ResourceBindingKind::BufferBase) {
      CHECK_EQ(binding.point, ShadowCS::kOccluderBinding);
      foundOccluderBinding = true;
    }
    if (binding.passName == "ShadowBuildPass" &&
        binding.resourceName == "ShadowDistanceField" &&
        binding.kind == ResourceBindingKind::ImageUnit) {
      CHECK_EQ(binding.point, ShadowCS::kSdfImageBinding);
      foundSdfImage = true;
    }
  }
  CHECK(foundOccluderBinding);
  CHECK(foundSdfImage);
}

TEST_CASE("[Unit] RenderGraph - Phase C GI chain same-pass phase barriers + cross-pass transitions") {
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::render::passes;
  using namespace NoMoreDay::RenderConstants;

  // Production-style GI chain (minus the shadow/VFX/UI wrapper passes): every
  // one of the six migrated passes must carry its same-pass phase barrier
  // declaration in the compiled plan, and the removed manual barriers at the
  // OccluderExtract->JFA and JFA->RadianceCascades boundaries must be
  // represented as graph transitions with the legacy Image|TexFetch bits.
  RenderGraph graph;
  graph.AddPass(std::make_shared<ScenePass>());
  graph.AddPass(std::make_shared<LightingPass>());
  graph.AddPass(std::make_shared<OccluderExtractPass>());
  graph.AddPass(std::make_shared<JFAPass>());
  graph.AddPass(std::make_shared<RadianceCascadesPass>());
  graph.AddPass(std::make_shared<GICompositePass>());
  graph.AddPass(std::make_shared<FluidSimulationPass>());

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
  const auto &plan = graph.GetCompiledPlan();
  REQUIRE(plan.isValid);

  constexpr uint32_t kTextureFetch = 0x00000008u;
  constexpr uint32_t kImageTexFetch =
      static_cast<uint32_t>(Barrier::Image) | kTextureFetch;
  constexpr uint32_t kBufferTexFetch =
      static_cast<uint32_t>(Barrier::Buffer) | kTextureFetch;
  constexpr uint32_t kImageBufferTexFetch =
      static_cast<uint32_t>(Barrier::Image) |
      static_cast<uint32_t>(Barrier::Buffer) | kTextureFetch;

  bool foundOccluderBarrier = false;
  bool foundRadianceBarrier = false;
  bool foundGiCompositeBarrier = false;
  bool foundFluidComputeBarrier = false;
  bool foundFluidVertexBarrier = false;
  bool foundJfaBarrier = false;
  for (const auto &barrier : plan.phaseBarriers) {
    if (barrier.passName == "OccluderExtractPass") {
      CHECK_EQ(barrier.sourcePhase, PipelineStage::Compute);
      CHECK_EQ(barrier.targetPhase, PipelineStage::Compute);
      CHECK_EQ(barrier.barrierBits, kImageTexFetch);
      foundOccluderBarrier = true;
    }
    if (barrier.passName == "RadianceCascadesPass") {
      CHECK_EQ(barrier.sourcePhase, PipelineStage::Compute);
      CHECK_EQ(barrier.targetPhase, PipelineStage::Compute);
      CHECK_EQ(barrier.barrierBits, kImageTexFetch);
      foundRadianceBarrier = true;
    }
    if (barrier.passName == "GICompositePass") {
      CHECK_EQ(barrier.sourcePhase, PipelineStage::Compute);
      CHECK_EQ(barrier.targetPhase, PipelineStage::Fragment);
      CHECK_EQ(barrier.barrierBits,
               static_cast<uint32_t>(Barrier::Image) |
                   static_cast<uint32_t>(Barrier::Buffer));
      foundGiCompositeBarrier = true;
    }
    if (barrier.passName == "FluidSimulationPass" &&
        barrier.sourcePhase == PipelineStage::Compute &&
        barrier.targetPhase == PipelineStage::Compute) {
      CHECK_EQ(barrier.barrierBits, static_cast<uint32_t>(Barrier::Buffer));
      foundFluidComputeBarrier = true;
    }
    if (barrier.passName == "FluidSimulationPass" &&
        barrier.sourcePhase == PipelineStage::Compute &&
        barrier.targetPhase == PipelineStage::Vertex) {
      CHECK_EQ(barrier.barrierBits, kBufferTexFetch);
      foundFluidVertexBarrier = true;
    }
    if (barrier.passName == "JFAPass") {
      CHECK_EQ(barrier.sourcePhase, PipelineStage::Compute);
      CHECK_EQ(barrier.targetPhase, PipelineStage::Compute);
      CHECK_EQ(barrier.barrierBits, kImageBufferTexFetch);
      foundJfaBarrier = true;
    }
  }
  CHECK(foundOccluderBarrier);
  CHECK(foundRadianceBarrier);
  CHECK(foundGiCompositeBarrier);
  CHECK(foundFluidComputeBarrier);
  CHECK(foundFluidVertexBarrier);
  CHECK(foundJfaBarrier);

  // The two deleted cross-pass sites must be covered by graph transitions
  // (Compute write -> Fragment read) carrying the legacy Image|TexFetch bits.
  bool transitionOccluderMask = false;
  bool transitionDistanceField = false;
  for (const auto &transition : plan.transitions) {
    if (transition.resourceName == "OccluderMask") {
      CHECK_EQ(transition.previousStage, PipelineStage::Compute);
      CHECK_EQ(transition.nextStage, PipelineStage::Fragment);
      CHECK_EQ(transition.previousMode, PassAccessMode::Write);
      CHECK_EQ(transition.nextMode, PassAccessMode::Read);
      CHECK_EQ(transition.barrierBits, kImageTexFetch);
      transitionOccluderMask = true;
    }
    if (transition.resourceName == "DistanceField") {
      CHECK_EQ(transition.previousStage, PipelineStage::Compute);
      CHECK_EQ(transition.nextStage, PipelineStage::Fragment);
      CHECK_EQ(transition.previousMode, PassAccessMode::Write);
      CHECK_EQ(transition.nextMode, PassAccessMode::Read);
      CHECK_EQ(transition.barrierBits, kImageTexFetch);
      transitionDistanceField = true;
    }
  }
  CHECK(transitionOccluderMask);
  CHECK(transitionDistanceField);
}

TEST_CASE("[Unit] RenderGraph - Phase C LightingPass consumes cluster SSBOs via graph transitions") {
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::render::passes;
  using namespace NoMoreDay::render::core;

  // LightingPass declares its typed cluster reads (gated on the
  // v3Enabled && clusteredLightingEnabled config) so the graph emits the
  // Compute->Fragment SSBO transitions at LightingPass's entry, replacing the
  // manual per-cluster-frame MemoryBarrier. The gate is driven by the
  // QualityTierManager singleton; set the fields the gate reads, then restore.
  auto &qm = QualityTierManager::Get();
  auto &cfg = const_cast<RenderConfig &>(qm.GetConfig());
  const bool originalV3 = cfg.v3Enabled;
  const bool originalClustered = cfg.clusteredLightingEnabled;
  const bool originalDynamic = cfg.dynamicLightingEnabled;
  cfg.v3Enabled = true;
  cfg.clusteredLightingEnabled = true;
  cfg.dynamicLightingEnabled = true;

  RenderGraph graph;
  graph.AddPass(std::make_shared<ScenePass>());
  graph.AddPass(std::make_shared<LightCullingPass>());
  graph.AddPass(std::make_shared<LightingPass>());

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
  const auto &plan = graph.GetCompiledPlan();
  REQUIRE(plan.isValid);

  bool transitionHeader = false;
  bool transitionLightIndex = false;
  bool transitionPackedLight = false;
  for (const auto &transition : plan.transitions) {
    const bool clusterResource =
        transition.resourceName == "ClusterHeaderSSBO" ||
        transition.resourceName == "ClusterLightIndexSSBO" ||
        transition.resourceName == "ClusterPackedLightSSBO";
    if (!clusterResource) {
      continue;
    }
    CHECK_EQ(transition.previousStage, PipelineStage::Compute);
    CHECK_EQ(transition.nextStage, PipelineStage::Fragment);
    CHECK_EQ(transition.previousMode, PassAccessMode::Write);
    CHECK_EQ(transition.nextMode, PassAccessMode::Read);
    // GL_SHADER_STORAGE_BARRIER_BIT == the removed manual barrier bits.
    CHECK_EQ(transition.barrierBits, 0x00002000u);
    const std::string &consumer = plan.passes[transition.consumerPassIndex].passName;
    CHECK(consumer == "LightingPass");
    if (transition.resourceName == "ClusterHeaderSSBO") {
      transitionHeader = true;
    }
    if (transition.resourceName == "ClusterLightIndexSSBO") {
      transitionLightIndex = true;
    }
    if (transition.resourceName == "ClusterPackedLightSSBO") {
      transitionPackedLight = true;
    }
  }
  CHECK(transitionHeader);
  CHECK(transitionLightIndex);
  CHECK(transitionPackedLight);

  // Restore the singleton config so later unit tests see the default baseline.
  cfg.v3Enabled = originalV3;
  cfg.clusteredLightingEnabled = originalClustered;
  cfg.dynamicLightingEnabled = originalDynamic;
}

TEST_CASE("[Unit] RenderGraph - duplicate phase barrier declaration fails closed") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ScenePass", [](RenderGraphBuilder &builder) {
        builder.AddPhaseBarrier(PipelineStage::Compute, PipelineStage::Fragment,
                                0x0020u);
        builder.AddPhaseBarrier(PipelineStage::Compute, PipelineStage::Fragment,
                                0x0200u);
      }));

#if defined(NDEBUG)
  CHECK_NOTHROW(graph.Build());
#else
  CHECK_THROWS_AS(graph.Build(), std::logic_error);
#endif
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(), "ScenePass",
                           "(phase-barrier)", "duplicate phase barrier"));
}

TEST_CASE("[Unit] RenderGraph - binding for unknown tag fails closed") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ScenePass", [](RenderGraphBuilder &builder) {
        builder.BindBufferBase(RenderResourceTag::Custom, 1u);
      }));

#if defined(NDEBUG)
  CHECK_NOTHROW(graph.Build());
#else
  CHECK_THROWS_AS(graph.Build(), std::logic_error);
#endif
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(), "ScenePass",
                           "(binding)", "unknown/custom resource tag"));
}

TEST_CASE("[Unit] RenderGraph - binding without descriptor warns (observer-only)") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ScenePass", [](RenderGraphBuilder &builder) {
        builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
        builder.BindBufferBase(RenderResourceTag::ShadowOccluderSSBO, 15u);
      }));

  CHECK_NOTHROW(graph.Build());
  // Observer-only contract: no GL ownership change, so this is a warning, not a
  // validation error.
  CHECK(!graph.HasValidationErrors());
  bool foundWarning = false;
  for (const auto &diagnostic : graph.GetValidationDiagnostics()) {
    if (diagnostic.severity ==
            RenderGraph::ValidationDiagnostic::Severity::Warning &&
        diagnostic.passName == "ScenePass" &&
        diagnostic.resourceName == "ShadowOccluderSSBO" &&
        diagnostic.message.find("no descriptor declared") != std::string::npos) {
      foundWarning = true;
    }
  }
  CHECK(foundWarning);
}

TEST_CASE("[Unit] RenderGraph - duplicate backing snapshots fail closed") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ShadowBuildPass", [](RenderGraphBuilder &builder) {
        TypedResourceDescriptor desc;
        desc.name = "ShadowOccluderSSBO";
        desc.tag = RenderResourceTag::ShadowOccluderSSBO;
        desc.ownerTag = RenderOwnerTag::Shadow;
        desc.kind = ResourceKind::StorageBuffer;
        desc.lifetime = ResourceLifetime::Persistent;
        builder.DeclareResource(desc);
        builder.BindBufferBase(RenderResourceTag::ShadowOccluderSSBO, 15u);

        ResourceImportInfo import;
        import.resourceTag = RenderResourceTag::ShadowOccluderSSBO;
        import.kind = ResourceKind::StorageBuffer;
        import.backingOwner = RenderOwnerTag::Shadow;
        import.bindingPoint = 15u;
        builder.ImportResource(import);
      }));
  graph.Build();

  RenderContext context;
  context.importedBackings = {
      {RenderResourceTag::ShadowOccluderSSBO, 11u, 0u, 0u},
      {RenderResourceTag::ShadowOccluderSSBO, 12u, 0u, 0u},
  };

  const auto result = graph.ResolvePassBindings(0u, context);
  CHECK_FALSE(result.allAdmitted);
  CHECK(result.operations.empty());
  CHECK(HasErrorContaining(result.diagnostics, "ShadowBuildPass",
                           "ShadowOccluderSSBO", "multiple imported backing"));
}

TEST_CASE("[Unit] RenderGraph - S0 timer ring frame index overflow guard") {
  using namespace NoMoreDay::render::debug;

  auto &ring = GPUTimerQueryRing::Get();
  ring.Initialize();
  ring.DebugSetFrameIndex(std::numeric_limits<uint64_t>::max());
  ring.BeginFrame();
  CHECK_EQ(ring.DebugGetFrameIndex(), std::numeric_limits<uint64_t>::max());
  ring.DebugSetFrameIndex(0);
  ring.BeginFrame();
  CHECK_EQ(ring.DebugGetFrameIndex(), 1u);
  ring.Shutdown();
}

TEST_CASE("[Unit] RenderGraph - S0 stable pass id timer plumbing round trip") {
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::render::debug;

  const uint32_t stablePassId = StablePassId(CanonicalizePassName("ScenePass"));
  CHECK_NE(stablePassId, 0u);
  CHECK_NE(stablePassId, kFrameLevelStablePassId);
  CHECK_EQ(CanonicalizePassName("  Scene Pass "), "scenepass");

  auto &ring = GPUTimerQueryRing::Get();
  ring.Initialize();
  ring.BeginFrame();
  ring.BeginPass(stablePassId);
  ring.EndPass(stablePassId);
  ring.EndFrame();
  ring.PollReadyQueries();
  GPUTimerResult res = ring.GetPassResult(stablePassId);
  CHECK((res.state == QueryState::Valid || res.state == QueryState::CpuFallback ||
         res.state == QueryState::Pending));
  ring.Shutdown();
}

TEST_CASE("[Unit] RenderGraph - Phase B external backing import contract lands in compiled plan") {
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::render::passes;
  using namespace NoMoreDay::RenderConstants;

  RenderGraph graph;
  graph.AddPass(std::make_shared<ShadowPreparePass>());
  graph.AddPass(std::make_shared<ShadowBuildPass>());
  graph.AddPass(std::make_shared<ShadowResolvePass>());
  graph.AddPass(std::make_shared<LightCullingPass>());

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
  const auto &plan = graph.GetCompiledPlan();
  REQUIRE(plan.isValid);

  // The import declarations are observer-only metadata: they must name the
  // external backing owner, its resize lifecycle, and the manual binding
  // surface used by each pass, without any GL ownership change.
  bool sdfImported = false;
  bool atlasImported = false;
  bool occluderImported = false;
  bool maskImported = false;
  bool clusterHeaderImported = false;
  bool lightBufferImported = false;
  for (const auto &import : plan.imports) {
    if (import.passName == "ShadowBuildPass" &&
        import.resourceName == "ShadowDistanceField") {
      CHECK_EQ(import.kind, ResourceKind::Texture2D);
      CHECK_EQ(import.format, ResourceFormat::RG16F);
      CHECK_EQ(import.backingOwner, RenderOwnerTag::Shadow);
      CHECK(import.resizeFollowsScreen);
      CHECK_FALSE(import.resizeFollowsCapacity);
      CHECK_EQ(import.imageUnit, ShadowCS::kSdfImageBinding);
      sdfImported = true;
    }
    if (import.passName == "ShadowBuildPass" &&
        import.resourceName == "ShadowAtlas") {
      CHECK_EQ(import.kind, ResourceKind::Texture2D);
      CHECK_EQ(import.format, ResourceFormat::RGBA16F);
      CHECK_EQ(import.backingOwner, RenderOwnerTag::Shadow);
      CHECK(import.resizeFollowsCapacity);
      CHECK_EQ(import.colorAttachmentIndex, 0u);
      atlasImported = true;
    }
    if (import.passName == "ShadowBuildPass" &&
        import.resourceName == "ShadowOccluderSSBO") {
      CHECK_EQ(import.kind, ResourceKind::StorageBuffer);
      CHECK_EQ(import.backingOwner, RenderOwnerTag::Shadow);
      CHECK(import.resizeFollowsCapacity);
      CHECK_EQ(import.bindingPoint, ShadowCS::kOccluderBinding);
      occluderImported = true;
    }
    if (import.passName == "ShadowResolvePass" &&
        import.resourceName == "ShadowMask") {
      CHECK_EQ(import.kind, ResourceKind::Texture2D);
      CHECK_EQ(import.format, ResourceFormat::RGBA16F);
      CHECK_EQ(import.backingOwner, RenderOwnerTag::Shadow);
      CHECK(import.resizeFollowsScreen);
      CHECK_EQ(import.colorAttachmentIndex, 0u);
      maskImported = true;
    }
    if (import.passName == "LightCullingPass" &&
        import.resourceName == "ClusterHeaderSSBO") {
      CHECK_EQ(import.kind, ResourceKind::StorageBuffer);
      CHECK_EQ(import.backingOwner, RenderOwnerTag::LightCulling);
      CHECK(import.resizeFollowsCapacity);
      CHECK_EQ(import.bindingPoint, 1u); // CLUSTER_HEADER_OUT
      clusterHeaderImported = true;
    }
    if (import.passName == "LightCullingPass" &&
        import.resourceName == "LightBufferSSBO") {
      CHECK_EQ(import.kind, ResourceKind::StorageBuffer);
      CHECK_EQ(import.backingOwner, RenderOwnerTag::Lighting); // LightManager
      CHECK(import.resizeFollowsCapacity);
      CHECK_EQ(import.bindingPoint, 0u); // LIGHT_LIST_IN
      lightBufferImported = true;
    }
  }
  CHECK(sdfImported);
  CHECK(atlasImported);
  CHECK(occluderImported);
  CHECK(maskImported);
  CHECK(clusterHeaderImported);
  CHECK(lightBufferImported);
}

TEST_CASE("[Unit] RenderGraph - Phase B6 VFXEmissionSnapshotPass declares particle emissive backing") {
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::render::passes;

  RenderGraph graph;
  graph.AddPass(std::make_shared<VFXEmissionSnapshotPass>());

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
  const auto &plan = graph.GetCompiledPlan();
  REQUIRE(plan.isValid);

  REQUIRE(plan.passOrder.size() == 1u);
  CHECK_EQ(plan.passOrder[0], "VFXEmissionSnapshotPass");

  // The pass writes the particle emissive texture (RadianceCascades-owned
  // FBO, RGBA16F at screen size); the write contract must carry the correct
  // tag, owner, stage, and usage.
  bool resourceFound = false;
  for (const auto &resource : plan.resources) {
    if (resource.resourceName != "ParticleEmissive") {
      continue;
    }
    CHECK_EQ(resource.tag, RenderResourceTag::ParticleEmissive);
    CHECK(resource.hasProducer);
    CHECK_EQ(resource.firstProducerPassIndex, 0u);
    REQUIRE(resource.writerPassIndices.size() == 1u);
    CHECK_EQ(resource.writerPassIndices[0], 0u);
    CHECK_EQ(resource.descriptor.kind, ResourceKind::Texture2D);
    CHECK_EQ(resource.descriptor.format, ResourceFormat::RGBA16F);
    CHECK_EQ(resource.descriptor.lifetime, ResourceLifetime::Persistent);
    CHECK_EQ(resource.descriptor.ownerTag, RenderOwnerTag::RadianceCascades);
    resourceFound = true;
  }
  CHECK(resourceFound);

  // The import contract names the external backing owner and its
  // screen-following resize lifecycle. Observer-only: the graph must never
  // allocate, resize, free, or GL-bind this backing.
  bool importFound = false;
  for (const auto &import : plan.imports) {
    if (import.passName != "VFXEmissionSnapshotPass" ||
        import.resourceName != "ParticleEmissive") {
      continue;
    }
    CHECK_EQ(import.resourceTag, RenderResourceTag::ParticleEmissive);
    CHECK_EQ(import.kind, ResourceKind::Texture2D);
    CHECK_EQ(import.format, ResourceFormat::RGBA16F);
    CHECK_EQ(import.backingOwner, RenderOwnerTag::RadianceCascades);
    CHECK(import.resizeFollowsScreen);
    CHECK_FALSE(import.resizeFollowsCapacity);
    CHECK_EQ(import.colorAttachmentIndex, 0u);
    importFound = true;
  }
  CHECK(importFound);
}

TEST_CASE("[Unit] RenderGraph - import on transient descriptor fails closed") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ShadowBuildPass", [](RenderGraphBuilder &builder) {
        TypedResourceDescriptor desc;
        desc.name = "ShadowAtlas";
        desc.tag = RenderResourceTag::ShadowAtlas;
        desc.ownerTag = RenderOwnerTag::Shadow;
        desc.kind = ResourceKind::Texture2D;
        desc.format = ResourceFormat::RGBA16F;
        desc.lifetime = ResourceLifetime::Transient; // graph would own this
        builder.DeclareResource(desc);
        builder.Write(RenderResourceTag::ShadowAtlas, RenderOwnerTag::Shadow,
                      PipelineStage::Fragment, ResourceUsage::ColorAttachment);

        ResourceImportInfo import;
        import.resourceTag = RenderResourceTag::ShadowAtlas;
        import.kind = ResourceKind::Texture2D;
        import.format = ResourceFormat::RGBA16F;
        import.backingOwner = RenderOwnerTag::Shadow;
        builder.ImportResource(import);
      }));

#if defined(NDEBUG)
  CHECK_NOTHROW(graph.Build());
#else
  CHECK_THROWS_AS(graph.Build(), std::logic_error);
#endif
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(), "ShadowBuildPass",
                           "ShadowAtlas", "must not be declared Transient"));
}

TEST_CASE("[Unit] RenderGraph - import kind mismatch with descriptor fails closed") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ShadowResolvePass", [](RenderGraphBuilder &builder) {
        TypedResourceDescriptor desc;
        desc.name = "ShadowMask";
        desc.tag = RenderResourceTag::ShadowMask;
        desc.ownerTag = RenderOwnerTag::Shadow;
        desc.kind = ResourceKind::Texture2D;
        desc.format = ResourceFormat::RGBA16F;
        desc.lifetime = ResourceLifetime::Persistent;
        builder.DeclareResource(desc);
        builder.Write(RenderResourceTag::ShadowMask, RenderOwnerTag::Shadow,
                      PipelineStage::FramebufferAttachment,
                      ResourceUsage::ColorAttachment);

        ResourceImportInfo import;
        import.resourceTag = RenderResourceTag::ShadowMask;
        import.kind = ResourceKind::StorageBuffer; // contradicts the descriptor
        import.backingOwner = RenderOwnerTag::Shadow;
        builder.ImportResource(import);
      }));

#if defined(NDEBUG)
  CHECK_NOTHROW(graph.Build());
#else
  CHECK_THROWS_AS(graph.Build(), std::logic_error);
#endif
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(), "ShadowResolvePass",
                           "ShadowMask", "import kind does not match"));
}

TEST_CASE("[Unit] RenderGraph - conflicting import across passes fails closed") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ShadowBuildPass", [](RenderGraphBuilder &builder) {
        ResourceImportInfo import;
        import.resourceTag = RenderResourceTag::ShadowAtlas;
        import.kind = ResourceKind::Texture2D;
        import.format = ResourceFormat::RGBA16F;
        import.backingOwner = RenderOwnerTag::Shadow;
        builder.ImportResource(import);
      }));
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ShadowResolvePass", [](RenderGraphBuilder &builder) {
        ResourceImportInfo import;
        import.resourceTag = RenderResourceTag::ShadowAtlas;
        import.kind = ResourceKind::StorageBuffer; // contradicts the first pass
        import.backingOwner = RenderOwnerTag::Shadow;
        builder.ImportResource(import);
      }));

#if defined(NDEBUG)
  CHECK_NOTHROW(graph.Build());
#else
  CHECK_THROWS_AS(graph.Build(), std::logic_error);
#endif
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(), "ShadowResolvePass",
                           "ShadowAtlas", "import conflicts with a declaration"));
}

TEST_CASE("[Unit] RenderGraph - import binding surface mismatch with observed manual bind warns") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ShadowBuildPass", [](RenderGraphBuilder &builder) {
        TypedResourceDescriptor occluderDesc;
        occluderDesc.name = "ShadowOccluderSSBO";
        occluderDesc.tag = RenderResourceTag::ShadowOccluderSSBO;
        occluderDesc.ownerTag = RenderOwnerTag::Shadow;
        occluderDesc.kind = ResourceKind::StorageBuffer;
        occluderDesc.lifetime = ResourceLifetime::Persistent;
        builder.DeclareResource(occluderDesc);

        // Manual bind stays authoritative; a stale import binding point is a
        // warning, never an error, and never a GL ownership change.
        builder.BindBufferBase(RenderResourceTag::ShadowOccluderSSBO, 15u);
        ResourceImportInfo import;
        import.resourceTag = RenderResourceTag::ShadowOccluderSSBO;
        import.kind = ResourceKind::StorageBuffer;
        import.backingOwner = RenderOwnerTag::Shadow;
        import.bindingPoint = 7u; // stale: manual bind uses 15
        builder.ImportResource(import);
      }));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
  bool foundWarning = false;
  for (const auto &diagnostic : graph.GetValidationDiagnostics()) {
    if (diagnostic.severity ==
            RenderGraph::ValidationDiagnostic::Severity::Warning &&
        diagnostic.passName == "ShadowBuildPass" &&
        diagnostic.resourceName == "ShadowOccluderSSBO" &&
        diagnostic.message.find("does not match the BindBufferBase observation") !=
            std::string::npos) {
      foundWarning = true;
    }
  }
  CHECK(foundWarning);
}

TEST_CASE("[Unit] RenderGraph - import without backing owner warns") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ScenePass", [](RenderGraphBuilder &builder) {
        builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);

        ResourceImportInfo import;
        import.resourceTag = RenderResourceTag::ShadowAtlas;
        import.kind = ResourceKind::Texture2D;
        import.format = ResourceFormat::RGBA16F;
        // backingOwner left Unknown on purpose: unresolved ownership is a
        // warning so the pass still builds and the observer contract survives.
        builder.ImportResource(import);
      }));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
  bool foundWarning = false;
  for (const auto &diagnostic : graph.GetValidationDiagnostics()) {
    if (diagnostic.severity ==
            RenderGraph::ValidationDiagnostic::Severity::Warning &&
        diagnostic.passName == "ScenePass" &&
        diagnostic.resourceName == "ShadowAtlas" &&
        diagnostic.message.find("does not name the external backing owner") !=
            std::string::npos) {
      foundWarning = true;
    }
  }
  CHECK(foundWarning);
}

TEST_CASE("[Unit] RenderGraph - B12 binding resolve admits BufferBase/ImageUnit ops from snapshot") {
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::render::passes;
  using namespace NoMoreDay::RenderConstants;

  RenderGraph graph;
  graph.AddPass(std::make_shared<ShadowPreparePass>());
  graph.AddPass(std::make_shared<ShadowBuildPass>());
  graph.AddPass(std::make_shared<ShadowResolvePass>());
  graph.AddPass(std::make_shared<LightCullingPass>());

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());

  // Per-frame imported backing snapshot as injected by RenderSystem; the graph
  // only copies handles out of it and never owns any GL resource.
  RenderContext context = {};
  context.importedBackings.push_back(
      {RenderResourceTag::ShadowOccluderSSBO, 100u, 0u, 0u});
  context.importedBackings.push_back(
      {RenderResourceTag::ShadowDistanceField, 0u, 200u, 0u});

  const auto result = graph.ResolvePassBindings(1u /* ShadowBuildPass */, context);
  CHECK(result.allAdmitted);
  CHECK(result.diagnostics.empty());
  REQUIRE(result.operations.size() == 2u);

  const auto &bufferOp = result.operations[0];
  CHECK_EQ(bufferOp.kind,
           RenderGraph::ResolvedBindingOperation::Kind::BindBufferBase);
  CHECK_EQ(bufferOp.resourceTag, RenderResourceTag::ShadowOccluderSSBO);
  CHECK_EQ(bufferOp.point, ShadowCS::kOccluderBinding);
  CHECK_EQ(bufferOp.handle, 100u);

  const auto &imageOp = result.operations[1];
  CHECK_EQ(imageOp.kind,
           RenderGraph::ResolvedBindingOperation::Kind::BindImageTexture);
  CHECK_EQ(imageOp.resourceTag, RenderResourceTag::ShadowDistanceField);
  CHECK_EQ(imageOp.point, ShadowCS::kSdfImageBinding);
  CHECK_EQ(imageOp.handle, 200u);
  CHECK_EQ(imageOp.access, 0x88B9u); // GL_WRITE_ONLY
  CHECK_EQ(imageOp.format, 0x822Fu); // GL_RG16F
}

TEST_CASE("[Unit] RenderGraph - B12 missing snapshot denies that binding fail-closed") {
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::render::passes;

  RenderGraph graph;
  graph.AddPass(std::make_shared<ShadowPreparePass>());
  graph.AddPass(std::make_shared<ShadowBuildPass>());
  graph.AddPass(std::make_shared<ShadowResolvePass>());
  graph.AddPass(std::make_shared<LightCullingPass>());
  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());

  RenderContext context = {};
  context.importedBackings.push_back(
      {RenderResourceTag::ShadowOccluderSSBO, 100u, 0u, 0u});
  // No snapshot for ShadowDistanceField on purpose.

  const auto result = graph.ResolvePassBindings(1u, context);
  CHECK_FALSE(result.allAdmitted);
  REQUIRE(result.operations.size() == 1u);
  CHECK_EQ(result.operations[0].kind,
           RenderGraph::ResolvedBindingOperation::Kind::BindBufferBase);
  CHECK_EQ(result.operations[0].resourceTag, RenderResourceTag::ShadowOccluderSSBO);
  CHECK_EQ(result.operations[0].handle, 100u);

  bool sdfDenied = false;
  for (const auto &diagnostic : result.diagnostics) {
    if (diagnostic.severity == RenderGraph::ValidationDiagnostic::Severity::Error &&
        diagnostic.passName == "ShadowBuildPass" &&
        diagnostic.resourceName == "ShadowDistanceField" &&
        diagnostic.message.find("no imported backing snapshot") !=
            std::string::npos) {
      sdfDenied = true;
    }
  }
  CHECK(sdfDenied);
}

TEST_CASE("[Unit] RenderGraph - B12 import kind incompatible with binding kind is denied") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ShadowBuildPass", [](RenderGraphBuilder &builder) {
        TypedResourceDescriptor desc;
        desc.name = "ShadowOccluderSSBO";
        desc.tag = RenderResourceTag::ShadowOccluderSSBO;
        desc.ownerTag = RenderOwnerTag::Shadow;
        desc.kind = ResourceKind::Texture2D; // import kind matches descriptor
        desc.format = ResourceFormat::RG16F;
        desc.lifetime = ResourceLifetime::Persistent;
        builder.DeclareResource(desc);
        builder.Write(RenderResourceTag::ShadowOccluderSSBO, RenderOwnerTag::Shadow,
                      PipelineStage::Compute, ResourceUsage::StorageWrite);
        // BufferBase binding on a Texture2D-backed import: kind clash.
        builder.BindBufferBase(RenderResourceTag::ShadowOccluderSSBO, 15u);

        ResourceImportInfo import;
        import.resourceTag = RenderResourceTag::ShadowOccluderSSBO;
        import.kind = ResourceKind::Texture2D;
        import.format = ResourceFormat::RG16F;
        import.backingOwner = RenderOwnerTag::Shadow;
        builder.ImportResource(import);
      }));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());

  RenderContext context = {};
  context.importedBackings.push_back(
      {RenderResourceTag::ShadowOccluderSSBO, 100u, 0u, 0u});

  const auto result = graph.ResolvePassBindings(0u, context);
  CHECK_FALSE(result.allAdmitted);
  CHECK(result.operations.empty());
  bool kindDenied = false;
  for (const auto &diagnostic : result.diagnostics) {
    if (diagnostic.severity == RenderGraph::ValidationDiagnostic::Severity::Error &&
        diagnostic.passName == "ShadowBuildPass" &&
        diagnostic.resourceName == "ShadowOccluderSSBO" &&
        diagnostic.message.find("import kind is incompatible") !=
            std::string::npos) {
      kindDenied = true;
    }
  }
  CHECK(kindDenied);
}

TEST_CASE("[Unit] RenderGraph - B12 zero imported backing handle denies fail-closed") {
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::render::passes;

  RenderGraph graph;
  graph.AddPass(std::make_shared<ShadowPreparePass>());
  graph.AddPass(std::make_shared<ShadowBuildPass>());
  graph.AddPass(std::make_shared<ShadowResolvePass>());
  graph.AddPass(std::make_shared<LightCullingPass>());
  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());

  RenderContext context = {};
  context.importedBackings.push_back(
      {RenderResourceTag::ShadowOccluderSSBO, 0u, 0u, 0u});
  context.importedBackings.push_back(
      {RenderResourceTag::ShadowDistanceField, 0u, 0u, 0u});

  const auto result = graph.ResolvePassBindings(1u, context);
  CHECK_FALSE(result.allAdmitted);
  CHECK(result.operations.empty()); // zero handles must never become GL binds
  size_t zeroHandleDenials = 0;
  for (const auto &diagnostic : result.diagnostics) {
    if (diagnostic.severity == RenderGraph::ValidationDiagnostic::Severity::Error &&
        diagnostic.passName == "ShadowBuildPass" &&
        diagnostic.message.find("zero/invalid handle") != std::string::npos) {
      ++zeroHandleDenials;
    }
  }
  CHECK_EQ(zeroHandleDenials, 2u);
}

TEST_CASE("[Unit] RenderGraph - B12 unsupported binding kind is diagnostic-only and fail-closed") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ScenePass", [](RenderGraphBuilder &builder) {
        builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
        // TextureUnit is outside the B12 execution scope (BufferBase/ImageUnit).
        builder.BindTextureUnit(RenderResourceTag::SceneHdrColor, 0u);
      }));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());

  RenderContext context = {};
  context.importedBackings.push_back(
      {RenderResourceTag::SceneHdrColor, 100u, 0u, 0u});

  const auto result = graph.ResolvePassBindings(0u, context);
  CHECK_FALSE(result.allAdmitted);
  CHECK(result.operations.empty()); // never faked as a GL bind
  bool unsupportedReported = false;
  for (const auto &diagnostic : result.diagnostics) {
    if (diagnostic.passName == "ScenePass" &&
        diagnostic.resourceName == "SceneColor" &&
        diagnostic.message.find("unsupported by graph-driven binding") !=
            std::string::npos) {
      unsupportedReported = true;
    }
  }
  CHECK(unsupportedReported);
}

TEST_CASE("[Unit] RenderGraph - B12 active-pass resolution outside Execute fails closed") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ScenePass", [](RenderGraphBuilder &builder) {
        builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
      }));
  CHECK_NOTHROW(graph.Build());

  RenderContext context = {};
  const auto result = graph.ResolveActivePassBindings(context);
  CHECK_FALSE(result.allAdmitted);
  REQUIRE(result.diagnostics.size() == 1u);
  CHECK_EQ(result.diagnostics[0].severity,
           RenderGraph::ValidationDiagnostic::Severity::Error);
  CHECK(result.diagnostics[0].message.find("no active pass") !=
        std::string::npos);
}

TEST_CASE("[Unit] RenderGraph - Phase D FindLastWriterOwner mirrors production owners") {
  using namespace NoMoreDay::render::graph;

  // Production-style multi-writer SceneHdrColor chain ending in UIWorld, plus
  // the LDR chain written by PostProcess then Distortion (distortion last).
  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ScenePass", [](RenderGraphBuilder &builder) {
        builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene,
                      PipelineStage::FramebufferAttachment,
                      ResourceUsage::ColorAttachment);
      }));
  graph.AddPass(std::make_shared<TestRenderPass>(
      "VFXPass", [](RenderGraphBuilder &builder) {
        builder.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::VFX,
                     PipelineStage::Fragment, ResourceUsage::ShaderRead);
        builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::VFX,
                      PipelineStage::FramebufferAttachment,
                      ResourceUsage::ColorAttachment);
      }));
  graph.AddPass(std::make_shared<TestRenderPass>(
      "UIWorldPass", [](RenderGraphBuilder &builder) {
        builder.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::UIWorld,
                     PipelineStage::Fragment, ResourceUsage::ShaderRead);
        builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::UIWorld,
                      PipelineStage::FramebufferAttachment,
                      ResourceUsage::ColorAttachment);
      }));
  graph.AddPass(std::make_shared<TestRenderPass>(
      "PostProcessPass", [](RenderGraphBuilder &builder) {
        builder.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::PostProcess,
                     PipelineStage::Fragment, ResourceUsage::ShaderRead);
        builder.Write(RenderResourceTag::PostProcessLdrColor,
                      RenderOwnerTag::PostProcess, PipelineStage::FramebufferAttachment,
                      ResourceUsage::ColorAttachment);
      }));
  graph.AddPass(std::make_shared<TestRenderPass>(
      "DistortionPass", [](RenderGraphBuilder &builder) {
        builder.Read(RenderResourceTag::PostProcessLdrColor,
                     RenderOwnerTag::Distortion, PipelineStage::Fragment,
                     ResourceUsage::ShaderRead);
        builder.Write(RenderResourceTag::DistortionLdrColor,
                      RenderOwnerTag::Distortion, PipelineStage::FramebufferAttachment,
                      ResourceUsage::ColorAttachment);
      }));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());

  // The last typed writer decides the owner (UIWorld after VFX after Scene).
  CHECK_EQ(graph.FindLastWriterOwner(RenderResourceTag::SceneHdrColor),
           RenderOwnerTag::UIWorld);
  // Distortion is the last writer of the LDR chain -> composite picks it.
  CHECK_EQ(graph.FindLastWriterOwner(RenderResourceTag::DistortionLdrColor),
           RenderOwnerTag::Distortion);
  // When only PostProcess writes the LDR color, it is the owner.
  CHECK_EQ(graph.FindLastWriterOwner(RenderResourceTag::PostProcessLdrColor),
           RenderOwnerTag::PostProcess);
}

TEST_CASE("[Unit] RenderGraph - Phase D FindLastWriterOwner falls back to Unknown") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ScenePass", [](RenderGraphBuilder &builder) {
        builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene,
                      PipelineStage::FramebufferAttachment,
                      ResourceUsage::ColorAttachment);
      }));

  // Unknown when the tag has no writer at all.
  CHECK_EQ(graph.FindLastWriterOwner(RenderResourceTag::SceneDepth),
           RenderOwnerTag::Unknown);
  CHECK_EQ(graph.FindLastWriterOwner(RenderResourceTag::DistortionLdrColor),
           RenderOwnerTag::Unknown);
  // Unknown for tags outside the typed enum.
  CHECK_EQ(graph.FindLastWriterOwner(RenderResourceTag::Custom),
           RenderOwnerTag::Unknown);

  // Accesses are collected during Build; before Build the graph must not
  // invent writers from an empty access set.
  CHECK_EQ(graph.FindLastWriterOwner(RenderResourceTag::SceneHdrColor),
           RenderOwnerTag::Unknown);

  CHECK_NOTHROW(graph.Build());
  CHECK_EQ(graph.FindLastWriterOwner(RenderResourceTag::SceneHdrColor),
           RenderOwnerTag::Scene);
}

TEST_CASE("[Unit] RenderGraph - Phase D OnResize fans out exactly once per node") {
  using namespace NoMoreDay::render::graph;

  int sceneResizes = 0;
  int compositeResizes = 0;
  RenderGraph graph;
  class ResizeCountingPass : public TestRenderPass {
  public:
    ResizeCountingPass(const char *name, int &count)
        : TestRenderPass(name, nullptr), m_count(count) {}
    void OnResize(int, int) override { ++m_count; }

  private:
    int &m_count;
  };

  graph.AddPass(std::make_shared<ResizeCountingPass>("ScenePass", sceneResizes));
  graph.AddPass(
      std::make_shared<ResizeCountingPass>("CompositePass", compositeResizes));

  graph.OnResize(640, 360);
  CHECK_EQ(sceneResizes, 1);
  CHECK_EQ(compositeResizes, 1);

  // A second OnResize drives the chain again (idempotent dispatch per call).
  graph.OnResize(1280, 720);
  CHECK_EQ(sceneResizes, 2);
  CHECK_EQ(compositeResizes, 2);
}

TEST_CASE("[Unit] RenderGraph - stable pass ids are frozen for the canonical name table") {
  using namespace NoMoreDay::render::graph;

  // Expected FNV-1a values (salt "NMD-STABLEPASS-V1" + lowercase canonical
  // name, truncated to uint32). Frozen so an accidental table rename or hash
  // change fails loudly instead of silently re-keying profiler/gate data.
  constexpr std::array<uint32_t, 20> kExpectedStableIds = {
      0x1FF39E00u, // ScenePass
      0xD6250DDAu, // LightingPass
      0x876148CDu, // HeightShadowPass
      0x4F859E20u, // OccluderExtractPass
      0x787C6EBFu, // JFAPass
      0x59E8A348u, // RadianceCascadesPass
      0x3C972F67u, // GICompositePass
      0x7C95D98Fu, // FluidSimulationPass
      0x37203200u, // VolumetricLightPass
      0xB6B3BAE0u, // VFXPass
      0xBDDCFD09u, // GPUTextPass
      0xD8604E24u, // GPULootPass
      0x56CD8176u, // UIWorldPass
      0x2DED7E3Fu, // PostProcessPass
      0xE2817481u, // DistortionPass
      0x8295B0D3u, // CompositePass
      0xF8B11E72u, // LightCullingPass
      0x2F7C6673u, // ShadowPreparePass
      0xCEE42870u, // ShadowBuildPass
      0x5F9C03D4u, // ShadowResolvePass
  };

  CHECK_EQ(kRenderPassNames.size(), kExpectedStableIds.size());
  for (size_t i = 0; i < kRenderPassNames.size(); ++i) {
    const uint32_t stableId =
        StablePassId(CanonicalizePassName(kRenderPassNames[i].full));
    INFO("pass name: ", kRenderPassNames[i].full);
    CHECK_EQ(stableId, kExpectedStableIds[i]);
  }

  // All 20 identities must be distinct.
  std::array<uint32_t, 20> seen = kExpectedStableIds;
  std::sort(seen.begin(), seen.end());
  CHECK_EQ(std::adjacent_find(seen.begin(), seen.end()), seen.end());
}




