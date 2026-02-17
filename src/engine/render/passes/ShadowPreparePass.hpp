#pragma once

#include "engine/render/graph/RenderPass.hpp"

namespace NoMoreDay::render::passes {

class ShadowPreparePass final : public graph::RenderPass {
public:
  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "ShadowPreparePass"; }
};

} // namespace NoMoreDay::render::passes
