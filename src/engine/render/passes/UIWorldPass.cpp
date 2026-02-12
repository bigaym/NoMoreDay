#include "engine/render/passes/UIWorldPass.hpp"

#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"

namespace NoMoreDay::render::passes {

UIWorldPass::UIWorldPass(ExecuteCallback callback)
    : m_callback(std::move(callback)) {}

void UIWorldPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read("SceneColor");
  builder.Write("SceneColor");
}

void UIWorldPass::Execute(graph::RenderContext &context) {
  if (m_callback) {
    m_callback(context);
  }
}

const char *UIWorldPass::GetName() const { return "UIWorldPass"; }

} // namespace NoMoreDay::render::passes
