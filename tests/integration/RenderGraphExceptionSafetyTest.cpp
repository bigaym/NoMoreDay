#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "raylib.h"

#include <cstdint>
#include <memory>
#include <stdexcept>

// F1 closure tests for review #9 (RenderGraph::Execute exception safety):
// a throwing pass must fail soft — the exception is swallowed, subsequent
// passes are skipped, timer/profiler/graph state is restored, and the next
// frame executes normally. Local contract verification only; never implies a
// production GO verdict.

namespace {

bool RGESCreateMinimalGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "RenderGraph Exception Safety Test Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  return NoMoreDay::utils::GPUUtils::IsInitialized();
}

// Pass that records execution and can optionally throw from Execute.
class ProbeRenderPass : public NoMoreDay::render::graph::RenderPass {
public:
  ProbeRenderPass(std::string name, bool &executed, bool &throwStd,
                  bool &throwNonStd)
      : m_name(std::move(name)), m_executed(executed), m_throwStd(throwStd),
        m_throwNonStd(throwNonStd) {}

  void Setup(NoMoreDay::render::graph::RenderGraphBuilder &) override {}

  void Execute(NoMoreDay::render::graph::RenderContext &) override {
    m_executed = true;
    if (m_throwStd) {
      throw std::runtime_error("F1 probe pass: deliberate std exception");
    }
    if (m_throwNonStd) {
      throw 42; // non-std exception: must still fail soft
    }
  }

  const char *GetName() const override { return m_name.c_str(); }

  // Probe passes must survive pass culling: no resource declarations, so mark
  // them side-effecting to stay reachable in the compiled plan.
  [[nodiscard]] bool HasSideEffects() const override { return true; }

  NoMoreDay::render::graph::RenderPassType Type() const override {
    return NoMoreDay::render::graph::RenderPassType::Scene;
  }

private:
  std::string m_name;
  bool &m_executed;
  bool &m_throwStd;
  bool &m_throwNonStd;
};

} // namespace

TEST_CASE("[Integration] RenderGraph - throwing pass fails soft and restores state") {
  if (!RGESCreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping exception safety test");
  }

  using namespace NoMoreDay;
  using namespace NoMoreDay::render::graph;

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::High);

  ResourceManager resources;
  render::graph::RenderContext context = {};
  context.registry = nullptr;
  context.resources = &resources;
  context.qualityManager = &qm;
  context.camera = nullptr;

  bool firstExecuted = false;
  bool throwerExecuted = false;
  bool trailingExecuted = false;
  bool firstThrowStd = false;
  bool throwStd = true;
  bool trailingThrowStd = false;
  bool firstThrowNonStd = false;
  bool throwNonStd = false;
  bool trailingThrowNonStd = false;

  RenderGraph graph;
  graph.AddPass(std::make_shared<ProbeRenderPass>(
      "ProbePassA", firstExecuted, firstThrowStd, firstThrowNonStd));
  graph.AddPass(std::make_shared<ProbeRenderPass>(
      "ProbePassB", throwerExecuted, throwStd, throwNonStd));
  graph.AddPass(std::make_shared<ProbeRenderPass>(
      "ProbePassC", trailingExecuted, trailingThrowStd, trailingThrowNonStd));

  REQUIRE_NOTHROW(graph.Build());
  REQUIRE_FALSE(graph.HasValidationErrors());

  // Frame 1: the middle pass throws. Execute must swallow the exception, skip
  // the trailing pass, and restore graph/timer state.
  CHECK_NOTHROW(graph.Execute(context));
  CHECK(firstExecuted);
  CHECK(throwerExecuted);
  CHECK_FALSE(trailingExecuted);
  CHECK(context.activeGraph == nullptr);

  // Frame 2: no throw — the full pass chain must run again, proving the timer
  // ring and graph state survived the failure.
  throwStd = false;
  firstExecuted = false;
  throwerExecuted = false;
  trailingExecuted = false;
  CHECK_NOTHROW(graph.Execute(context));
  CHECK(firstExecuted);
  CHECK(throwerExecuted);
  CHECK(trailingExecuted);
  CHECK(context.activeGraph == nullptr);

  render::debug::GPUTimerQueryRing::Get().Shutdown();
}

TEST_CASE("[Integration] RenderGraph - non-std exception fails soft") {
  if (!RGESCreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping exception safety test");
  }

  using namespace NoMoreDay;
  using namespace NoMoreDay::render::graph;

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::High);

  ResourceManager resources;
  render::graph::RenderContext context = {};
  context.registry = nullptr;
  context.resources = &resources;
  context.qualityManager = &qm;
  context.camera = nullptr;

  bool firstExecuted = false;
  bool throwerExecuted = false;
  bool trailingExecuted = false;
  bool firstThrowStd = false;
  bool throwStd = false;
  bool trailingThrowStd = false;
  bool firstThrowNonStd = false;
  bool throwNonStd = true;
  bool trailingThrowNonStd = false;

  RenderGraph graph;
  graph.AddPass(std::make_shared<ProbeRenderPass>(
      "ProbePassD", firstExecuted, firstThrowStd, firstThrowNonStd));
  graph.AddPass(std::make_shared<ProbeRenderPass>(
      "ProbePassE", throwerExecuted, throwStd, throwNonStd));
  graph.AddPass(std::make_shared<ProbeRenderPass>(
      "ProbePassF", trailingExecuted, trailingThrowStd, trailingThrowNonStd));

  REQUIRE_NOTHROW(graph.Build());
  REQUIRE_FALSE(graph.HasValidationErrors());

  CHECK_NOTHROW(graph.Execute(context));
  CHECK(firstExecuted);
  CHECK(throwerExecuted);
  CHECK_FALSE(trailingExecuted);
  CHECK(context.activeGraph == nullptr);

  // Subsequent frame still healthy.
  throwNonStd = false;
  trailingExecuted = false;
  CHECK_NOTHROW(graph.Execute(context));
  CHECK(trailingExecuted);

  render::debug::GPUTimerQueryRing::Get().Shutdown();
}
