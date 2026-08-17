#include "doctest.h"

#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/graph/RenderResourceDescriptor.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

using namespace NoMoreDay;
using NoMoreDay::render::graph::RenderContext;
using NoMoreDay::render::graph::RenderGraph;
using NoMoreDay::render::graph::RenderGraphBuilder;
using NoMoreDay::render::graph::RenderPass;
using NoMoreDay::render::graph::RenderPassType;
using NoMoreDay::render::graph::RenderResourceTag;
using NoMoreDay::render::graph::RenderOwnerTag;
using NoMoreDay::render::graph::RGTextureHandle;
using NoMoreDay::render::graph::RGBufferHandle;
using NoMoreDay::render::graph::ResourceKind;
using NoMoreDay::render::graph::ResourceFormat;
using NoMoreDay::render::graph::ExtentPolicy;
using NoMoreDay::render::graph::ExtentMode;
using NoMoreDay::render::graph::TypedResourceDescriptor;
using NoMoreDay::render::graph::kRenderPassNames;

class AD6MockPass : public RenderPass {
public:
  using SetupFn = std::function<void(RenderGraphBuilder &)>;
  using ExecFn = std::function<void(RenderContext &)>;

  AD6MockPass(std::string name, SetupFn setup, ExecFn exec = nullptr, bool sideEffects = false)
      : m_name(std::move(name)), m_setup(std::move(setup)), m_exec(std::move(exec)), m_hasSideEffects(sideEffects) {}

  void Setup(RenderGraphBuilder &builder) override {
    if (m_setup) {
      m_setup(builder);
    }
  }

  void Execute(RenderContext &ctx) override {
    m_executed = true;
    if (m_exec) {
      m_exec(ctx);
    }
  }

  const char *GetName() const override { return m_name.c_str(); }

  RenderPassType Type() const override {
    for (size_t i = 0; i < kRenderPassNames.size(); ++i) {
      if (m_name == kRenderPassNames[i].full || m_name == kRenderPassNames[i].display) {
        return static_cast<RenderPassType>(i);
      }
    }
    return RenderPassType::Scene;
  }

  bool HasSideEffects() const override { return m_hasSideEffects; }

  bool WasExecuted() const { return m_executed; }
  void ResetExecuted() { m_executed = false; }

private:
  std::string m_name;
  SetupFn m_setup;
  ExecFn m_exec;
  bool m_hasSideEffects = false;
  bool m_executed = false;
};

} // namespace

TEST_CASE("[Unit] RenderGraph - Typed Handles RGTextureHandle and RGBufferHandle Operations") {
  using namespace NoMoreDay::render::graph;

  RenderGraphBuilder builder;

  // Test RGTextureHandle
  RGTextureHandle invalidTex;
  CHECK_FALSE(invalidTex.IsValid());

  ExtentPolicy extent;
  extent.mode = ExtentMode::MatchScreen;

  TypedResourceDescriptor texDesc;
  texDesc.tag = RenderResourceTag::SceneHdrColor;
  texDesc.name = "SceneColor";
  texDesc.kind = ResourceKind::Texture2D;
  texDesc.format = ResourceFormat::RGBA16F;
  texDesc.extentPolicy = extent;

  RGTextureHandle tex1 = builder.DeclareTexture(texDesc);
  CHECK(tex1.IsValid());
  CHECK(tex1.tag == RenderResourceTag::SceneHdrColor);

  RGTextureHandle tex2 = builder.DeclareTexture(texDesc);
  CHECK(tex1 == tex2);

  RGTextureHandle createdTex = builder.CreateTexture("CustomTempTex", ResourceFormat::RGBA8, extent);
  CHECK(createdTex.IsValid());
  CHECK(createdTex.name == "CustomTempTex");

  // Test RGBufferHandle
  RGBufferHandle invalidBuf;
  CHECK_FALSE(invalidBuf.IsValid());

  TypedResourceDescriptor bufDesc;
  bufDesc.tag = RenderResourceTag::LightBufferSSBO;
  bufDesc.name = "LightBufferSSBO";
  bufDesc.kind = ResourceKind::StorageBuffer;
  bufDesc.estimatedSizeBytes = 65536;

  RGBufferHandle buf1 = builder.DeclareBuffer(bufDesc);
  CHECK(buf1.IsValid());
  CHECK(buf1.tag == RenderResourceTag::LightBufferSSBO);

  RGBufferHandle createdBuf = builder.CreateBuffer("CustomSSBO", 32768);
  CHECK(createdBuf.IsValid());
  CHECK(createdBuf.name == "CustomSSBO");

  // Verify alignment helper
  CHECK(Align256(0) == 0);
  CHECK(Align256(1) == 256);
  CHECK(Align256(256) == 256);
  CHECK(Align256(257) == 512);
}

