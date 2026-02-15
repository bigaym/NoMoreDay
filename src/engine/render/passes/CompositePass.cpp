#include "engine/render/passes/CompositePass.hpp"

#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"

namespace NoMoreDay::render::passes {

CompositePass::CompositePass(graph::RenderResourceTag inputResourceTag,
                             graph::RenderOwnerTag inputOwnerTag,
                             ExecuteCallback callback)
    : m_inputResourceTag(inputResourceTag), m_inputOwnerTag(inputOwnerTag),
      m_callback(std::move(callback)) {}

void CompositePass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read(m_inputResourceTag, m_inputOwnerTag);
  builder.Write(graph::RenderResourceTag::FinalOutputColor,
                graph::RenderOwnerTag::Composite);
}

void CompositePass::Execute(graph::RenderContext &context) {
  if (m_callback) {
    m_callback(context);
  }
}

const char *CompositePass::GetName() const { return "CompositePass"; }

} // namespace NoMoreDay::render::passes
