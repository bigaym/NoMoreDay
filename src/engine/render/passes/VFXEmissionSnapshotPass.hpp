#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include <functional>

namespace NoMoreDay::render::passes {

class VFXEmissionSnapshotPass final : public graph::RenderPass {
public:
  using ExecuteCallback = std::function<void(graph::RenderContext &)>;

  explicit VFXEmissionSnapshotPass(ExecuteCallback callback = {});

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override;
  graph::RenderPassType Type() const override;

private:
  ExecuteCallback m_callback;
};

} // namespace NoMoreDay::render::passes