TEST_CASE("[Unit] RenderGraph - Reverse BFS Pass Culling of Unreachable Passes") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.OnResize(1920, 1080);

  // Pass 1: ScenePass (0) writes SceneHdrColor
  auto pass1 = std::make_shared<AD6MockPass>("ScenePass", [](RenderGraphBuilder &b) {
    b.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
  });
  graph.AddPass(pass1);

  // Pass 2: DistortionPass (14) writes DistortionLdrColor which nobody reads (Unreachable / Culled)
  auto pass2 = std::make_shared<AD6MockPass>("DistortionPass", [](RenderGraphBuilder &b) {
    b.Write(RenderResourceTag::DistortionLdrColor, RenderOwnerTag::Distortion);
  });
  graph.AddPass(pass2);

  // Pass 3: CompositePass (15) reads SceneHdrColor and writes FinalOutputColor (Root of reverse BFS)
  auto pass3 = std::make_shared<AD6MockPass>("CompositePass", [](RenderGraphBuilder &b) {
    b.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Composite);
    b.Write(RenderResourceTag::FinalOutputColor, RenderOwnerTag::Composite);
  });
  graph.AddPass(pass3);

  graph.Build();

  // Verify plan culling state
  CHECK_FALSE(graph.IsPassCulled("ScenePass"));
  CHECK(graph.IsPassCulled("DistortionPass"));
  CHECK_FALSE(graph.IsPassCulled("CompositePass"));

  const auto &plan = graph.GetCompiledPlan();
  CHECK(plan.cullingInfo.totalPassCount == 3);
  CHECK(plan.cullingInfo.culledPassCount == 1);
  CHECK(plan.cullingInfo.cullingRate > 0.3f);
  CHECK(plan.cullingInfo.culledPassNames.size() == 1);
  CHECK(plan.cullingInfo.culledPassNames[0] == "DistortionPass");

  // Verify execution skips culled pass
  RenderContext ctx;
  graph.Execute(ctx);

  CHECK(pass1->WasExecuted());
  CHECK_FALSE(pass2->WasExecuted());
  CHECK(pass3->WasExecuted());
}

TEST_CASE("[Unit] RenderGraph - Pass Culling Side Effect and Export Protection") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.OnResize(1920, 1080);

  // Pass 1: ScenePass (0) has side effects declared via virtual method
  auto pass1 = std::make_shared<AD6MockPass>("ScenePass", [](RenderGraphBuilder &b) {
    b.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
  }, nullptr, true /* hasSideEffects */);
  graph.AddPass(pass1);

  // Pass 2: RadianceCascadesPass (5) exports a resource
  auto pass2 = std::make_shared<AD6MockPass>("RadianceCascadesPass", [](RenderGraphBuilder &b) {
    b.Write(RenderResourceTag::RadianceMap, RenderOwnerTag::RadianceCascades);
    b.ExportResource(RenderResourceTag::RadianceMap);
  });
  graph.AddPass(pass2);

  // Pass 3: DistortionPass (14) has side effects declared via builder
  auto pass3 = std::make_shared<AD6MockPass>("DistortionPass", [](RenderGraphBuilder &b) {
    b.SetHasSideEffects(true);
    b.Write(RenderResourceTag::DistortionLdrColor, RenderOwnerTag::Distortion);
  });
  graph.AddPass(pass3);

  // Pass 4: CompositePass (15) writes FinalOutputColor
  auto pass4 = std::make_shared<AD6MockPass>("CompositePass", [](RenderGraphBuilder &b) {
    b.Write(RenderResourceTag::FinalOutputColor, RenderOwnerTag::Composite);
  });
  graph.AddPass(pass4);

  graph.Build();

  CHECK_FALSE(graph.IsPassCulled("ScenePass"));
  CHECK_FALSE(graph.IsPassCulled("RadianceCascadesPass"));
  CHECK_FALSE(graph.IsPassCulled("DistortionPass"));
  CHECK_FALSE(graph.IsPassCulled("CompositePass"));

  const auto &plan = graph.GetCompiledPlan();
  CHECK(plan.cullingInfo.culledPassCount == 0);

  RenderContext ctx;
  graph.Execute(ctx);

  CHECK(pass1->WasExecuted());
  CHECK(pass2->WasExecuted());
  CHECK(pass3->WasExecuted());
  CHECK(pass4->WasExecuted());
}

