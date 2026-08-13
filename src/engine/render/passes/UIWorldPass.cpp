#include "engine/render/passes/UIWorldPass.hpp"

#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"

namespace NoMoreDay::render::passes {

UIWorldPass::UIWorldPass(ExecuteCallback callback)
    : m_callback(std::move(callback)) {}

void UIWorldPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read(graph::RenderResourceTag::SceneHdrColor,
               graph::RenderOwnerTag::UIWorld);
  builder.Write(graph::RenderResourceTag::SceneHdrColor,
                graph::RenderOwnerTag::UIWorld);
}

void UIWorldPass::Execute(graph::RenderContext &context) {
  if (m_callback) {
    m_callback(context);
  }
}

const char *UIWorldPass::GetName() const { return "UIWorldPass"; }

graph::RenderPassType UIWorldPass::Type() const {
  return graph::RenderPassType::UIWorld;
}

} // namespace NoMoreDay::render::passes
