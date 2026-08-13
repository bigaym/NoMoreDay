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
  graph::TypedResourceDescriptor finalDesc{};
  finalDesc.tag = graph::RenderResourceTag::FinalOutputColor;
  finalDesc.name = "FinalOutputColor";
  finalDesc.kind = graph::ResourceKind::Framebuffer;
  finalDesc.format = graph::ResourceFormat::R8;
  finalDesc.extentPolicy = graph::ExtentPolicy{graph::ExtentMode::MatchScreen};
  finalDesc.usageFlags = graph::ResourceUsage::ColorAttachment;
  finalDesc.lifetime = graph::ResourceLifetime::External;
  builder.DeclareResource(finalDesc);

  builder.Read(m_inputResourceTag, m_inputOwnerTag,
               graph::PipelineStage::Fragment,
               graph::ResourceUsage::ShaderRead);
  builder.Write(graph::RenderResourceTag::FinalOutputColor,
                graph::RenderOwnerTag::Composite,
                graph::PipelineStage::FramebufferAttachment,
                graph::ResourceUsage::ColorAttachment);
}

void CompositePass::Execute(graph::RenderContext &context) {
  if (m_callback) {
    m_callback(context);
  }
}

const char *CompositePass::GetName() const { return "CompositePass"; }

graph::RenderPassType CompositePass::Type() const {
  return graph::RenderPassType::Composite;
}

} // namespace NoMoreDay::render::passes