TEST_CASE("[Unit] RenderGraph - Culled Pass GPUTimer Slot Bookkeeping") {
  using namespace NoMoreDay::render::graph;
  using NoMoreDay::render::debug::GPUTimerQueryRing;
  using NoMoreDay::render::debug::SlotState;

  RenderGraph graph;
  graph.OnResize(1920, 1080);

  auto pass1 = std::make_shared<AD6MockPass>("DistortionPass", [](RenderGraphBuilder &b) {
    b.Write(RenderResourceTag::DistortionLdrColor, RenderOwnerTag::Distortion);
  });
  graph.AddPass(pass1);

  auto pass2 = std::make_shared<AD6MockPass>("CompositePass", [](RenderGraphBuilder &b) {
    b.Write(RenderResourceTag::FinalOutputColor, RenderOwnerTag::Composite);
  });
  graph.AddPass(pass2);

  graph.Build();
  CHECK(graph.IsPassCulled("DistortionPass"));

  RenderContext ctx;
  graph.Execute(ctx);

  auto &timerRing = GPUTimerQueryRing::Get();
  uint32_t stablePassId = 1;
  timerRing.DiscardPass(stablePassId);
  SlotState slotState = timerRing.DebugGetSlotState(0, stablePassId);
  CHECK((slotState == SlotState::Discarded || slotState == SlotState::Free));
}

TEST_CASE("[Unit] RenderGraph - Transient Resource Lifetime Intervals and Aliasing Table") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.OnResize(1920, 1080);
  graph.SetTransientAliasingEnabled(true);

  // Pass 1: writes DistortionLdrColor
  auto pass1 = std::make_shared<AD6MockPass>("DistortionPass", [](RenderGraphBuilder &b) {
    b.SetHasSideEffects(true);
    b.Write(RenderResourceTag::DistortionLdrColor, RenderOwnerTag::Distortion);
  });
  graph.AddPass(pass1);

  // Pass 2: writes FinalOutputColor
  auto pass2 = std::make_shared<AD6MockPass>("CompositePass", [](RenderGraphBuilder &b) {
    b.Write(RenderResourceTag::FinalOutputColor, RenderOwnerTag::Composite);
  });
  graph.AddPass(pass2);

  graph.Build();

  const auto &plan = graph.GetCompiledPlan();
  CHECK(plan.aliasingTable.entries.size() > 0);

  for (const auto &entry : plan.aliasingTable.entries) {
    CHECK(entry.allocatedSizeBytes % kAliasingAlignmentBytes == 0);
    CHECK(entry.byteOffset % kAliasingAlignmentBytes == 0);
  }
}

TEST_CASE("[Unit] RenderGraph - Plan Compilation Key Caching and Invalidation") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.OnResize(1920, 1080);

  auto pass1 = std::make_shared<AD6MockPass>("ScenePass", [](RenderGraphBuilder &b) {
    b.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
  });
  graph.AddPass(pass1);

  auto pass2 = std::make_shared<AD6MockPass>("CompositePass", [](RenderGraphBuilder &b) {
    b.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
    b.Write(RenderResourceTag::FinalOutputColor, RenderOwnerTag::Composite);
  });
  graph.AddPass(pass2);

  // First Build -> Cache Miss
  graph.Build();
  CHECK(graph.GetCompilationCacheMisses() == 1);
  CHECK(graph.GetCompilationCacheHits() == 0);

  // Second Build (identical graph) -> Cache Hit
  graph.Build();
  CHECK(graph.GetCompilationCacheMisses() == 1);
  CHECK(graph.GetCompilationCacheHits() == 1);

  // Resize -> Cache Invalidation -> Cache Miss
  graph.OnResize(1280, 720);
  graph.Build();
  CHECK(graph.GetCompilationCacheMisses() == 2);

  // Explicit Invalidation -> Cache Miss
  graph.InvalidateCompilationCache();
  graph.Build();
  CHECK(graph.GetCompilationCacheMisses() == 3);
}

