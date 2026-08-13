#include "engine/render/passes/VFXEmissionSnapshotPass.hpp"

#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"

#include <utility>

namespace NoMoreDay::render::passes {

VFXEmissionSnapshotPass::VFXEmissionSnapshotPass(ExecuteCallback callback)
    : m_callback(std::move(callback)) {}

void VFXEmissionSnapshotPass::Setup(graph::RenderGraphBuilder &builder) {
  // The pass renders the particle emission quads into the particle emissive
  // FBO (RadianceCascadesPass::m_particleEmissive, RGBA16F at full screen
  // size). RadianceCascadesPass owns that backing: it creates/resizes it in
  // EnsureResources and destroys it in Shutdown. The graph only observes the
  // write; it never allocates, resizes, frees, or GL-binds this backing, and
  // the manual surface inside the execute callback stays authoritative.
  graph::TypedResourceDescriptor emissiveDesc;
  emissiveDesc.name = "ParticleEmissive";
  emissiveDesc.tag = graph::RenderResourceTag::ParticleEmissive;
  emissiveDesc.ownerTag = graph::RenderOwnerTag::RadianceCascades;
  emissiveDesc.kind = graph::ResourceKind::Texture2D;
  emissiveDesc.format = graph::ResourceFormat::RGBA16F; // V5GI::kEmissiveFormat
  emissiveDesc.lifetime = graph::ResourceLifetime::Persistent;
  builder.DeclareResource(emissiveDesc);

  builder.Write(graph::RenderResourceTag::ParticleEmissive,
                graph::RenderOwnerTag::RadianceCascades,
                graph::PipelineStage::Fragment,
                graph::ResourceUsage::ColorAttachment);

  // External backing import contract: m_particleEmissive is a FramebufferManager
  // framebuffer owned by RadianceCascadesPass (EnsureResources/Shutdown).
  // Observer-only metadata: the graph never allocates, resizes, frees, or
  // GL-binds this backing.
  graph::ResourceImportInfo emissiveImport;
  emissiveImport.resourceTag = graph::RenderResourceTag::ParticleEmissive;
  emissiveImport.kind = graph::ResourceKind::Texture2D;
  emissiveImport.format = graph::ResourceFormat::RGBA16F;
  emissiveImport.backingOwner = graph::RenderOwnerTag::RadianceCascades;
  emissiveImport.resizeFollowsScreen = true; // EnsureResources recreates backing at screen size
  emissiveImport.colorAttachmentIndex = 0;
  builder.ImportResource(emissiveImport);
}

void VFXEmissionSnapshotPass::Execute(graph::RenderContext &context) {
  if (m_callback) {
    m_callback(context);
  }
}

const char *VFXEmissionSnapshotPass::GetName() const {
  return "VFXEmissionSnapshotPass";
}

graph::RenderPassType VFXEmissionSnapshotPass::Type() const {
  return graph::RenderPassType::VFX;
}

} // namespace NoMoreDay::render::passes
