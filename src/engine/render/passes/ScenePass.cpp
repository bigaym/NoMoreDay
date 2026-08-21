#include "engine/render/passes/ScenePass.hpp"

#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"

namespace NoMoreDay::render::passes {

ScenePass::ScenePass(ExecuteCallback callback) : m_callback(std::move(callback)) {}

void ScenePass::Setup(graph::RenderGraphBuilder &builder) {
  graph::TypedResourceDescriptor desc{};
  desc.tag = graph::RenderResourceTag::SceneHdrColor;
  desc.name = "SceneHdrColor";
  desc.kind = graph::ResourceKind::Texture2D;
  desc.format = graph::ResourceFormat::RGBA16F;
  desc.extentPolicy = graph::ExtentPolicy{graph::ExtentMode::MatchScreen};
  desc.usageFlags = graph::ResourceUsage::ColorAttachment;
  desc.lifetime = graph::ResourceLifetime::Transient;
  builder.DeclareResource(desc);

  builder.Write(graph::RenderResourceTag::SceneHdrColor,
                graph::RenderOwnerTag::Scene,
                graph::PipelineStage::FramebufferAttachment,
                graph::ResourceUsage::ColorAttachment);
}

void ScenePass::Execute(graph::RenderContext &context) {
  if (m_callback) {
    m_callback(context);
  }
}

const char *ScenePass::GetName() const { return "ScenePass"; }

graph::RenderPassType ScenePass::Type() const {
  return graph::RenderPassType::Scene;
}

} // namespace NoMoreDay::render::passes
