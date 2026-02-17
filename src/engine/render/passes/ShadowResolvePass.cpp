#include "engine/render/passes/ShadowResolvePass.hpp"

#include "engine/render/core/RenderSyncContracts.hpp"

namespace NoMoreDay::render::passes {

void ShadowResolvePass::Setup(graph::RenderGraphBuilder &builder) {
  (void)builder;
}

void ShadowResolvePass::Execute(graph::RenderContext &context) {
  (void)context;
  // Baseline placeholder for V3 pass slot.
  core::ApplyComputeToFragmentBarrierTemplate();
  core::ApplyRlglFlushTemplate();
}

} // namespace NoMoreDay::render::passes
