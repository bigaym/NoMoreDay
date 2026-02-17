#pragma once

#include "engine/render/graph/RenderPass.hpp"

namespace NoMoreDay::render::passes {

class ShadowResolvePass final : public graph::RenderPass {
public:
  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "ShadowResolvePass"; }
};

} // namespace NoMoreDay::render::passes
