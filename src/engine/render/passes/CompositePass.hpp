#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include <functional>

namespace NoMoreDay::render::passes {

class CompositePass final : public graph::RenderPass {
public:
  using ExecuteCallback = std::function<void(graph::RenderContext &)>;

  explicit CompositePass(
      graph::RenderResourceTag inputResourceTag = graph::RenderResourceTag::SceneHdrColor,
      graph::RenderOwnerTag inputOwnerTag = graph::RenderOwnerTag::Composite,
      ExecuteCallback callback = {});

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override;
  graph::RenderPassType Type() const override;

private:
  graph::RenderResourceTag m_inputResourceTag = graph::RenderResourceTag::SceneHdrColor;
  graph::RenderOwnerTag m_inputOwnerTag = graph::RenderOwnerTag::Composite;
  ExecuteCallback m_callback;
};

} // namespace NoMoreDay::render::passes