TEST_CASE("[Unit] RenderGraph - Aliasing Group Key Rejects SampleCount/Mip/Usage Mismatches (H3)") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.OnResize(1920, 1080);
  graph.SetTransientAliasingEnabled(true);

  ExtentPolicy full;
  full.mode = ExtentMode::MatchScreen;

  // Base single-sample RGBA16F color target.
  TypedResourceDescriptor linearDesc;
  linearDesc.name = "LinearTarget";
  linearDesc.kind = ResourceKind::Texture2D;
  linearDesc.format = ResourceFormat::RGBA16F;
  linearDesc.extentPolicy = full;
  linearDesc.usageFlags = ResourceUsage::ColorAttachment;
  linearDesc.lifetime = ResourceLifetime::Transient;

  // Same kind/format but MSAA (sampleCount=4) - must be excluded entirely.
  TypedResourceDescriptor msaaDesc = linearDesc;
  msaaDesc.name = "MsaaTarget";
  msaaDesc.sampleCount = 4;

  // Same kind/format/sample/mip but different usage - must not share a group.
  TypedResourceDescriptor usageDesc = linearDesc;
  usageDesc.name = "UsageTarget";
  usageDesc.usageFlags = ResourceUsage::ColorAttachment | ResourceUsage::ShaderRead;

  // Same kind/format/sample/usage but deeper mip chain - must not share a group.
  TypedResourceDescriptor mipDesc = linearDesc;
  mipDesc.name = "MipTarget";
  mipDesc.mipLevels = 4;

  // Each pass writes one resource in its own pass index, so every interval is
  // disjoint; only the group compatibility key decides whether they share.
  // Pass order follows the locked contract sequence
  // (Scene < RadianceCascades < Distortion < Composite).
  auto pass1 = std::make_shared<AD6MockPass>("ScenePass", [&](RenderGraphBuilder &b) {
    b.SetHasSideEffects(true);
    b.Write(b.DeclareTexture(linearDesc), RenderOwnerTag::Scene,
            PipelineStage::FramebufferAttachment, ResourceUsage::ColorAttachment);
  });
  graph.AddPass(pass1);

  auto pass2 = std::make_shared<AD6MockPass>("RadianceCascadesPass", [&](RenderGraphBuilder &b) {
    b.SetHasSideEffects(true);
    b.Write(b.DeclareTexture(mipDesc), RenderOwnerTag::RadianceCascades,
            PipelineStage::FramebufferAttachment, ResourceUsage::ColorAttachment);
  });
  graph.AddPass(pass2);

  auto pass3 = std::make_shared<AD6MockPass>("DistortionPass", [&](RenderGraphBuilder &b) {
    b.SetHasSideEffects(true);
    b.Write(b.DeclareTexture(msaaDesc), RenderOwnerTag::Distortion,
            PipelineStage::FramebufferAttachment, ResourceUsage::ColorAttachment);
  });
  graph.AddPass(pass3);

  auto pass4 = std::make_shared<AD6MockPass>("CompositePass", [&](RenderGraphBuilder &b) {
    b.SetHasSideEffects(true);
    b.Write(b.DeclareTexture(usageDesc), RenderOwnerTag::Composite,
            PipelineStage::FramebufferAttachment,
            ResourceUsage::ColorAttachment | ResourceUsage::ShaderRead);
  });
  graph.AddPass(pass4);

  graph.Build();

  const auto &table = graph.GetAliasingTable();
  CHECK(table.entries.size() == 3); // LinearTarget, UsageTarget, MipTarget

  // MSAA target must be excluded from aliasing candidates entirely.
  for (const auto &entry : table.entries) {
    CHECK(entry.resourceName != "MsaaTarget");
  }

  // kind+format alone is no longer a sufficient compatibility key: each
  // remaining resource must live in its own group.
  std::unordered_set<uint32_t> groupIds;
  for (const auto &entry : table.entries) {
    groupIds.insert(entry.aliasGroupIndex);
  }
  CHECK(groupIds.size() == 3);
}

