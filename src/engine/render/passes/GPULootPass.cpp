#include "engine/render/passes/GPULootPass.hpp"

#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "rlgl.h"
#include <utility>

namespace NoMoreDay::render::passes {

GPULootPass::GPULootPass(ExecuteCallback callback)
    : m_callback(std::move(callback)) {}

void GPULootPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read(graph::RenderResourceTag::SceneHdrColor, graph::RenderOwnerTag::VFX);
  builder.Write(graph::RenderResourceTag::SceneHdrColor, graph::RenderOwnerTag::VFX);
}

void GPULootPass::Execute(graph::RenderContext &context) {
  rlDrawRenderBatchActive();
  if (m_callback) {
    m_callback(context);
  }
  rlDrawRenderBatchActive();
}

const char *GPULootPass::GetName() const { return "GPULootPass"; }

graph::RenderPassType GPULootPass::Type() const {
  return graph::RenderPassType::GPULoot;
}

} // namespace NoMoreDay::render::passes
