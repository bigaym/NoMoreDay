#include "doctest.h"

#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/passes/CompositePass.hpp"
#include "engine/render/passes/FluidSimulationPass.hpp"
#include "engine/render/passes/GICompositePass.hpp"
#include "engine/render/passes/HeightShadowPass.hpp"
#include "engine/render/passes/JFAPass.hpp"
#include "engine/render/passes/LightingPass.hpp"
#include "engine/render/passes/OccluderExtractPass.hpp"
#include "engine/render/passes/RadianceCascadesPass.hpp"
#include "engine/render/passes/ScenePass.hpp"
#include "engine/render/passes/UIWorldPass.hpp"
#include "engine/render/passes/VFXPass.hpp"

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