TEST_CASE("[Unit] RenderGraph - Depth/Stencil Targets Excluded from Aliasing (M6)") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.OnResize(1920, 1080);
  graph.SetTransientAliasingEnabled(true);

  // Tag-keyed SceneDepth resource (tag exclusion).
  auto pass1 = std::make_shared<AD6MockPass>("ScenePass", [](RenderGraphBuilder &b) {
    b.SetHasSideEffects(true);
    b.Write(RenderResourceTag::SceneDepth, RenderOwnerTag::Scene);
  });
  graph.AddPass(pass1);

  // Descriptor-keyed depth-format resource (format + usage-flag exclusion).
  ExtentPolicy full;
  full.mode = ExtentMode::MatchScreen;
  auto pass2 = std::make_shared<AD6MockPass>("CompositePass", [&](RenderGraphBuilder &b) {
    b.SetHasSideEffects(true);
    TypedResourceDescriptor depthDesc;
    depthDesc.name = "DepthTarget";
    depthDesc.kind = ResourceKind::Texture2D;
    depthDesc.format = ResourceFormat::Depth32F;
    depthDesc.extentPolicy = full;
    depthDesc.usageFlags = ResourceUsage::DepthAttachment;
    depthDesc.lifetime = ResourceLifetime::Transient;
    b.Write(b.DeclareTexture(depthDesc), RenderOwnerTag::Scene,
            PipelineStage::FramebufferAttachment, ResourceUsage::DepthAttachment);
  });
  graph.AddPass(pass2);

  graph.Build();

  // Neither depth resource may appear in the aliasing candidate set.
  const auto &table = graph.GetAliasingTable();
  CHECK(table.entries.empty());
}

// P2 AD-6 (H1): the compiled-plan cache is engine-level (cross-instance). A
// fresh graph instance whose PlanCompilationKey matches an earlier instance
// must hit without re-running validation or plan construction. This is the
// production shape: RenderSystem builds a new graph every frame.
TEST_CASE("[Unit] RenderGraph - Engine-Level Compiled Plan Cache Hit Across Instances (H1)") {
  using namespace NoMoreDay::render::graph;

  auto makePasses = [](RenderGraph &graph) {
    graph.OnResize(1920, 1080);
    auto pass1 = std::make_shared<AD6MockPass>("AD6CacheScenePass", [](RenderGraphBuilder &b) {
      b.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
    });
    graph.AddPass(pass1);
    auto pass2 = std::make_shared<AD6MockPass>("AD6CacheCompositePass", [](RenderGraphBuilder &b) {
      b.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Composite);
      b.Write(RenderResourceTag::FinalOutputColor, RenderOwnerTag::Composite);
    });
    graph.AddPass(pass2);
  };

  RenderGraph first;
  makePasses(first);
  first.Build();
  CHECK(first.GetCompilationCacheMisses() == 1);
  CHECK(first.GetCompilationCacheHits() == 0);

  // A brand-new instance with an identical compilation key must reuse the
  // engine-level cached plan: no miss, one hit.
  RenderGraph second;
  makePasses(second);
  second.Build();
  CHECK(second.GetCompilationCacheMisses() == 0);
  CHECK(second.GetCompilationCacheHits() == 1);

  // The reused plan is bit-identical (same compilation key, same pass order).
  CHECK(second.GetCompilationKey().GetCombinedHash() ==
        first.GetCompilationKey().GetCombinedHash());
  CHECK(second.GetCompiledPlan().passOrder == first.GetCompiledPlan().passOrder);

  // Execute on the cache-hit instance must work (plan is complete).
  RenderContext ctx;
  second.Execute(ctx);

  // Cleanup so other cases in this binary start with an empty engine cache.
  RenderGraph::ClearCompilationCache();
}

