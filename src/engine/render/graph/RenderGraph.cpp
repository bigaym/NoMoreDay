#include "engine/render/graph/RenderGraph.hpp"

#include "engine/render/debug/RenderProfiler.hpp"
#include "engine/render/graph/RenderContext.hpp"

#include <chrono>
#include <optional>

namespace NoMoreDay::render::graph {

void RenderGraphBuilder::Read(const std::string &resourceName) {
  m_accesses.push_back({resourceName, ResourceAccess::Type::Read});
}

void RenderGraphBuilder::Write(const std::string &resourceName) {
  m_accesses.push_back({resourceName, ResourceAccess::Type::Write});
}

void RenderGraph::AddPass(std::shared_ptr<RenderPass> pass) {
  if (!pass) {
    return;
  }
  m_nodes.push_back({std::move(pass), {}});
  m_isBuilt = false;
}

void RenderGraph::Clear() {
  m_nodes.clear();
  m_isBuilt = false;
}

void RenderGraph::Build() {
  for (Node &node : m_nodes) {
    RenderGraphBuilder builder;
    node.pass->Setup(builder);
    node.accesses = builder.GetAccesses();
  }
  m_isBuilt = true;
}

void RenderGraph::Execute(RenderContext &context) {
  if (!m_isBuilt) {
    Build();
  }
  for (Node &node : m_nodes) {
    const auto passId = (context.renderProfiler != nullptr)
                            ? debug::RenderProfiler::FromPassName(
                                  node.pass->GetName())
                            : std::optional<debug::RenderPassId>{};
    if (context.renderProfiler != nullptr && passId.has_value()) {
      context.renderProfiler->BeginPass(*passId);
    }

    node.pass->Execute(context);

    if (context.renderProfiler != nullptr && passId.has_value()) {
      context.renderProfiler->EndPass(*passId);
    }
  }
}

} // namespace NoMoreDay::render::graph
