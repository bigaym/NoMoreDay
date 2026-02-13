#include "engine/render/passes/VFXPass.hpp"

#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "rlgl.h"

namespace NoMoreDay::render::passes {

VFXPass::VFXPass(ExecuteCallback callback) : m_callback(std::move(callback)) {}

void VFXPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read("SceneColor");
  builder.Write("SceneColor");
}

void VFXPass::Execute(graph::RenderContext &context) {
  // Keep pass boundaries explicit to avoid leaking stale rlgl batch state.
  rlDrawRenderBatchActive();
  if (m_callback) {
    m_callback(context);
  }
  rlDrawRenderBatchActive();
}

const char *VFXPass::GetName() const { return "VFXPass"; }

} // namespace NoMoreDay::render::passes