// P2 AD-6 (M1): extentPolicy.scale participates in the declaration hash. Two
// graphs that differ only in the declared scale must NOT share a cached plan.
TEST_CASE("[Unit] RenderGraph - ExtentPolicy.scale Change Invalidates Cached Plan (M1)") {
  using namespace NoMoreDay::render::graph;

  auto makePasses = [](RenderGraph &graph, float scale) {
    graph.OnResize(1920, 1080);
    auto pass1 = std::make_shared<AD6MockPass>("AD6ScaleScenePass", [scale](RenderGraphBuilder &b) {
      b.SetHasSideEffects(true);
      ExtentPolicy policy;
      policy.mode = ExtentMode::MatchScreen;
      policy.scale = scale;
      TypedResourceDescriptor desc;
      desc.name = "AD6ScaledHalfTarget";
      desc.kind = ResourceKind::Texture2D;
      desc.format = ResourceFormat::RGBA16F;
      desc.extentPolicy = policy;
      desc.usageFlags = ResourceUsage::ColorAttachment;
      desc.lifetime = ResourceLifetime::Transient;
      b.Write(b.DeclareTexture(desc), RenderOwnerTag::Scene,
              PipelineStage::FramebufferAttachment, ResourceUsage::ColorAttachment);
    });
    graph.AddPass(pass1);
    auto pass2 = std::make_shared<AD6MockPass>("AD6ScaleCompositePass", [](RenderGraphBuilder &b) {
      b.Write(RenderResourceTag::FinalOutputColor, RenderOwnerTag::Composite);
    });
    graph.AddPass(pass2);
  };

  RenderGraph graphHalf;
  makePasses(graphHalf, 0.5f);
  graphHalf.Build();
  CHECK(graphHalf.GetCompilationCacheMisses() == 1);

  // Same pass set, same descriptor name -- only the extent scale differs.
  // Must be a cache miss: the resolved resource dimensions changed.
  RenderGraph graphFull;
  makePasses(graphFull, 1.0f);
  graphFull.Build();
  CHECK(graphFull.GetCompilationCacheMisses() == 1);
  CHECK(graphFull.GetCompilationCacheHits() == 0);

  RenderGraph::ClearCompilationCache();
}

// P2 AD-6 (M1): the live dynamic-resolution scale participates in the quality
// & feature hash. A DRS change must invalidate the cached plan even when the
// screen size is unchanged.
TEST_CASE("[Unit] RenderGraph - Dynamic Resolution Scale Change Invalidates Cached Plan (M1)") {
  using namespace NoMoreDay::render::graph;

  auto makePasses = [](RenderGraph &graph, float drsScale) {
    graph.OnResize(1920, 1080);
    graph.SetDynamicResolutionScale(drsScale);
    auto pass1 = std::make_shared<AD6MockPass>("AD6DrsScenePass", [](RenderGraphBuilder &b) {
      b.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
    });
    graph.AddPass(pass1);
    auto pass2 = std::make_shared<AD6MockPass>("AD6DrsCompositePass", [](RenderGraphBuilder &b) {
      b.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Composite);
      b.Write(RenderResourceTag::FinalOutputColor, RenderOwnerTag::Composite);
    });
    graph.AddPass(pass2);
  };

  RenderGraph graphScale1;
  makePasses(graphScale1, 1.0f);
  graphScale1.Build();
  CHECK(graphScale1.GetCompilationCacheMisses() == 1);

  RenderGraph graphScale075;
  makePasses(graphScale075, 0.75f);
  graphScale075.Build();
  CHECK(graphScale075.GetCompilationCacheMisses() == 1);
  CHECK(graphScale075.GetCompilationCacheHits() == 0);

  RenderGraph::ClearCompilationCache();
}

// P2 AD-6 (H1): InvalidateCompilationCache() is a forced-recompile contract --
// it must also evict the engine-level cache so the next identical graph does
// not hit the stale plan.
TEST_CASE("[Unit] RenderGraph - Invalidation Also Evicts Engine-Level Cache (H1)") {
  using namespace NoMoreDay::render::graph;

  auto makePasses = [](RenderGraph &graph) {
    graph.OnResize(1920, 1080);
    auto pass1 = std::make_shared<AD6MockPass>("AD6InvalidateScenePass", [](RenderGraphBuilder &b) {
      b.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
    });
    graph.AddPass(pass1);
    auto pass2 = std::make_shared<AD6MockPass>("AD6InvalidateCompositePass", [](RenderGraphBuilder &b) {
      b.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Composite);
      b.Write(RenderResourceTag::FinalOutputColor, RenderOwnerTag::Composite);
    });
    graph.AddPass(pass2);
  };

  RenderGraph first;
  makePasses(first);
  first.Build();
  CHECK(first.GetCompilationCacheMisses() == 1);

  // Cross-instance hit proves the engine-level cache holds the plan.
  RenderGraph second;
  makePasses(second);
  second.Build();
  CHECK(second.GetCompilationCacheHits() == 1);

  // Forced invalidation evicts both the instance and engine-level caches.
  first.InvalidateCompilationCache();

  RenderGraph third;
  makePasses(third);
  third.Build();
  CHECK(third.GetCompilationCacheMisses() == 1);
  CHECK(third.GetCompilationCacheHits() == 0);

  RenderGraph::ClearCompilationCache();
}
