#include "engine/render/passes/ScenePass.hpp"

#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"

namespace NoMoreDay::render::passes {

ScenePass::ScenePass(ExecuteCallback callback) : m_callback(std::move(callback)) {}

void ScenePass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Write(graph::RenderResourceTag::SceneHdrColor,
                graph::RenderOwnerTag::Scene);
  builder.Write(graph::RenderResourceTag::SceneDepth,
                graph::RenderOwnerTag::Scene);
}

void ScenePass::Execute(graph::RenderContext &context) {
  if (m_callback) {
    m_callback(context);
  }
}

const char *ScenePass::GetName() const { return "ScenePass"; }

} // namespace NoMoreDay::render::passes
