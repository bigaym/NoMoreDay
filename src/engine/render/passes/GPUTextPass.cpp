#include "engine/render/passes/GPUTextPass.hpp"

#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "rlgl.h"
#include <utility>

namespace NoMoreDay::render::passes {

GPUTextPass::GPUTextPass(ExecuteCallback callback)
    : m_callback(std::move(callback)) {}

void GPUTextPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read(graph::RenderResourceTag::SceneHdrColor,
               graph::RenderOwnerTag::VFX);
  builder.Write(graph::RenderResourceTag::SceneHdrColor,
                graph::RenderOwnerTag::VFX);
}

void GPUTextPass::Execute(graph::RenderContext &context) {
  rlDrawRenderBatchActive();
  if (m_callback) {
    m_callback(context);
  }
  rlDrawRenderBatchActive();
}

const char *GPUTextPass::GetName() const { return "GPUTextPass"; }

} // namespace NoMoreDay::render::passes
