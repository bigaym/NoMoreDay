#include "engine/render/passes/LightCullingPass.hpp"

#include "engine/render/core/RenderSyncContracts.hpp"

namespace NoMoreDay::render::passes {

void LightCullingPass::Setup(graph::RenderGraphBuilder &builder) {
  (void)builder;
}

void LightCullingPass::Execute(graph::RenderContext &context) {
  (void)context;
  // Baseline placeholder for V3 pass slot.
  core::ApplyRlglFlushTemplate();
}

} // namespace NoMoreDay::render::passes
