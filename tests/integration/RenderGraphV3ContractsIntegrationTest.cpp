#include "doctest.h"

#include "engine/render/core/BindingRegistry.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/passes/CompositePass.hpp"
#include "engine/render/passes/LightCullingPass.hpp"
#include "engine/render/passes/LightingPass.hpp"
#include "engine/render/passes/ScenePass.hpp"
#include "engine/render/passes/ShadowBuildPass.hpp"
#include "engine/render/passes/ShadowPreparePass.hpp"
#include "engine/render/passes/ShadowResolvePass.hpp"
#include "engine/render/passes/VFXPass.hpp"
#include "engine/render/passes/UIWorldPass.hpp"

#include <array>
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

TEST_CASE("[Integration] RenderGraph V3 Contracts - Pass order violation detection") {
  using namespace NoMoreDay::render;

  graph::RenderGraph graph;
  graph.AddPass(std::make_shared<passes::ScenePass>());
  graph.AddPass(std::make_shared<passes::VFXPass>());
  graph.AddPass(std::make_shared<passes::LightingPass>());
  graph.AddPass(std::make_shared<passes::CompositePass>(
      graph::RenderResourceTag::SceneHdrColor, graph::RenderOwnerTag::Lighting));

#if defined(NDEBUG)
  CHECK_NOTHROW(graph.Build());
#else
  CHECK_THROWS_AS(graph.Build(), std::logic_error);
#endif
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(),
                           "pass order violation against locked contract sequence"));
}

TEST_CASE("[Integration] RenderGraph V3 Contracts - Binding conflict detection") {
  using namespace NoMoreDay::render::core;

  CHECK(!BindingRegistry::HasAnyConflicts());
  CHECK(!BindingRegistry::HasDomainConflicts(BindingDomain::Global));
  CHECK(!BindingRegistry::HasDomainConflicts(BindingDomain::LightCulling));
  CHECK(!BindingRegistry::HasDomainConflicts(BindingDomain::ShadowPrepare));
  CHECK(!BindingRegistry::HasDomainConflicts(BindingDomain::ShadowBuild));
  CHECK(!BindingRegistry::HasDomainConflicts(BindingDomain::ShadowResolve));

  const std::array<uint32_t, 4> noConflicts = {0u, 1u, 2u, 3u};
  const std::array<uint32_t, 4> conflicts = {0u, 1u, 1u, 2u};
  CHECK(!BindingRegistry::HasConflicts(noConflicts));
  CHECK(BindingRegistry::HasConflicts(conflicts));
}

TEST_CASE("[Integration] RenderGraph V3 Contracts - Frame ownership violation detection") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "ScenePass", [](RenderGraphBuilder &builder) {
        builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
      }));
  graph.AddPass(std::make_shared<TestRenderPass>(
      "IllegalFinalWriterPass", [](RenderGraphBuilder &builder) {
        builder.Write(RenderResourceTag::FinalOutputColor,
                      RenderOwnerTag::PostProcess);
      }));

#if defined(NDEBUG)
  CHECK_NOTHROW(graph.Build());
#else
  CHECK_THROWS_AS(graph.Build(), std::logic_error);
#endif
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(),
                           "is not allowed to write this resource"));
}

TEST_CASE("[Integration] RenderGraph V3 Contracts - V2 baseline chain stays valid") {
  using namespace NoMoreDay::render;

  graph::RenderGraph graph;
  graph.AddPass(std::make_shared<passes::ScenePass>());
  graph.AddPass(std::make_shared<passes::VFXPass>());
  graph.AddPass(std::make_shared<passes::UIWorldPass>());
  graph.AddPass(std::make_shared<passes::CompositePass>(
      graph::RenderResourceTag::SceneHdrColor, graph::RenderOwnerTag::UIWorld));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
  CHECK(graph.GetPassCount() == 4);
}

TEST_CASE("[Integration] RenderGraph V3 Contracts - V3 no-op pass chain stays valid") {
  using namespace NoMoreDay::render;

  graph::RenderGraph graph;
  graph.AddPass(std::make_shared<passes::ScenePass>());
  graph.AddPass(std::make_shared<passes::ShadowPreparePass>());
  graph.AddPass(std::make_shared<passes::ShadowBuildPass>());
  graph.AddPass(std::make_shared<passes::ShadowResolvePass>());
  graph.AddPass(std::make_shared<passes::LightCullingPass>());
  graph.AddPass(std::make_shared<passes::VFXPass>());
  graph.AddPass(std::make_shared<passes::UIWorldPass>());
  graph.AddPass(std::make_shared<passes::CompositePass>(
      graph::RenderResourceTag::SceneHdrColor, graph::RenderOwnerTag::UIWorld));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
  CHECK(graph.GetPassCount() == 8);
}
