#include "engine/render/passes/ShadowBuildPass.hpp"

#include "engine/render/core/RenderSyncContracts.hpp"

namespace NoMoreDay::render::passes {

void ShadowBuildPass::Setup(graph::RenderGraphBuilder &builder) {
  (void)builder;
}

void ShadowBuildPass::Execute(graph::RenderContext &context) {
  (void)context;
  // Baseline placeholder for V3 pass slot.
  core::ApplyRlglFlushTemplate();
}

} // namespace NoMoreDay::render::passes
