#include "engine/render/passes/VFXPass.hpp"

#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"

namespace NoMoreDay::render::passes {

VFXPass::VFXPass(ExecuteCallback callback) : m_callback(std::move(callback)) {}

void VFXPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read("SceneColor");
  builder.Write("SceneColor");
}

void VFXPass::Execute(graph::RenderContext &context) {
  if (m_callback) {
    m_callback(context);
  }
}

const char *VFXPass::GetName() const { return "VFXPass"; }

} // namespace NoMoreDay::render::passes
