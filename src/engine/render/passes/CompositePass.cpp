#include "engine/render/passes/CompositePass.hpp"

#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"

namespace NoMoreDay::render::passes {

CompositePass::CompositePass(ExecuteCallback callback)
    : m_callback(std::move(callback)) {}

void CompositePass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read("SceneColor");
  builder.Write("BackBuffer");
}

void CompositePass::Execute(graph::RenderContext &context) {
  if (m_callback) {
    m_callback(context);
  }
}

const char *CompositePass::GetName() const { return "CompositePass"; }

} // namespace NoMoreDay::render::passes
