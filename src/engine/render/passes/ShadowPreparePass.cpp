#include "engine/render/passes/ShadowPreparePass.hpp"

#include "engine/render/core/RenderSyncContracts.hpp"

namespace NoMoreDay::render::passes {

void ShadowPreparePass::Setup(graph::RenderGraphBuilder &builder) {
  (void)builder;
}

void ShadowPreparePass::Execute(graph::RenderContext &context) {
  (void)context;
  // Baseline placeholder for V3 pass slot.
  core::ApplyRlglFlushTemplate();
}

} // namespace NoMoreDay::render::passes
