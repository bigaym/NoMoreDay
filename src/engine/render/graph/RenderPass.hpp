#pragma once

namespace NoMoreDay::render::graph {

class RenderGraphBuilder;
struct RenderContext;

class RenderPass {
public:
  virtual ~RenderPass() = default;
  virtual void Setup(RenderGraphBuilder &builder) = 0;
  virtual void Execute(RenderContext &context) = 0;
  virtual const char *GetName() const = 0;
};

} // namespace NoMoreDay::render::graph
