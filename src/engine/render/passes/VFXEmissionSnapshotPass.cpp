#include "engine/render/passes/VFXEmissionSnapshotPass.hpp"

#include "engine/render/graph/RenderContext.hpp"

#include <utility>

namespace NoMoreDay::render::passes {

VFXEmissionSnapshotPass::VFXEmissionSnapshotPass(ExecuteCallback callback)
    : m_callback(std::move(callback)) {}

void VFXEmissionSnapshotPass::Setup(graph::RenderGraphBuilder &) {}

void VFXEmissionSnapshotPass::Execute(graph::RenderContext &context) {
  if (m_callback) {
    m_callback(context);
  }
}

const char *VFXEmissionSnapshotPass::GetName() const {
  return "VFXEmissionSnapshotPass";
}

} // namespace NoMoreDay::render::passes
