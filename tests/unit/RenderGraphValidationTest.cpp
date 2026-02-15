#include "doctest.h"

#include "engine/render/graph/RenderGraph.hpp"

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

TEST_CASE("[Unit] RenderGraph - Reject Undeclared Known Resource Usage") {
  using namespace NoMoreDay::render::graph;

  RenderGraph graph;
  graph.AddPass(std::make_shared<TestRenderPass>(
      "LegacyPass", [](RenderGraphBuilder &builder) {
        builder.Write("SceneColor");
      }));

#if defined(NDEBUG)
  CHECK_NOTHROW(graph.Build());
#else
  CHECK_THROWS_AS(graph.Build(), std::logic_error);
#endif
  CHECK(graph.HasValidationErrors());
  CHECK(HasErrorContaining(graph.GetValidationDiagnostics(), "LegacyPass",
                           "SceneColor",
                           "known resource write must declare an owner tag"));
}
