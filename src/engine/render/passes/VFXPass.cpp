#include "engine/render/passes/VFXPass.hpp"

#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "rlgl.h"

namespace NoMoreDay::render::passes {

VFXPass::VFXPass(ExecuteCallback callback) : m_callback(std::move(callback)) {}

void VFXPass::Setup(graph::RenderGraphBuilder &builder) {
  graph::TypedResourceDescriptor vfxDesc{};
  vfxDesc.tag = graph::RenderResourceTag::VFXParticleSSBO;
  vfxDesc.name = "VFXParticleSSBO";
  vfxDesc.kind = graph::ResourceKind::StorageBuffer;
  vfxDesc.format = graph::ResourceFormat::R32F;
  vfxDesc.extentPolicy = graph::ExtentPolicy{graph::ExtentMode::Fixed};
  vfxDesc.usageFlags = graph::ResourceUsage::StorageBuffer;
  vfxDesc.lifetime = graph::ResourceLifetime::Persistent;
  builder.DeclareResource(vfxDesc);

  builder.Read(graph::RenderResourceTag::SceneHdrColor,
               graph::RenderOwnerTag::VFX,
               graph::PipelineStage::Fragment,
               graph::ResourceUsage::ShaderRead);
  builder.Read(graph::RenderResourceTag::SceneDepth,
               graph::RenderOwnerTag::VFX,
               graph::PipelineStage::Fragment,
               graph::ResourceUsage::ShaderRead);
  builder.Write(graph::RenderResourceTag::SceneHdrColor,
                graph::RenderOwnerTag::VFX,
                graph::PipelineStage::FramebufferAttachment,
                graph::ResourceUsage::ColorAttachment);
}

void VFXPass::Execute(graph::RenderContext &context) {
  rlDrawRenderBatchActive();
  if (m_callback) {
    m_callback(context);
  }
  rlDrawRenderBatchActive();
}

const char *VFXPass::GetName() const { return "VFXPass"; }

} // namespace NoMoreDay::render::passes
