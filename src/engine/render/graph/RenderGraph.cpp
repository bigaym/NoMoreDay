#include "engine/render/graph/RenderGraph.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/core/DeviceCapabilityMatrix.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/render/core/ScopedGLState.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/debug/RenderProfiler.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "rlgl.h"

#include <algorithm>
#include <bit>
#include <map>
#include <optional>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace NoMoreDay::render::graph {

bool RenderGraph::s_validationEnabled = true;

// P2 AD-6 (H1): engine-level (cross-instance) compiled-plan cache. Bounded:
// plans are keyed by the combined PlanCompilationKey; the map is cleared when
// it grows past kMaxCachedPlans so a long session with many distinct keys
// (resizes, feature toggles) cannot accumulate unbounded memory. Clearing only
// costs a future miss, never correctness.
std::unordered_map<uint64_t, RenderGraph::CachedPlanEntry>
    RenderGraph::s_compiledPlanCache;

namespace {
constexpr size_t kMaxCachedPlans = 64;

bool IsWriterAllowedForResource(RenderResourceTag resourceTag,
                                RenderOwnerTag ownerTag) {
  switch (resourceTag) {
  case RenderResourceTag::SceneHdrColor:
    return ownerTag == RenderOwnerTag::Scene ||
           ownerTag == RenderOwnerTag::Lighting ||
           ownerTag == RenderOwnerTag::HeightShadow ||
           ownerTag == RenderOwnerTag::GIComposite ||
           ownerTag == RenderOwnerTag::FluidSimulation ||
           ownerTag == RenderOwnerTag::Volumetric ||
           ownerTag == RenderOwnerTag::VFX ||
           ownerTag == RenderOwnerTag::UIWorld;
  case RenderResourceTag::SceneDepth:
    return ownerTag == RenderOwnerTag::Scene;
  case RenderResourceTag::PostProcessLdrColor:
    return ownerTag == RenderOwnerTag::PostProcess;
  case RenderResourceTag::DistortionLdrColor:
    return ownerTag == RenderOwnerTag::Distortion;
  case RenderResourceTag::FinalOutputColor:
    return ownerTag == RenderOwnerTag::Composite;
  case RenderResourceTag::OccluderMask:
    return ownerTag == RenderOwnerTag::OccluderExtract;
  case RenderResourceTag::DistanceField:
    return ownerTag == RenderOwnerTag::JFA;
  case RenderResourceTag::EmissiveBuffer:
    return ownerTag == RenderOwnerTag::RadianceCascades;
  case RenderResourceTag::ParticleEmissive:
    return ownerTag == RenderOwnerTag::RadianceCascades;
  case RenderResourceTag::RadianceMap:
    return ownerTag == RenderOwnerTag::RadianceCascades;
  case RenderResourceTag::GIHistoryColor:
    return ownerTag == RenderOwnerTag::GIComposite;
  case RenderResourceTag::LightBufferSSBO:
  case RenderResourceTag::TileLightIndexSSBO:
    return ownerTag == RenderOwnerTag::Lighting;
  case RenderResourceTag::VFXParticleSSBO:
    return ownerTag == RenderOwnerTag::VFX;
  case RenderResourceTag::FluidParticleSSBO:
    return ownerTag == RenderOwnerTag::FluidSimulation;
  case RenderResourceTag::GPUTextBufferSSBO:
    return ownerTag == RenderOwnerTag::GPUText;
  case RenderResourceTag::GPULootBufferSSBO:
    return ownerTag == RenderOwnerTag::GPULoot;
  case RenderResourceTag::ShadowAtlas:
  case RenderResourceTag::ShadowDistanceField:
  case RenderResourceTag::ShadowMask:
  case RenderResourceTag::ShadowOccluderSSBO:
    return ownerTag == RenderOwnerTag::Shadow;
  case RenderResourceTag::ClusterHeaderSSBO:
  case RenderResourceTag::ClusterLightIndexSSBO:
  case RenderResourceTag::ClusterPackedLightSSBO:
  case RenderResourceTag::ClusterCounterSSBO:
  case RenderResourceTag::LightBoundsSSBO:
    return ownerTag == RenderOwnerTag::LightCulling;
  case RenderResourceTag::Custom:
  default:
    return true;
  }
}

bool IsFirstWriterValid(RenderResourceTag resourceTag, RenderOwnerTag ownerTag) {
  switch (resourceTag) {
  case RenderResourceTag::SceneHdrColor:
    return ownerTag == RenderOwnerTag::Scene;
  case RenderResourceTag::SceneDepth:
    return ownerTag == RenderOwnerTag::Scene;
  case RenderResourceTag::PostProcessLdrColor:
    return ownerTag == RenderOwnerTag::PostProcess;
  case RenderResourceTag::DistortionLdrColor:
    return ownerTag == RenderOwnerTag::Distortion;
  case RenderResourceTag::FinalOutputColor:
    return ownerTag == RenderOwnerTag::Composite;
  case RenderResourceTag::OccluderMask:
    return ownerTag == RenderOwnerTag::OccluderExtract;
  case RenderResourceTag::DistanceField:
    return ownerTag == RenderOwnerTag::JFA;
  case RenderResourceTag::EmissiveBuffer:
    return ownerTag == RenderOwnerTag::RadianceCascades;
  case RenderResourceTag::ParticleEmissive:
    return ownerTag == RenderOwnerTag::RadianceCascades;
  case RenderResourceTag::RadianceMap:
    return ownerTag == RenderOwnerTag::RadianceCascades;
  case RenderResourceTag::GIHistoryColor:
    return ownerTag == RenderOwnerTag::GIComposite;
  case RenderResourceTag::LightBufferSSBO:
  case RenderResourceTag::TileLightIndexSSBO:
    return ownerTag == RenderOwnerTag::Lighting;
  case RenderResourceTag::VFXParticleSSBO:
    return ownerTag == RenderOwnerTag::VFX;
  case RenderResourceTag::FluidParticleSSBO:
    return ownerTag == RenderOwnerTag::FluidSimulation;
  case RenderResourceTag::GPUTextBufferSSBO:
    return ownerTag == RenderOwnerTag::GPUText;
  case RenderResourceTag::GPULootBufferSSBO:
    return ownerTag == RenderOwnerTag::GPULoot;
  case RenderResourceTag::ShadowAtlas:
  case RenderResourceTag::ShadowDistanceField:
  case RenderResourceTag::ShadowMask:
  case RenderResourceTag::ShadowOccluderSSBO:
    return ownerTag == RenderOwnerTag::Shadow;
  case RenderResourceTag::ClusterHeaderSSBO:
  case RenderResourceTag::ClusterLightIndexSSBO:
  case RenderResourceTag::ClusterPackedLightSSBO:
  case RenderResourceTag::ClusterCounterSSBO:
  case RenderResourceTag::LightBoundsSSBO:
    return ownerTag == RenderOwnerTag::LightCulling;
  case RenderResourceTag::Custom:
  default:
    return true;
  }
}

bool IsAdditionalWriterValid(RenderResourceTag resourceTag,
                             RenderOwnerTag ownerTag) {
  switch (resourceTag) {
  case RenderResourceTag::SceneHdrColor:
    return ownerTag == RenderOwnerTag::Lighting ||
           ownerTag == RenderOwnerTag::HeightShadow ||
           ownerTag == RenderOwnerTag::GIComposite ||
           ownerTag == RenderOwnerTag::FluidSimulation ||
           ownerTag == RenderOwnerTag::Volumetric ||
           ownerTag == RenderOwnerTag::VFX ||
           ownerTag == RenderOwnerTag::UIWorld;
  case RenderResourceTag::Custom:
    return true;
  case RenderResourceTag::SceneDepth:
  case RenderResourceTag::PostProcessLdrColor:
  case RenderResourceTag::DistortionLdrColor:
  case RenderResourceTag::FinalOutputColor:
  case RenderResourceTag::OccluderMask:
  case RenderResourceTag::DistanceField:
  case RenderResourceTag::EmissiveBuffer:
  case RenderResourceTag::RadianceMap:
  case RenderResourceTag::GIHistoryColor:
  case RenderResourceTag::LightBufferSSBO:
  case RenderResourceTag::TileLightIndexSSBO:
  case RenderResourceTag::VFXParticleSSBO:
  case RenderResourceTag::FluidParticleSSBO:
  case RenderResourceTag::GPUTextBufferSSBO:
  case RenderResourceTag::GPULootBufferSSBO:
  default:
    return false;
  }
}

RenderOwnerTag ExpectedFirstWriter(RenderResourceTag resourceTag) {
  switch (resourceTag) {
  case RenderResourceTag::SceneHdrColor:
  case RenderResourceTag::SceneDepth:
    return RenderOwnerTag::Scene;
  case RenderResourceTag::PostProcessLdrColor:
    return RenderOwnerTag::PostProcess;
  case RenderResourceTag::DistortionLdrColor:
    return RenderOwnerTag::Distortion;
  case RenderResourceTag::FinalOutputColor:
    return RenderOwnerTag::Composite;
  case RenderResourceTag::OccluderMask:
    return RenderOwnerTag::OccluderExtract;
  case RenderResourceTag::DistanceField:
    return RenderOwnerTag::JFA;
  case RenderResourceTag::EmissiveBuffer:
    return RenderOwnerTag::RadianceCascades;
  case RenderResourceTag::ParticleEmissive:
    return RenderOwnerTag::RadianceCascades;
  case RenderResourceTag::RadianceMap:
    return RenderOwnerTag::RadianceCascades;
  case RenderResourceTag::GIHistoryColor:
    return RenderOwnerTag::GIComposite;
  case RenderResourceTag::LightBufferSSBO:
  case RenderResourceTag::TileLightIndexSSBO:
    return RenderOwnerTag::Lighting;
  case RenderResourceTag::VFXParticleSSBO:
    return RenderOwnerTag::VFX;
  case RenderResourceTag::FluidParticleSSBO:
    return RenderOwnerTag::FluidSimulation;
  case RenderResourceTag::GPUTextBufferSSBO:
    return RenderOwnerTag::GPUText;
  case RenderResourceTag::GPULootBufferSSBO:
    return RenderOwnerTag::GPULoot;
  case RenderResourceTag::ShadowAtlas:
  case RenderResourceTag::ShadowDistanceField:
  case RenderResourceTag::ShadowMask:
  case RenderResourceTag::ShadowOccluderSSBO:
    return RenderOwnerTag::Shadow;
  case RenderResourceTag::ClusterHeaderSSBO:
  case RenderResourceTag::ClusterLightIndexSSBO:
  case RenderResourceTag::ClusterPackedLightSSBO:
  case RenderResourceTag::ClusterCounterSSBO:
  case RenderResourceTag::LightBoundsSSBO:
    return RenderOwnerTag::LightCulling;
  case RenderResourceTag::Custom:
  default:
    return RenderOwnerTag::Unknown;
  }
}

bool IsKnownResource(RenderResourceTag resourceTag) {
  return resourceTag != RenderResourceTag::Custom;
}

enum class PassContractStage : int {
  Scene = 0,
  Shadow = 1,
  LightCulling = 2,
  Lighting = 3,
  HeightShadow = 4,
  OccluderExtract = 5,
  JFA = 6,
  RadianceCascades = 7,
  GIComposite = 8,
  FluidSimulation = 9,
  Volumetric = 10,
  VFX = 11,
  GPUText = 12,
  GPULoot = 13,
  UIWorld = 14,
  PostProcess = 15,
  Distortion = 16,
  Composite = 17,
};

// Contract stage of each enum-typed pass. The Shadow-pipeline passes share the
// Shadow stage (the singular-stage check exempts it).
constexpr std::array<PassContractStage,
                     static_cast<size_t>(RenderPassType::Count)>
    kPassContractStages = {
        PassContractStage::Scene,        // Scene
        PassContractStage::Lighting,     // Lighting
        PassContractStage::HeightShadow, // HeightShadow
        PassContractStage::OccluderExtract,    // OccluderExtract
        PassContractStage::JFA,          // JFA
        PassContractStage::RadianceCascades,   // RadianceCascades
        PassContractStage::GIComposite,  // GIComposite
        PassContractStage::FluidSimulation,    // FluidSimulation
        PassContractStage::Volumetric,   // Volumetric
        PassContractStage::VFX,          // VFX
        PassContractStage::GPUText,      // GPUText
        PassContractStage::GPULoot,      // GPULoot
        PassContractStage::UIWorld,      // UIWorld
        PassContractStage::PostProcess,  // PostProcess
        PassContractStage::Distortion,   // Distortion
        PassContractStage::Composite,    // Composite
        PassContractStage::LightCulling, // LightCulling
        PassContractStage::Shadow,       // ShadowPrepare
        PassContractStage::Shadow,       // ShadowBuild
        PassContractStage::Shadow,       // ShadowResolve
    };

std::optional<PassContractStage> ResolvePassContractStage(
    RenderPassType passType, std::string_view passName) {
  const size_t index = static_cast<size_t>(passType);
  if (index >= kPassContractStages.size()) {
    return std::nullopt;
  }
  // A pass whose runtime name is not the canonical table name of its type
  // (e.g. the VFX emission snapshot helper, which shares the VFX type) is
  // exempt from the stage contract, matching the legacy string lookup.
  if (passName != kRenderPassNames[index].full) {
    return std::nullopt;
  }
  return kPassContractStages[index];
}

// B12 graph-driven binding: kind-compatibility helpers between a binding kind
// and the imported backing kind admitted for it.
bool IsBufferBindingKind(ResourceKind kind) {
  return kind == ResourceKind::StorageBuffer || kind == ResourceKind::UniformBuffer;
}

bool IsImageBindingKind(ResourceKind kind) {
  return kind == ResourceKind::Texture2D || kind == ResourceKind::Texture2DArray ||
         kind == ResourceKind::Framebuffer;
}

} // namespace

void RenderGraphBuilder::Read(const std::string &resourceName) {
  const RenderResourceTag inferredTag = ToResourceTag(resourceName);
  m_accesses.push_back({resourceName, ResourceAccess::Type::Read,
                         inferredTag, RenderOwnerTag::Unknown,
                         StableResourceId(resourceName),
                         true});
  m_typedAccesses.push_back({resourceName, inferredTag, PassAccessMode::Read,
                             PipelineStage::Fragment, ResourceUsage::ShaderRead,
                             0, RenderOwnerTag::Unknown});
}

void RenderGraphBuilder::Write(const std::string &resourceName) {
  const RenderResourceTag inferredTag = ToResourceTag(resourceName);
  m_accesses.push_back({resourceName, ResourceAccess::Type::Write,
                         inferredTag, RenderOwnerTag::Unknown,
                         StableResourceId(resourceName),
                         true});
  m_typedAccesses.push_back({resourceName, inferredTag, PassAccessMode::Write,
                             PipelineStage::FramebufferAttachment,
                             ResourceUsage::ColorAttachment, 0,
                             RenderOwnerTag::Unknown});
}

void RenderGraphBuilder::Read(RenderResourceTag resourceTag,
                              RenderOwnerTag ownerTag) {
  const std::string name = ToResourceName(resourceTag);
  m_accesses.push_back({name, ResourceAccess::Type::Read, resourceTag, ownerTag,
                        StableResourceId(name)});
  m_typedAccesses.push_back({name, resourceTag, PassAccessMode::Read,
                             PipelineStage::Fragment, ResourceUsage::ShaderRead,
                             0, ownerTag});
}

void RenderGraphBuilder::Write(RenderResourceTag resourceTag,
                               RenderOwnerTag ownerTag) {
  const std::string name = ToResourceName(resourceTag);
  m_accesses.push_back({name, ResourceAccess::Type::Write, resourceTag, ownerTag,
                        StableResourceId(name)});
  m_typedAccesses.push_back({name, resourceTag, PassAccessMode::Write,
                             PipelineStage::FramebufferAttachment,
                             ResourceUsage::ColorAttachment, 0, ownerTag});
}

void RenderGraphBuilder::Read(RenderResourceTag resourceTag, RenderOwnerTag ownerTag,
                              PipelineStage stage, uint32_t usageFlags) {
  const std::string name = ToResourceName(resourceTag);
  m_accesses.push_back({name, ResourceAccess::Type::Read, resourceTag, ownerTag,
                        StableResourceId(name)});
  m_typedAccesses.push_back({name, resourceTag, PassAccessMode::Read, stage,
                             usageFlags, 0, ownerTag});
}

void RenderGraphBuilder::Write(RenderResourceTag resourceTag, RenderOwnerTag ownerTag,
                               PipelineStage stage, uint32_t usageFlags) {
  const std::string name = ToResourceName(resourceTag);
  m_accesses.push_back({name, ResourceAccess::Type::Write, resourceTag, ownerTag,
                        StableResourceId(name)});
  m_typedAccesses.push_back({name, resourceTag, PassAccessMode::Write, stage,
                             usageFlags, 0, ownerTag});
}

void RenderGraphBuilder::Read(const TypedPassAccess &access) {
  m_typedAccesses.push_back(access);
  m_accesses.push_back({access.resourceName, ResourceAccess::Type::Read,
                        access.resourceTag, access.ownerTag,
                        access.stableResourceId});
}

void RenderGraphBuilder::Write(const TypedPassAccess &access) {
  m_typedAccesses.push_back(access);
  m_accesses.push_back({access.resourceName, ResourceAccess::Type::Write,
                        access.resourceTag, access.ownerTag,
                        access.stableResourceId});
}

void RenderGraphBuilder::DeclareResource(const TypedResourceDescriptor &descriptor) {
  m_declaredDescriptors.push_back(descriptor);
}

RGTextureHandle RenderGraphBuilder::DeclareTexture(const TypedResourceDescriptor &descriptor) {
  TypedResourceDescriptor desc = descriptor;
  desc.kind = ResourceKind::Texture2D;
  if (desc.stableResourceId == 0) {
    desc.stableResourceId = ResolveStableResourceId(0, desc.name.empty() ? ToResourceName(desc.tag) : desc.name);
  }
  m_declaredDescriptors.push_back(desc);
  return RGTextureHandle(desc.tag, desc.stableResourceId, desc.name);
}

RGBufferHandle RenderGraphBuilder::DeclareBuffer(const TypedResourceDescriptor &descriptor) {
  TypedResourceDescriptor desc = descriptor;
  if (desc.kind != ResourceKind::StorageBuffer && desc.kind != ResourceKind::UniformBuffer &&
      desc.kind != ResourceKind::VertexBuffer && desc.kind != ResourceKind::IndexBuffer) {
    desc.kind = ResourceKind::StorageBuffer;
  }
  if (desc.stableResourceId == 0) {
    desc.stableResourceId = ResolveStableResourceId(0, desc.name.empty() ? ToResourceName(desc.tag) : desc.name);
  }
  m_declaredDescriptors.push_back(desc);
  return RGBufferHandle(desc.tag, desc.stableResourceId, desc.name);
}

RGTextureHandle RenderGraphBuilder::CreateTexture(std::string_view name,
                                                  ResourceFormat format,
                                                  const ExtentPolicy &extentPolicy,
                                                  uint32_t usage,
                                                  ResourceLifetime lifetime,
                                                  RenderOwnerTag ownerTag) {
  TypedResourceDescriptor desc;
  desc.name = std::string(name);
  desc.tag = ToResourceTag(name);
  desc.kind = ResourceKind::Texture2D;
  desc.format = format;
  desc.extentPolicy = extentPolicy;
  desc.usageFlags = usage;
  desc.lifetime = lifetime;
  desc.ownerTag = ownerTag;
  desc.stableResourceId = StableResourceId(desc.name);
  return DeclareTexture(desc);
}

RGBufferHandle RenderGraphBuilder::CreateBuffer(std::string_view name,
                                                size_t estimatedSizeBytes,
                                                uint32_t usage,
                                                ResourceLifetime lifetime,
                                                RenderOwnerTag ownerTag) {
  TypedResourceDescriptor desc;
  desc.name = std::string(name);
  desc.tag = ToResourceTag(name);
  desc.kind = ResourceKind::StorageBuffer;
  desc.format = ResourceFormat::Unknown;
  desc.estimatedSizeBytes = estimatedSizeBytes;
  desc.usageFlags = usage;
  desc.lifetime = lifetime;
  desc.ownerTag = ownerTag;
  desc.stableResourceId = StableResourceId(desc.name);
  return DeclareBuffer(desc);
}

RGTextureHandle RenderGraphBuilder::Read(RGTextureHandle handle, RenderOwnerTag ownerTag,
                                         PipelineStage stage, uint32_t usageFlags) {
  const std::string name = !handle.name.empty() ? handle.name : ToResourceName(handle.tag);
  const uint64_t id = handle.id != 0 ? handle.id : StableResourceId(name);
  m_accesses.push_back({name, ResourceAccess::Type::Read, handle.tag, ownerTag, id});
  m_typedAccesses.push_back({name, handle.tag, PassAccessMode::Read, stage, usageFlags, 0, ownerTag, id});
  return handle;
}

RGTextureHandle RenderGraphBuilder::Write(RGTextureHandle handle, RenderOwnerTag ownerTag,
                                          PipelineStage stage, uint32_t usageFlags) {
  const std::string name = !handle.name.empty() ? handle.name : ToResourceName(handle.tag);
  const uint64_t id = handle.id != 0 ? handle.id : StableResourceId(name);
  m_accesses.push_back({name, ResourceAccess::Type::Write, handle.tag, ownerTag, id});
  m_typedAccesses.push_back({name, handle.tag, PassAccessMode::Write, stage, usageFlags, 0, ownerTag, id});
  return handle;
}

RGBufferHandle RenderGraphBuilder::Read(RGBufferHandle handle, RenderOwnerTag ownerTag,
                                        PipelineStage stage, uint32_t usageFlags) {
  const std::string name = !handle.name.empty() ? handle.name : ToResourceName(handle.tag);
  const uint64_t id = handle.id != 0 ? handle.id : StableResourceId(name);
  m_accesses.push_back({name, ResourceAccess::Type::Read, handle.tag, ownerTag, id});
  m_typedAccesses.push_back({name, handle.tag, PassAccessMode::Read, stage, usageFlags, 0, ownerTag, id});
  return handle;
}

RGBufferHandle RenderGraphBuilder::Write(RGBufferHandle handle, RenderOwnerTag ownerTag,
                                         PipelineStage stage, uint32_t usageFlags) {
  const std::string name = !handle.name.empty() ? handle.name : ToResourceName(handle.tag);
  const uint64_t id = handle.id != 0 ? handle.id : StableResourceId(name);
  m_accesses.push_back({name, ResourceAccess::Type::Write, handle.tag, ownerTag, id});
  m_typedAccesses.push_back({name, handle.tag, PassAccessMode::Write, stage, usageFlags, 0, ownerTag, id});
  return handle;
}

void RenderGraphBuilder::ExportResource(RenderResourceTag tag) {
  const std::string name = ToResourceName(tag);
  if (!name.empty()) {
    m_exportedResources.push_back(name);
  }
}

void RenderGraphBuilder::ExportResource(RGTextureHandle handle) {
  const std::string name = !handle.name.empty() ? handle.name : ToResourceName(handle.tag);
  if (!name.empty()) {
    m_exportedResources.push_back(name);
  }
}

void RenderGraphBuilder::ExportResource(RGBufferHandle handle) {
  const std::string name = !handle.name.empty() ? handle.name : ToResourceName(handle.tag);
  if (!name.empty()) {
    m_exportedResources.push_back(name);
  }
}

void RenderGraphBuilder::ExportResource(const std::string &resourceName) {
  if (!resourceName.empty()) {
    m_exportedResources.push_back(resourceName);
  }
}

void RenderGraphBuilder::SetHasSideEffects(bool hasSideEffects) {
  m_hasSideEffects = hasSideEffects;
}

void RenderGraph::AddPass(std::shared_ptr<RenderPass> pass) {
  if (!pass) {
    return;
  }
  Node node = {};
  node.pass = std::move(pass);
  m_nodes.push_back(std::move(node));
  m_isBuilt = false;
  m_hasCachedPlan = false;
}

void RenderGraph::Clear() {
  m_nodes.clear();
  m_validationDiagnostics.clear();
  m_compiledPlan = {};
  m_hasValidationErrors = false;
  m_isBuilt = false;
  m_hasCachedPlan = false;
  m_cachedPlanKey = 0;
}

void RenderGraphBuilder::AddPassLocalBarrier(uint32_t barrierBits) {
  m_passLocalBarriers.push_back(barrierBits);
}

void RenderGraphBuilder::AddPhaseBarrier(PipelineStage sourcePhase,
                                         PipelineStage targetPhase,
                                         uint32_t barrierBits) {
  PhaseBarrierDeclaration declaration = {};
  declaration.sourcePhase = sourcePhase;
  declaration.targetPhase = targetPhase;
  declaration.barrierBits = barrierBits;
  m_phaseBarriers.push_back(declaration);
}

void RenderGraphBuilder::BindBufferBase(RenderResourceTag resourceTag,
                                        uint32_t bindingPoint) {
  ResourceBindingDeclaration declaration = {};
  declaration.resourceTag = resourceTag;
  declaration.kind = ResourceBindingKind::BufferBase;
  declaration.point = bindingPoint;
  m_bindings.push_back(declaration);
}

void RenderGraphBuilder::BindImageUnit(RenderResourceTag resourceTag,
                                       uint32_t unit, uint32_t access,
                                       uint32_t format) {
  ResourceBindingDeclaration declaration = {};
  declaration.resourceTag = resourceTag;
  declaration.kind = ResourceBindingKind::ImageUnit;
  declaration.point = unit;
  declaration.access = access;
  declaration.format = format;
  m_bindings.push_back(declaration);
}

void RenderGraphBuilder::BindTextureUnit(RenderResourceTag resourceTag,
                                         uint32_t textureUnit) {
  ResourceBindingDeclaration declaration = {};
  declaration.resourceTag = resourceTag;
  declaration.kind = ResourceBindingKind::TextureUnit;
  declaration.point = textureUnit;
  m_bindings.push_back(declaration);
}

void RenderGraphBuilder::BindColorAttachment(RenderResourceTag resourceTag,
                                             uint32_t attachmentIndex) {
  ResourceBindingDeclaration declaration = {};
  declaration.resourceTag = resourceTag;
  declaration.kind = ResourceBindingKind::ColorAttachment;
  declaration.point = attachmentIndex;
  m_bindings.push_back(declaration);
}

void RenderGraphBuilder::ImportResource(const ResourceImportInfo &import) {
  m_imports.push_back(import);
}

bool RenderGraph::s_transientAliasingEnabled = false;

void RenderGraph::SetTransientAliasingEnabled(bool enabled) {
  s_transientAliasingEnabled = enabled;
}

bool RenderGraph::IsTransientAliasingEnabled() {
  return s_transientAliasingEnabled;
}

bool RenderGraph::IsPassCulled(size_t passIndex) const {
  if (passIndex < m_compiledPlan.cullingInfo.passCulled.size()) {
    return m_compiledPlan.cullingInfo.passCulled[passIndex];
  }
  return false;
}

bool RenderGraph::IsPassCulled(uint32_t stablePassId) const {
  for (size_t i = 0; i < m_compiledPlan.passes.size(); ++i) {
    if (m_compiledPlan.passes[i].stablePassId == stablePassId) {
      return m_compiledPlan.passes[i].isCulled;
    }
  }
  return false;
}

bool RenderGraph::IsPassCulled(std::string_view passName) const {
  for (size_t i = 0; i < m_nodes.size(); ++i) {
    if (m_nodes[i].pass && m_nodes[i].passName == passName) {
      return IsPassCulled(i);
    }
  }
  return false;
}

void RenderGraph::InvalidateCompilationCache() {
  m_hasCachedPlan = false;
  m_cachedPlanKey = 0;
  // P2 AD-6 (H1): invalidation is a forced-recompile contract; the engine-level
  // (cross-instance) cache must not serve the stale plan for this key either.
  s_compiledPlanCache.clear();
}

void RenderGraph::SetDynamicResolutionScale(float scale) {
  m_dynamicResolutionScale = scale;
}

void RenderGraph::ClearCompilationCache() {
  s_compiledPlanCache.clear();
}

void RenderGraph::OnResize(int width, int height) {
  m_screenWidth = width;
  m_screenHeight = height;
  m_hasCachedPlan = false;
  for (Node &node : m_nodes) {
    if (node.pass) {
      node.pass->OnResize(width, height);
    }
  }
}

RenderOwnerTag RenderGraph::FindLastWriterOwner(
    RenderResourceTag resourceTag) const {
  RenderOwnerTag lastWriterOwner = RenderOwnerTag::Unknown;
  for (const Node &node : m_nodes) {
    for (const ResourceAccess &access : node.accesses) {
      if (access.type == ResourceAccess::Type::Write &&
          access.resourceTag == resourceTag) {
        lastWriterOwner = access.ownerTag;
      }
    }
  }
  return lastWriterOwner;
}

PlanCompilationKey RenderGraph::ComputeCompilationKey() const {
  PlanCompilationKey key = {};
  constexpr uint64_t kOffsetBasis = 1469598103934665603ull;
  constexpr uint64_t kPrime = 1099511628211ull;

  auto hashString = [&](uint64_t &h, std::string_view s) {
    for (char c : s) {
      h ^= static_cast<uint8_t>(c);
      h *= kPrime;
    }
  };

  auto hashU64 = [&](uint64_t &h, uint64_t v) {
    h ^= v;
    h *= kPrime;
  };

  // 1. Topology Hash
  uint64_t topo = kOffsetBasis;
  hashU64(topo, m_nodes.size());
  for (const Node &node : m_nodes) {
    hashU64(topo, node.stablePassId);
    hashU64(topo, static_cast<uint64_t>(node.passType));
    hashU64(topo, node.hasSideEffects ? 1 : 0);
    hashString(topo, node.passName);
    for (const ResourceAccess &access : node.accesses) {
      hashU64(topo, access.stableResourceId);
      hashU64(topo, static_cast<uint64_t>(access.type));
      hashU64(topo, static_cast<uint64_t>(access.resourceTag));
      hashU64(topo, static_cast<uint64_t>(access.ownerTag));
    }
  }
  key.topoHash = topo;

  // 2. Declaration Hash
  uint64_t decl = kOffsetBasis;
  for (const Node &node : m_nodes) {
    for (const TypedPassAccess &access : node.typedAccesses) {
      hashU64(decl, access.stableResourceId);
      hashU64(decl, static_cast<uint64_t>(access.mode));
      hashU64(decl, static_cast<uint64_t>(access.stage));
      hashU64(decl, access.usageFlags);
      hashU64(decl, access.bindingOrAttachmentIndex);
      hashU64(decl, static_cast<uint64_t>(access.ownerTag));
    }
    for (const TypedResourceDescriptor &desc : node.declaredDescriptors) {
      hashU64(decl, desc.stableResourceId);
      hashU64(decl, static_cast<uint64_t>(desc.tag));
      hashU64(decl, static_cast<uint64_t>(desc.kind));
      hashU64(decl, static_cast<uint64_t>(desc.format));
      hashU64(decl, static_cast<uint64_t>(desc.extentPolicy.mode));
      hashU64(decl, desc.extentPolicy.width);
      hashU64(decl, desc.extentPolicy.height);
      // P2 AD-6 (M1): the extent scale (e.g. half-res GI targets) changes the
      // resolved resource dimensions; it must invalidate the compiled plan.
      hashU64(decl, std::bit_cast<uint32_t>(desc.extentPolicy.scale));
      hashU64(decl, desc.mipLevels);
      hashU64(decl, desc.arrayLayers);
      hashU64(decl, desc.sampleCount);
      hashU64(decl, desc.usageFlags);
      hashU64(decl, static_cast<uint64_t>(desc.lifetime));
      hashU64(decl, static_cast<uint64_t>(desc.historyRelation));
      hashU64(decl, static_cast<uint64_t>(desc.ownerTag));
      hashU64(decl, desc.estimatedSizeBytes);
    }
    for (const PhaseBarrierDeclaration &pb : node.phaseBarriers) {
      hashU64(decl, static_cast<uint64_t>(pb.sourcePhase));
      hashU64(decl, static_cast<uint64_t>(pb.targetPhase));
      hashU64(decl, pb.barrierBits);
    }
    for (const ResourceBindingDeclaration &binding : node.bindings) {
      hashU64(decl, static_cast<uint64_t>(binding.resourceTag));
      hashU64(decl, static_cast<uint64_t>(binding.kind));
      hashU64(decl, binding.point);
      hashU64(decl, binding.access);
      hashU64(decl, binding.format);
    }
    for (const ResourceImportInfo &import : node.imports) {
      hashU64(decl, static_cast<uint64_t>(import.resourceTag));
      hashU64(decl, static_cast<uint64_t>(import.kind));
      hashU64(decl, static_cast<uint64_t>(import.format));
      hashU64(decl, static_cast<uint64_t>(import.backingOwner));
      hashU64(decl, import.resizeFollowsScreen ? 1 : 0);
      hashU64(decl, import.resizeFollowsCapacity ? 1 : 0);
      hashU64(decl, import.bindingPoint);
      hashU64(decl, import.imageUnit);
      hashU64(decl, import.imageAccess);
      hashU64(decl, import.imageFormat);
      hashU64(decl, import.colorAttachmentIndex);
    }
    for (const std::string &exp : node.exportedResources) {
      hashString(decl, exp);
    }
  }
  key.declHash = decl;

  // 3. Extent Hash
  uint64_t ext = kOffsetBasis;
  hashU64(ext, static_cast<uint64_t>(m_screenWidth));
  hashU64(ext, static_cast<uint64_t>(m_screenHeight));
  key.extentHash = ext;

  // 4. Quality & Feature Hash
  uint64_t qf = kOffsetBasis;
  const auto &qm = render::core::QualityTierManager::Get();
  hashU64(qf, static_cast<uint64_t>(qm.GetTier()));
  const auto &cfg = qm.GetConfig();

  // P2 AD-6 (M1): every cfg bit that participates in graph compilation must
  // invalidate the cache. Pass-registration toggles (mirrors RenderSystem's
  // per-pass AddPass gating):
  hashU64(qf, cfg.v3Enabled ? 1 : 0);
  hashU64(qf, cfg.clusteredLightingEnabled ? 1 : 0);
  hashU64(qf, cfg.giEnabled ? 1 : 0);
  hashU64(qf, cfg.bloomEnabled ? 1 : 0);
  hashU64(qf, cfg.distortionEnabled ? 1 : 0);
  hashU64(qf, cfg.shadowEnabled ? 1 : 0);
  hashU64(qf, cfg.dynamicLightingEnabled ? 1 : 0);
  hashU64(qf, cfg.volumetricLightEnabled ? 1 : 0);
  hashU64(qf, cfg.heightShadowEnabled ? 1 : 0);
  hashU64(qf, cfg.fluidEnabled ? 1 : 0);
  hashU64(qf, cfg.gpuTextEnabled ? 1 : 0);
  hashU64(qf, cfg.gpuLootEnabled ? 1 : 0);
  // Post-process routing (IsHdrPostProcessRequested) decides the HDR scene
  // pipeline and the post-process pass itself:
  hashU64(qf, cfg.fxaaEnabled ? 1 : 0);
  hashU64(qf, cfg.vignetteEnabled ? 1 : 0);
  hashU64(qf, cfg.colorGradingEnabled ? 1 : 0);
  hashU64(qf, static_cast<uint64_t>(cfg.colorGradingLutSize));
  hashU64(qf, cfg.linearPipeline ? 1 : 0);
  // V4/V5 shader-behavior toggles (conservative: an invalidation is harmless,
  // a missed one would serve a stale plan):
  hashU64(qf, cfg.normalLightingEnabled ? 1 : 0);
  hashU64(qf, cfg.specularEnabled ? 1 : 0);
  hashU64(qf, cfg.clusteredLightingV4Enabled ? 1 : 0);
  hashU64(qf, cfg.selfShadowEnabled ? 1 : 0);
  hashU64(qf, cfg.pomEnabled ? 1 : 0);
  hashU64(qf, cfg.giHalfResolution ? 1 : 0);
  hashU64(qf, cfg.giHolographicEnabled ? 1 : 0);
  // Resource-sizing parameters that change declared descriptors/extents:
  hashU64(qf, static_cast<uint64_t>(cfg.shadowMode));
  hashU64(qf, static_cast<uint64_t>(cfg.shadowResolution));
  hashU64(qf, static_cast<uint64_t>(cfg.shadowAtlasSize));
  hashU64(qf, static_cast<uint64_t>(cfg.bloomMipLevels));
  hashU64(qf, static_cast<uint64_t>(cfg.volumetricSampleCount));
  hashU64(qf, static_cast<uint64_t>(cfg.materialQualityLevel));
  hashU64(qf, static_cast<uint64_t>(cfg.clusterTileSize));
  hashU64(qf, static_cast<uint64_t>(cfg.clusterZSliceCount));
  hashU64(qf, static_cast<uint64_t>(cfg.giCascadeLevels));
  hashU64(qf, static_cast<uint64_t>(cfg.giSdfUpdateInterval));
  hashU64(qf, static_cast<uint64_t>(cfg.vfxSequenceDetail));

  // P2 AD-6 (M1): the live dynamic-resolution scale. Adaptive resolution can
  // change the rendered extent at an unchanged screen size, which must
  // invalidate the compiled plan.
  hashU64(qf, std::bit_cast<uint32_t>(m_dynamicResolutionScale));

  hashU64(qf, s_transientAliasingEnabled ? 1 : 0);
  hashU64(qf, s_validationEnabled ? 1 : 0);
  key.qualityAndFeatureHash = qf;

  return key;
}

void RenderGraph::CollectPassDeclarations() {
  for (size_t index = 0; index < m_nodes.size(); ++index) {
    Node &node = m_nodes[index];
    RenderGraphBuilder builder;
    node.pass->Setup(builder);
    node.accesses = builder.GetAccesses();
    node.typedAccesses = builder.GetTypedAccesses();
    node.declaredDescriptors = builder.GetDeclaredDescriptors();
    node.passLocalBarriers = builder.GetPassLocalBarriers();
    node.phaseBarriers = builder.GetPhaseBarriers();
    node.bindings = builder.GetBindings();
    node.imports = builder.GetImports();
    node.exportedResources = builder.GetExportedResources();
    node.hasSideEffects = builder.HasSideEffects() || (node.pass != nullptr && node.pass->HasSideEffects());
    node.passName = (node.pass != nullptr && node.pass->GetName() != nullptr)
                        ? node.pass->GetName()
                        : "UnnamedPass";
    node.canonicalPassName = CanonicalizePassName(node.passName);
    node.stablePassId = StablePassId(node.canonicalPassName);
    node.passType = (node.pass != nullptr) ? node.pass->Type()
                                           : RenderPassType::Count;
    node.passIndex = index;
  }
}

void RenderGraph::Build() {
  m_validationDiagnostics.clear();
  m_hasValidationErrors = false;

  CollectPassDeclarations();

  const PlanCompilationKey key = ComputeCompilationKey();
  const uint64_t combinedKey = key.GetCombinedHash();

  // Instance-local fast path: the same instance compiled the same key before.
  // m_hasCachedPlan is only set when the plan passed validation, so this hit
  // implicitly satisfies the M3 "already validated" contract.
  if (m_hasCachedPlan && m_cachedPlanKey == combinedKey && !m_compiledPlan.passes.empty()) {
    m_compilationCacheHits++;
    m_isBuilt = true;
    return;
  }

  // P2 AD-6 (H1): engine-level (cross-instance) cache. RenderSystem builds a
  // fresh graph every frame; an identical PlanCompilationKey means the
  // topology, per-node declarations, extent and quality/feature inputs are
  // bit-identical to a previously compiled plan, so the compiled plan is
  // reused without re-running validation or plan construction. M3: a hit is
  // only trusted when the entry was recorded as having passed validation;
  // anything else falls through to a full rebuild.
  auto cachedIt = s_compiledPlanCache.find(combinedKey);
  if (cachedIt != s_compiledPlanCache.end() && cachedIt->second.validated &&
      !cachedIt->second.plan.passes.empty()) {
    m_compiledPlan = cachedIt->second.plan;
    m_cachedPlanKey = combinedKey;
    m_hasCachedPlan = true;
    m_compilationCacheHits++;
    m_isBuilt = true;
    return;
  }

  m_compilationCacheMisses++;

  const bool identityContractFailed = ValidatePassIdentityContract();
  const bool legacyAccessRejected = RejectLegacyStringAccess();

  if (s_validationEnabled) {
    ValidateBuildContracts();
  }

  BuildCompiledPlan();
  m_compiledPlan.compilationKey = key;

  if (!m_hasValidationErrors) {
    m_cachedPlanKey = combinedKey;
    m_hasCachedPlan = true;

    // P2 AD-6 (H1/M3): publish to the engine-level cache so later frames
    // (fresh graph instances with the same key) hit instead of rebuilding.
    // Only validated plans are published (validated=true).
    if (s_compiledPlanCache.size() >= kMaxCachedPlans) {
      s_compiledPlanCache.clear();
    }
    CachedPlanEntry entry;
    entry.plan = m_compiledPlan;
    entry.validated = true;
    s_compiledPlanCache[combinedKey] = std::move(entry);
  } else {
    m_hasCachedPlan = false;
  }

  if (m_hasValidationErrors) {
    for (const ValidationDiagnostic &diagnostic : m_validationDiagnostics) {
      if (diagnostic.severity == ValidationDiagnostic::Severity::Error) {
        LOG_ERROR("RenderGraph[v{}] validation error (pass #{} {} resource={}): {}",
                  RENDERGRAPH_CONTRACT_VERSION,
                  diagnostic.passIndex, diagnostic.passName,
                  diagnostic.resourceName, diagnostic.message);
      } else {
        LOG_WARN("RenderGraph[v{}] validation warning (pass #{} {} resource={}): {}",
                 RENDERGRAPH_CONTRACT_VERSION,
                 diagnostic.passIndex, diagnostic.passName,
                 diagnostic.resourceName, diagnostic.message);
      }
    }

    if (identityContractFailed) {
      std::ostringstream message;
      message << "RenderGraph[v" << RENDERGRAPH_CONTRACT_VERSION
              << "] stable pass identity contract failed with "
              << m_validationDiagnostics.size()
              << " diagnostics; execution is forbidden";
      throw std::logic_error(message.str());
    }

    if (legacyAccessRejected) {
      std::ostringstream message;
      message << "RenderGraph[v" << RENDERGRAPH_CONTRACT_VERSION
              << "] string-based access is denied by default; "
                 "declare typed access via Read/Write(Tag, Owner, Stage, "
                 "Usage); execution is forbidden";
      throw std::logic_error(message.str());
    }

#ifndef NDEBUG
    std::ostringstream message;
    message << "RenderGraph[v" << RENDERGRAPH_CONTRACT_VERSION
            << "] validation failed with "
            << m_validationDiagnostics.size() << " diagnostics";
    throw std::logic_error(message.str());
#endif
  }

  m_isBuilt = true;
}

void RenderGraph::SetValidationEnabled(bool enabled) {
  s_validationEnabled = enabled;
}

bool RenderGraph::IsValidationEnabled() { return s_validationEnabled; }

void RenderGraph::Execute(RenderContext &context) {
  if (!m_isBuilt) {
    Build();
  }

  m_runtimeBindingDiagnostics.clear();

  debug::GPUTimerQueryRing::Get().BeginFrame();

  // Review #9 (exception safety): local RAII guard for the pass-execution
  // window. If a pass throws, the guard's destructor restores every piece of
  // state a partially-executed pass can leave behind: the open GL timer query
  // is closed and its slot marked Discarded (the ring stays clean for the next
  // frame), the profiler CPU sample is closed (cpuRunning does not leak), and
  // context.activeGraph / m_activeNodeIndex are cleared. The success path calls
  // Commit() to disarm the guard and performs the regular EndPass bookkeeping
  // in the loop body.
  class PassExecutionGuard {
  public:
    PassExecutionGuard(RenderContext &context, RenderGraph &graph,
                       uint32_t stablePassId, debug::RenderPassId passType)
        : m_context(context), m_graph(graph), m_stablePassId(stablePassId),
          m_passType(passType), m_committed(false) {}

    ~PassExecutionGuard() {
      if (m_committed) {
        return;
      }
      // Abort path: DiscardPass closes the in-flight GL query (glEndQuery) and
      // marks the slot Discarded so the timer ring stays clean; the profiler
      // sample is closed so cpuRunning does not leak into the next frame.
      debug::GPUTimerQueryRing::Get().DiscardPass(m_stablePassId);
      if (m_context.renderProfiler != nullptr) {
        m_context.renderProfiler->EndPass(m_passType);
      }
      m_graph.m_activeNodeIndex = static_cast<size_t>(-1);
      m_context.activeGraph = nullptr;
    }

    void Commit() { m_committed = true; }

  private:
    RenderContext &m_context;
    RenderGraph &m_graph;
    uint32_t m_stablePassId;
    debug::RenderPassId m_passType;
    bool m_committed;
  };

  for (Node &node : m_nodes) {
    const uint32_t stablePassId = node.stablePassId;

    // Check pass culling (T1.2, T1.7)
    if (node.passIndex < m_compiledPlan.cullingInfo.passCulled.size() &&
        m_compiledPlan.cullingInfo.passCulled[node.passIndex]) {
      // Culled pass: do NOT execute GPU work, but perform CPU bookkeeping
      debug::GPUTimerQueryRing::Get().DiscardPass(stablePassId);
      continue;
    }

    for (uint32_t barrierBits : node.passLocalBarriers) {
      if (barrierBits > 0) {
        NoMoreDay::utils::GPUUtils::MemoryBarrier(barrierBits);
      }
    }

    for (const auto &transition : m_compiledPlan.transitions) {
      if (transition.consumerPassIndex == node.passIndex &&
          transition.barrierBits != 0 &&
          transition.barrierBits != kInvalidBarrierBits) {
        NoMoreDay::utils::GPUUtils::MemoryBarrier(transition.barrierBits);
      }
    }

    NoMoreDay::render::core::ApplyRlglFlushTemplate();
    const NoMoreDay::render::core::ScopedGLState scopedState;
    debug::GPUTimerQueryRing::Get().BeginPass(stablePassId);

    if (context.renderProfiler != nullptr) {
      context.renderProfiler->BeginPass(node.pass->Type());
    }

    context.activeGraph = this;
    m_activeNodeIndex = node.passIndex;

    // RAII guard for the pass-execution window (class defined above): restores
    // timer/profiler/graph state if the pass throws; disarmed via Commit() on
    // the success path.
    PassExecutionGuard passGuard(context, *this, stablePassId,
                                 node.pass->Type());

    try {
      ApplyActivePassBindings(context);
      node.pass->Execute(context);
    } catch (const std::exception &e) {
      // Fail-soft (review #9): a failing pass must not take down the frame or
      // the game. The guard already restored timer/profiler/graph state during
      // cleanup; here we log, flush any pending rlgl batch (stale vertices
      // must not leak into the next frame), and stop executing further passes.
      // The frame degrades to the passes already executed, and the caller's
      // frame-end cleanup (timer ring EndFrame, transient/texture pools,
      // registry AdvanceFrame) still runs normally.
      LOG_ERROR("RenderGraph: pass '{}' threw during Execute: {}", node.passName,
                e.what());
      NoMoreDay::render::core::ApplyRlglFlushTemplate();
      break;
    } catch (...) {
      LOG_ERROR(
          "RenderGraph: pass '{}' threw a non-std exception during Execute",
          node.passName);
      NoMoreDay::render::core::ApplyRlglFlushTemplate();
      break;
    }

    m_activeNodeIndex = static_cast<size_t>(-1);
    context.activeGraph = nullptr;
    NoMoreDay::render::core::ApplyRlglFlushTemplate();

    if (context.renderProfiler != nullptr) {
      context.renderProfiler->EndPass(node.pass->Type());
    }
    debug::GPUTimerQueryRing::Get().EndPass(stablePassId);
    passGuard.Commit();
  }

  debug::GPUTimerQueryRing::Get().EndFrame();
}

bool RenderGraph::EmitActivePassPhaseBarrier(PipelineStage sourcePhase,
                                             PipelineStage targetPhase) {
  if (m_activeNodeIndex >= m_nodes.size()) {
    LOG_ERROR(
        "RenderGraph: EmitPhaseBarrier called outside RenderGraph::Execute "
        "(no active pass)");
    return false;
  }
  const Node &node = m_nodes[m_activeNodeIndex];
  for (const PhaseBarrierDeclaration &declaration : node.phaseBarriers) {
    if (declaration.sourcePhase == sourcePhase &&
        declaration.targetPhase == targetPhase) {
      if (declaration.barrierBits == 0u) {
        LOG_ERROR(
            "RenderGraph: pass '{}' declared phase barrier {} -> {} with zero "
            "barrier bits",
            node.passName, ToPipelineStageName(sourcePhase),
            ToPipelineStageName(targetPhase));
        return false;
      }
      NoMoreDay::utils::GPUUtils::MemoryBarrier(declaration.barrierBits);
      return true;
    }
  }
  LOG_ERROR(
      "RenderGraph: pass '{}' emitted phase barrier {} -> {} without a "
      "declaration",
      node.passName, ToPipelineStageName(sourcePhase),
      ToPipelineStageName(targetPhase));
  return false;
}

bool RenderGraph::ValidatePassIdentityContract() {
  bool failed = false;
  std::unordered_map<std::string, size_t> canonicalToPassIndex;
  std::unordered_map<uint32_t, std::string> idToCanonicalName;

  for (Node &node : m_nodes) {
    node.canonicalPassName = CanonicalizePassName(node.passName);
    if (node.canonicalPassName.empty()) {
      AddValidationDiagnostic(
          ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
          "(identity)",
          "pass canonical name is empty after canonicalization");
      failed = true;
      continue;
    }

    const auto canonicalIt =
        canonicalToPassIndex.find(node.canonicalPassName);
    if (canonicalIt != canonicalToPassIndex.end()) {
      AddValidationDiagnostic(
          ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
          "(identity)",
          "duplicate canonical pass name (also declared by pass #" +
              std::to_string(canonicalIt->second) + ")");
      failed = true;
    } else {
      canonicalToPassIndex.emplace(node.canonicalPassName, node.passIndex);
    }

    // Name/type agreement: the kRenderPassNames table is the single source of
    // truth. A pass whose RenderPassType maps to a table entry must carry
    // that entry's canonical name (otherwise its stable id would silently
    // diverge from the profiler/gate identity derived from the table). Passes
    // whose names match no table entry at all (e.g. VFXEmissionSnapshotPass)
    // stay exempt, preserving legacy non-canonical behavior.
    if (node.pass != nullptr) {
      const RenderPassType passType = node.pass->Type();
      if (static_cast<size_t>(passType) < kRenderPassNames.size()) {
        const std::string tableCanonical = CanonicalizePassName(
            kRenderPassNames[static_cast<size_t>(passType)].full);
        if (tableCanonical != node.canonicalPassName) {
          bool nameMatchesOtherEntry = false;
          for (size_t i = 0; i < kRenderPassNames.size(); ++i) {
            if (CanonicalizePassName(kRenderPassNames[i].full) ==
                node.canonicalPassName) {
              nameMatchesOtherEntry = true;
              break;
            }
          }
          if (nameMatchesOtherEntry) {
            AddValidationDiagnostic(
                ValidationDiagnostic::Severity::Error, node.passIndex,
                node.passName, "(identity)",
                "pass name canonicalizes to a different pass's table name "
                "than its RenderPassType");
            failed = true;
          }
        }
      }
    }

    node.stablePassId = StablePassId(node.canonicalPassName);

    if (node.stablePassId == kInvalidStablePassId) {
      AddValidationDiagnostic(
          ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
          "(identity)",
          "stable pass id collides with reserved id 0 (invalid/unassigned)");
      failed = true;
    } else if (node.stablePassId == kFrameLevelStablePassId) {
      AddValidationDiagnostic(
          ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
          "(identity)",
          "stable pass id collides with reserved frame-level id 0xFFFFFFFF");
      failed = true;
    }

    const auto idIt = idToCanonicalName.find(node.stablePassId);
    if (idIt != idToCanonicalName.end() &&
        idIt->second != node.canonicalPassName) {
      AddValidationDiagnostic(
          ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
          "(identity)",
          "stable pass id hash collision with canonical name '" +
              idIt->second + "'");
      failed = true;
    } else if (idIt == idToCanonicalName.end()) {
      idToCanonicalName.emplace(node.stablePassId, node.canonicalPassName);
    }
  }

  return failed;
}

bool RenderGraph::RejectLegacyStringAccess() {
  bool rejected = false;
  for (const Node &node : m_nodes) {
    for (const ResourceAccess &access : node.accesses) {
      if (!access.isStringBasedAccess) {
        continue;
      }
      AddValidationDiagnostic(
          ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
          access.resourceName,
          "string-based access is denied by default; declare typed "
          "access via Read/Write(Tag, Owner, Stage, Usage)");
      rejected = true;
    }
  }
  return rejected;
}

void RenderGraph::ValidateBuildContracts() {
  int lastStage = -1;
  bool seenComposite = false;
  std::unordered_set<int> seenSingularStages;
  for (const Node &node : m_nodes) {
    const auto stage = ResolvePassContractStage(node.passType, node.passName);
    if (!stage.has_value()) {
      continue;
    }

    const int stageValue = static_cast<int>(*stage);
    if (stageValue < lastStage) {
      AddValidationDiagnostic(
          ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
          "(order)",
          "pass order violation against locked contract sequence");
    }
    lastStage = std::max(lastStage, stageValue);

    const bool isShadowStage = (*stage == PassContractStage::Shadow);
    if (!isShadowStage && !seenSingularStages.insert(stageValue).second) {
      AddValidationDiagnostic(ValidationDiagnostic::Severity::Error, node.passIndex,
                              node.passName, "(order)",
                              "duplicate pass stage in locked contract sequence");
    }

    if (*stage == PassContractStage::Composite) {
      seenComposite = true;
      if (node.passIndex + 1 != m_nodes.size()) {
        AddValidationDiagnostic(
            ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
            "BackBuffer", "Composite pass must be the final stage");
      }
    } else if (seenComposite) {
      AddValidationDiagnostic(ValidationDiagnostic::Severity::Error, node.passIndex,
                              node.passName, "(order)",
                              "no pass is allowed after Composite stage");
    }
  }

  std::unordered_map<std::string, size_t> firstWriterPass;

  for (const Node &node : m_nodes) {
    std::unordered_set<std::string> localWrites;
    for (const ResourceAccess &access : node.accesses) {
      if (access.resourceName.empty()) {
        AddValidationDiagnostic(
            ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
            "(empty)", "resource access uses an empty resource name");
        continue;
      }

      const RenderResourceTag nameInferredTag = ToResourceTag(access.resourceName);
      if (IsKnownResource(nameInferredTag) &&
          access.resourceTag != nameInferredTag) {
        AddValidationDiagnostic(
            ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
            access.resourceName,
            "known resource usage has undeclared or mismatched resource tag");
      }

      if (IsKnownResource(access.resourceTag) &&
          access.resourceName != ToResourceName(access.resourceTag)) {
        AddValidationDiagnostic(
            ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
            access.resourceName,
            "resource tag does not match canonical resource name");
      }

      if (access.type == ResourceAccess::Type::Read) {
        if (IsKnownResource(access.resourceTag) &&
            access.ownerTag == RenderOwnerTag::Unknown) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Error, node.passIndex,
              node.passName, access.resourceName,
              "known resource read must declare an owner tag");
        }
        if (firstWriterPass.find(access.resourceName) == firstWriterPass.end()) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Error, node.passIndex,
              node.passName, access.resourceName,
              "read-before-write detected");
        }
        continue;
      }

      if (!localWrites.insert(access.resourceName).second) {
        AddValidationDiagnostic(
            ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
            access.resourceName, "duplicate write declaration in the same pass");
      }

      if (access.resourceTag != RenderResourceTag::Custom) {
        if (access.ownerTag == RenderOwnerTag::Unknown) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Error, node.passIndex,
              node.passName, access.resourceName,
              "known resource write must declare an owner tag");
        }

        if (!IsWriterAllowedForResource(access.resourceTag, access.ownerTag)) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Error, node.passIndex,
              node.passName, access.resourceName,
              std::string("owner '") + ToOwnerName(access.ownerTag) +
                  "' is not allowed to write this resource");
        }

        const auto writerIt = firstWriterPass.find(access.resourceName);
        if (writerIt == firstWriterPass.end()) {
          if (!IsFirstWriterValid(access.resourceTag, access.ownerTag)) {
            const RenderOwnerTag requiredOwner =
                ExpectedFirstWriter(access.resourceTag);
            AddValidationDiagnostic(
                ValidationDiagnostic::Severity::Error, node.passIndex,
                node.passName, access.resourceName,
                std::string("first writer must be '") +
                    ToOwnerName(requiredOwner) +
                    "'");
          }
        } else if (!IsAdditionalWriterValid(access.resourceTag, access.ownerTag)) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Error, node.passIndex,
              node.passName, access.resourceName,
              "resource does not allow multiple write owners");
        }
      }

      firstWriterPass.try_emplace(access.resourceName, node.passIndex);
    }
  }

  // --- Phase-aware barrier declaration validation (B2 contract) -------------
  // A (source,target) phase pair must be declared at most once per pass and
  // must carry non-zero barrier bits; otherwise EmitPhaseBarrier would resolve
  // nothing or emit nothing at the execution point.
  for (const Node &node : m_nodes) {
    std::unordered_set<uint32_t> declaredPhaseKeys;
    for (const PhaseBarrierDeclaration &barrier : node.phaseBarriers) {
      const uint32_t phaseKey =
          (static_cast<uint32_t>(barrier.sourcePhase) << 16) |
          static_cast<uint32_t>(barrier.targetPhase);
      if (!declaredPhaseKeys.insert(phaseKey).second) {
        AddValidationDiagnostic(
            ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
            "(phase-barrier)",
            "duplicate phase barrier declaration for the same phase pair");
      }
      if (barrier.barrierBits == 0u) {
        AddValidationDiagnostic(
            ValidationDiagnostic::Severity::Warning, node.passIndex, node.passName,
            "(phase-barrier)",
            "phase barrier declared with zero barrier bits; nothing will be emitted");
      }
    }
  }

  // --- Resource binding observation validation ------------------------------
  // Bindings are observer-only declarations; validation checks internal
  // consistency without issuing any GL call or changing resource ownership.
  for (const Node &node : m_nodes) {
    std::unordered_set<std::string> declaredBindingKeys;
    std::unordered_map<RenderResourceTag, ResourceKind> descriptorKinds;
    for (const TypedResourceDescriptor &descriptor : node.declaredDescriptors) {
      descriptorKinds[descriptor.tag] = descriptor.kind;
    }
    for (const ResourceBindingDeclaration &binding : node.bindings) {
      const std::string bindingKey =
          std::to_string(static_cast<int>(binding.kind)) + ":" +
          std::to_string(static_cast<int>(binding.resourceTag)) + ":" +
          std::to_string(binding.point);
      if (!declaredBindingKeys.insert(bindingKey).second) {
        AddValidationDiagnostic(
            ValidationDiagnostic::Severity::Warning, node.passIndex, node.passName,
            ToResourceName(binding.resourceTag),
            "duplicate resource binding declaration (tag/kind/point)");
      }
      if (binding.resourceTag == RenderResourceTag::Custom) {
        AddValidationDiagnostic(
            ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
            "(binding)", "binding declared for unknown/custom resource tag");
        continue;
      }
      const auto descriptorIt = descriptorKinds.find(binding.resourceTag);
      if (descriptorIt == descriptorKinds.end()) {
        AddValidationDiagnostic(
            ValidationDiagnostic::Severity::Warning, node.passIndex, node.passName,
            ToResourceName(binding.resourceTag),
            "binding declared for a resource with no descriptor declared in this pass");
        continue;
      }
      if (binding.kind == ResourceBindingKind::BufferBase) {
        const bool isBufferKind =
            descriptorIt->second == ResourceKind::StorageBuffer ||
            descriptorIt->second == ResourceKind::UniformBuffer;
        if (!isBufferKind) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Warning, node.passIndex, node.passName,
              ToResourceName(binding.resourceTag),
              "BufferBase binding declared for a non-buffer resource kind");
        }
      } else if (binding.kind == ResourceBindingKind::ImageUnit) {
        const bool isImageKind =
            descriptorIt->second == ResourceKind::Texture2D ||
            descriptorIt->second == ResourceKind::Texture2DArray ||
            descriptorIt->second == ResourceKind::Framebuffer;
        if (!isImageKind) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Warning, node.passIndex, node.passName,
              ToResourceName(binding.resourceTag),
              "ImageUnit binding declared for a non-texture resource kind");
        }
      }
    }
  }

  // --- External backing import validation (blocker contract) ----------------
  // Imports are observer-only declarations of GL backing that is created and
  // owned outside the graph. Validation checks internal consistency only; the
  // graph never allocates, resizes, frees, or GL-binds imported backing.
  std::map<RenderResourceTag, ResourceImportInfo> firstImportByTag;
  for (const Node &node : m_nodes) {
    std::unordered_set<RenderResourceTag> localImportTags;
    std::unordered_map<RenderResourceTag, TypedResourceDescriptor> descriptorsByTag;
    for (const TypedResourceDescriptor &descriptor : node.declaredDescriptors) {
      descriptorsByTag[descriptor.tag] = descriptor;
    }
    for (const ResourceImportInfo &import : node.imports) {
      if (import.resourceTag == RenderResourceTag::Custom) {
        AddValidationDiagnostic(
            ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
            "(import)", "import declared for unknown/custom resource tag");
        continue;
      }
      if (!localImportTags.insert(import.resourceTag).second) {
        AddValidationDiagnostic(
            ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
            ToResourceName(import.resourceTag),
            "duplicate import declaration for the same resource in this pass");
      }

      const auto descriptorIt = descriptorsByTag.find(import.resourceTag);
      if (descriptorIt != descriptorsByTag.end()) {
        const TypedResourceDescriptor &descriptor = descriptorIt->second;
        if (descriptor.lifetime == ResourceLifetime::Transient) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
              ToResourceName(import.resourceTag),
              "imported resource must not be declared Transient (the graph would allocate it)");
        }
        if (descriptor.kind != import.kind) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
              ToResourceName(import.resourceTag),
              "import kind does not match the descriptor declared in this pass");
        }
        if (descriptor.format != ResourceFormat::Unknown &&
            import.format != ResourceFormat::Unknown &&
            descriptor.format != import.format) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
              ToResourceName(import.resourceTag),
              "import format does not match the descriptor declared in this pass");
        }
      }

      const auto firstIt = firstImportByTag.find(import.resourceTag);
      if (firstIt == firstImportByTag.end()) {
        firstImportByTag[import.resourceTag] = import;
      } else {
        const ResourceImportInfo &first = firstIt->second;
        const bool conflictingKind = first.kind != import.kind;
        const bool conflictingFormat =
            first.format != ResourceFormat::Unknown &&
            import.format != ResourceFormat::Unknown &&
            first.format != import.format;
        if (conflictingKind || conflictingFormat) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Error, node.passIndex, node.passName,
              ToResourceName(import.resourceTag),
              "import conflicts with a declaration made by another pass (kind/format)");
        }
      }

      if (import.backingOwner == RenderOwnerTag::Unknown) {
        AddValidationDiagnostic(
            ValidationDiagnostic::Severity::Warning, node.passIndex, node.passName,
            ToResourceName(import.resourceTag),
            "import does not name the external backing owner");
      }

      // Binding-surface consistency with the observed manual binds of the same
      // pass (mismatch => stale metadata, warn; manual binds stay authoritative
      // until the graph-driven replacement lands).
      for (const ResourceBindingDeclaration &binding : node.bindings) {
        if (binding.resourceTag != import.resourceTag) {
          continue;
        }
        if (binding.kind == ResourceBindingKind::BufferBase &&
            import.bindingPoint != 0 && binding.point != import.bindingPoint) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Warning, node.passIndex, node.passName,
              ToResourceName(import.resourceTag),
              "import binding point does not match the BindBufferBase observation");
        }
        if (binding.kind == ResourceBindingKind::ImageUnit && import.imageUnit != 0 &&
            binding.point != import.imageUnit) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Warning, node.passIndex, node.passName,
              ToResourceName(import.resourceTag),
              "import image unit does not match the BindImageUnit observation");
        }
        if (binding.kind == ResourceBindingKind::ImageUnit && import.imageAccess != 0 &&
            binding.access != import.imageAccess) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Warning, node.passIndex, node.passName,
              ToResourceName(import.resourceTag),
              "import image access does not match the BindImageUnit observation");
        }
        if (binding.kind == ResourceBindingKind::ImageUnit && import.imageFormat != 0 &&
            binding.format != import.imageFormat) {
          AddValidationDiagnostic(
              ValidationDiagnostic::Severity::Warning, node.passIndex, node.passName,
              ToResourceName(import.resourceTag),
              "import image format does not match the BindImageUnit observation");
        }
      }
    }
  }
}

void RenderGraph::BuildCompiledPlan() {
  m_compiledPlan = {};
  m_compiledPlan.isValid = !m_hasValidationErrors;

  for (const Node &node : m_nodes) {
    m_compiledPlan.passOrder.push_back(node.passName);
    CompiledPassState passState = {};
    passState.stablePassId = node.stablePassId;
    passState.passName = node.passName;
    passState.passIndex = node.passIndex;
    m_compiledPlan.passes.push_back(std::move(passState));
  }

  std::map<uint64_t, CompiledResourceState> resourceMap;
  std::unordered_map<std::string, uint64_t> descriptorIdsByName;

  for (const Node &node : m_nodes) {
    for (const auto &desc : node.declaredDescriptors) {
      const uint64_t resourceId =
          ResolveStableResourceId(desc.stableResourceId, desc.name);
      descriptorIdsByName[desc.name] = resourceId;
      auto &res = resourceMap[resourceId];
      res.stableResourceId = resourceId;
      res.resourceName = desc.name;
      res.tag = desc.tag;
      res.descriptor = desc;
    }
  }

  for (const Node &node : m_nodes) {
    for (const auto &access : node.accesses) {
      if (access.resourceName.empty()) continue;
      const auto descriptorIt = descriptorIdsByName.find(access.resourceName);
      const uint64_t resourceId = access.stableResourceId != 0
                                      ? access.stableResourceId
                                      : (descriptorIt != descriptorIdsByName.end()
                                             ? descriptorIt->second
                                             : StableResourceId(access.resourceName));
      auto &res = resourceMap[resourceId];
      res.stableResourceId = resourceId;
      if (res.resourceName.empty()) {
        res.resourceName = access.resourceName;
      }
      if (res.tag == RenderResourceTag::Custom) {
        res.tag = access.resourceTag;
      }
      if (access.resourceTag == RenderResourceTag::FinalOutputColor) {
        res.isExternal = true;
      }

      if (access.type == ResourceAccess::Type::Write) {
        if (res.writerPassIndices.empty()) {
          res.firstProducerPassIndex = node.passIndex;
          res.hasProducer = true;
        }
        res.writerPassIndices.push_back(node.passIndex);
      } else {
        res.readerPassIndices.push_back(node.passIndex);
        res.lastConsumerPassIndex = node.passIndex;
      }
    }
  }

  struct PreviousAccess {
    size_t passIndex = 0;
    PipelineStage stage = PipelineStage::Fragment;
    PassAccessMode mode = PassAccessMode::Read;
    ResourceKind kind = ResourceKind::Texture2D;
  };
  std::map<uint64_t, PreviousAccess> previousAccesses;
  for (const Node &node : m_nodes) {
    for (const TypedPassAccess &access : node.typedAccesses) {
      if (access.resourceName.empty()) continue;
      const auto descriptorIt = descriptorIdsByName.find(access.resourceName);
      const uint64_t resourceId = access.stableResourceId != 0
                                      ? access.stableResourceId
                                      : (descriptorIt != descriptorIdsByName.end()
                                             ? descriptorIt->second
                                             : StableResourceId(access.resourceName));
      const auto resourceIt = resourceMap.find(resourceId);
      if (resourceIt == resourceMap.end()) continue;

      const auto previousIt = previousAccesses.find(resourceId);
      if (previousIt != previousAccesses.end() &&
          (previousIt->second.stage != access.stage ||
           previousIt->second.mode != access.mode)) {
        const PreviousAccess &previous = previousIt->second;
        RenderTransition transition = {};
        transition.stableResourceId = resourceId;
        transition.resourceName = access.resourceName;
        transition.previousPassIndex = previous.passIndex;
        transition.consumerPassIndex = node.passIndex;
        transition.previousStage = previous.stage;
        transition.nextStage = access.stage;
        transition.previousMode = previous.mode;
        transition.nextMode = access.mode;
        transition.resourceKind = previous.kind;
        transition.barrierBits = MapGlBarrierBits(
            previous.stage, previous.mode, access.stage, access.mode,
            previous.kind);
        m_compiledPlan.transitions.push_back(std::move(transition));
      }

      previousAccesses[resourceId] = {
          node.passIndex, access.stage, access.mode, resourceIt->second.descriptor.kind};
    }
  }

  for (const Node &node : m_nodes) {
    for (const PhaseBarrierDeclaration &declaration : node.phaseBarriers) {
      CompiledPhaseBarrier compiled = {};
      compiled.passIndex = node.passIndex;
      compiled.passName = node.passName;
      compiled.sourcePhase = declaration.sourcePhase;
      compiled.targetPhase = declaration.targetPhase;
      compiled.barrierBits = declaration.barrierBits;
      m_compiledPlan.phaseBarriers.push_back(std::move(compiled));
    }
  }

  for (const Node &node : m_nodes) {
    for (const ResourceBindingDeclaration &binding : node.bindings) {
      CompiledResourceBinding compiled = {};
      compiled.passIndex = node.passIndex;
      compiled.passName = node.passName;
      compiled.resourceName = ToResourceName(binding.resourceTag);
      compiled.kind = binding.kind;
      compiled.point = binding.point;
      compiled.access = binding.access;
      compiled.format = binding.format;
      m_compiledPlan.bindings.push_back(std::move(compiled));
    }
  }

  // Export external-backing import declarations (blocker contract). These are
  // observer-only metadata; the graph never allocates, resizes, frees, or
  // GL-binds imported backing.
  for (const Node &node : m_nodes) {
    for (const ResourceImportInfo &import : node.imports) {
      CompiledResourceImport compiled = {};
      compiled.passIndex = node.passIndex;
      compiled.passName = node.passName;
      compiled.resourceName = ToResourceName(import.resourceTag);
      compiled.resourceTag = import.resourceTag;
      compiled.kind = import.kind;
      compiled.format = import.format;
      compiled.backingOwner = import.backingOwner;
      compiled.resizeFollowsScreen = import.resizeFollowsScreen;
      compiled.resizeFollowsCapacity = import.resizeFollowsCapacity;
      compiled.bindingPoint = import.bindingPoint;
      compiled.imageUnit = import.imageUnit;
      compiled.imageAccess = import.imageAccess;
      compiled.imageFormat = import.imageFormat;
      compiled.colorAttachmentIndex = import.colorAttachmentIndex;
      m_compiledPlan.imports.push_back(std::move(compiled));
    }
  }

  for (const Node &node : m_nodes) {
    for (const auto &access : node.accesses) {
      if (access.type == ResourceAccess::Type::Read && !access.resourceName.empty()) {
        const auto descriptorIt = descriptorIdsByName.find(access.resourceName);
        const uint64_t resourceId = access.stableResourceId != 0
                                        ? access.stableResourceId
                                        : (descriptorIt != descriptorIdsByName.end()
                                               ? descriptorIt->second
                                               : StableResourceId(access.resourceName));
        const auto resourceIt = resourceMap.find(resourceId);
        if (resourceIt == resourceMap.end()) {
          continue;
        }
        const auto &res = resourceIt->second;
        for (const size_t writerIndex : res.writerPassIndices) {
          if (writerIndex < node.passIndex) {
            ProducerConsumerEdge edge = {};
            edge.producerPassIndex = writerIndex;
            edge.producerPassName = m_nodes[writerIndex].passName;
            edge.consumerPassIndex = node.passIndex;
            edge.consumerPassName = node.passName;
            edge.resourceName = access.resourceName;
            edge.resourceTag = access.resourceTag;
            m_compiledPlan.edges.push_back(edge);
          }
        }
      }
    }
  }

  const size_t passCount = m_nodes.size();
  std::vector<std::vector<size_t>> adj(passCount);
  std::vector<size_t> inDegree(passCount, 0);

  for (const auto &edge : m_compiledPlan.edges) {
    if (edge.producerPassIndex < passCount && edge.consumerPassIndex < passCount) {
      adj[edge.producerPassIndex].push_back(edge.consumerPassIndex);
      inDegree[edge.consumerPassIndex]++;
    }
  }

  std::queue<size_t> q;
  for (size_t i = 0; i < passCount; ++i) {
    if (inDegree[i] == 0) {
      q.push(i);
    }
  }

  size_t visitedCount = 0;
  while (!q.empty()) {
    const size_t u = q.front();
    q.pop();
    visitedCount++;
    for (const size_t v : adj[u]) {
      inDegree[v]--;
      if (inDegree[v] == 0) {
        q.push(v);
      }
    }
  }

  if (visitedCount < passCount) {
    m_compiledPlan.isValid = false;
    AddValidationDiagnostic(
        ValidationDiagnostic::Severity::Error, 0, "RenderGraph", "(DAG)",
        "cycle detected in render graph dependency edges");
  }

  PerformPassCulling(resourceMap);
  ComputeTransientAliasing(resourceMap);

  for (const auto &[resourceId, state] : resourceMap) {
    m_compiledPlan.resources.push_back(state);
  }

  m_compiledPlan.diagnostics = m_validationDiagnostics;
}

void RenderGraph::PerformPassCulling(std::map<uint64_t, CompiledResourceState> &resourceMap) {
  const size_t passCount = m_nodes.size();
  m_compiledPlan.cullingInfo = {};
  m_compiledPlan.cullingInfo.totalPassCount = passCount;
  m_compiledPlan.cullingInfo.passCulled.assign(passCount, false);

  if (passCount == 0) {
    return;
  }

  // 1. Identify root resources (external/exported/FinalOutputColor)
  std::unordered_set<uint64_t> rootResourceIds;
  std::unordered_set<std::string> exportedNames;
  for (const Node &node : m_nodes) {
    for (const std::string &name : node.exportedResources) {
      exportedNames.insert(name);
    }
  }

  for (const auto &[id, res] : resourceMap) {
    if (res.tag == RenderResourceTag::FinalOutputColor ||
        res.isExternal ||
        res.descriptor.lifetime == ResourceLifetime::External ||
        exportedNames.count(res.resourceName) > 0) {
      rootResourceIds.insert(id);
    }
  }

  // 2. Identify root passes (side effects, writes to root resources, or writes to imported backings)
  std::vector<bool> reachable(passCount, false);
  std::queue<size_t> q;

  for (size_t i = 0; i < passCount; ++i) {
    const Node &node = m_nodes[i];
    bool isRoot = false;

    if (node.hasSideEffects || (node.pass != nullptr && node.pass->HasSideEffects())) {
      isRoot = true;
    }

    // Check if pass writes to any root resource
    for (const ResourceAccess &access : node.accesses) {
      if (access.type == ResourceAccess::Type::Write) {
        if (access.resourceTag == RenderResourceTag::FinalOutputColor) {
          isRoot = true;
          break;
        }
        const uint64_t resId = access.stableResourceId != 0
                                   ? access.stableResourceId
                                   : StableResourceId(access.resourceName);
        if (rootResourceIds.count(resId) > 0) {
          isRoot = true;
          break;
        }
      }
    }

    // Check if pass writes to imported resources
    if (!isRoot && !node.imports.empty()) {
      for (const ResourceAccess &access : node.accesses) {
        if (access.type == ResourceAccess::Type::Write) {
          for (const ResourceImportInfo &import : node.imports) {
            if (access.resourceTag == import.resourceTag) {
              isRoot = true;
              break;
            }
          }
          if (isRoot) break;
        }
      }
    }

    if (isRoot) {
      reachable[i] = true;
      q.push(i);
    }
  }

  // 3. Reverse BFS traversal from root passes
  while (!q.empty()) {
    const size_t u = q.front();
    q.pop();

    const Node &node = m_nodes[u];
    for (const ResourceAccess &access : node.accesses) {
      if (access.type == ResourceAccess::Type::Read && !access.resourceName.empty()) {
        const uint64_t resId = access.stableResourceId != 0
                                   ? access.stableResourceId
                                   : StableResourceId(access.resourceName);
        const auto it = resourceMap.find(resId);
        if (it != resourceMap.end()) {
          const auto &res = it->second;
          for (const size_t writerIndex : res.writerPassIndices) {
            if (writerIndex < passCount && !reachable[writerIndex]) {
              reachable[writerIndex] = true;
              q.push(writerIndex);
            }
          }
        }
      }
    }
  }

  // 4. Update culling info and CompiledPassState
  size_t culledCount = 0;
  for (size_t i = 0; i < passCount; ++i) {
    const bool isCulled = !reachable[i];
    m_compiledPlan.cullingInfo.passCulled[i] = isCulled;
    if (i < m_compiledPlan.passes.size()) {
      m_compiledPlan.passes[i].isCulled = isCulled;
    }
    if (isCulled) {
      culledCount++;
      m_compiledPlan.cullingInfo.culledStablePassIds.push_back(m_nodes[i].stablePassId);
      m_compiledPlan.cullingInfo.culledPassNames.push_back(m_nodes[i].passName);
    }
  }

  m_compiledPlan.cullingInfo.culledPassCount = culledCount;
  m_compiledPlan.cullingInfo.cullingRate =
      passCount > 0 ? static_cast<float>(culledCount) / static_cast<float>(passCount) : 0.0f;
}

void RenderGraph::ComputeTransientAliasing(std::map<uint64_t, CompiledResourceState> &resourceMap) {
  m_compiledPlan.aliasingTable = {};
  m_compiledPlan.aliasingTable.enabled = s_transientAliasingEnabled;

  const size_t passCount = m_nodes.size();
  const auto &culling = m_compiledPlan.cullingInfo;

  // 1. Extract lifetime intervals [firstUse, lastUse] for all transient resources among non-culled passes
  std::vector<ResourceLifetimeInterval> intervals;
  for (auto &[resId, res] : resourceMap) {
    // Exclude non-transient, persistent history, imported, and external/exported resources.
    //
    // M6: depth/stencil targets are excluded from aliasing candidates. The
    // pool path (TransientResourcePool -> GPUTexturePool) only backs color
    // targets (withDepth=false), and there is no depth-stencil barrier/reuse
    // design; aliasing a depth target (e.g. SceneDepth) with a color target of
    // the same size would corrupt the depth buffer of a later pass sharing the
    // backing store. Exclusion is keyed on the descriptor (format + usage
    // flag + tag) so any depth-stencil transient resource is covered.
    //
    // H3: MSAA resources (sampleCount > 1) are excluded. GPUTexturePool's
    // TexturePoolKey carries no sampleCount (it only supports single-sample
    // backing), the size estimate does not multiply in sampleCount, and no
    // multisample reuse contract exists. Reusing a multisample target as a
    // single-sample target (or vice versa) would render garbage. If a future
    // pool gains MSAA support, remove this exclusion and multiply the size
    // estimate by sampleCount (already done defensively below).
    if (res.descriptor.lifetime != ResourceLifetime::Transient ||
        res.tag == RenderResourceTag::FinalOutputColor ||
        res.tag == RenderResourceTag::GIHistoryColor ||
        res.tag == RenderResourceTag::DistanceField ||
        res.tag == RenderResourceTag::ShadowAtlas ||
        res.tag == RenderResourceTag::ShadowDistanceField ||
        res.tag == RenderResourceTag::ShadowMask ||
        res.tag == RenderResourceTag::SceneDepth ||
        res.descriptor.format == ResourceFormat::Depth24Stencil8 ||
        res.descriptor.format == ResourceFormat::Depth32F ||
        (res.descriptor.usageFlags & ResourceUsage::DepthAttachment) != 0u ||
        res.descriptor.sampleCount > 1 ||
        res.isExternal) {
      continue;
    }

    size_t firstUse = static_cast<size_t>(-1);
    size_t lastUse = 0;
    bool usedInActivePass = false;

    // Check writers
    for (size_t w : res.writerPassIndices) {
      if (w < passCount && (!culling.passCulled.empty() && !culling.passCulled[w])) {
        usedInActivePass = true;
        firstUse = std::min(firstUse, w);
        lastUse = std::max(lastUse, w);
      }
    }
    // Check readers
    for (size_t r : res.readerPassIndices) {
      if (r < passCount && (!culling.passCulled.empty() && !culling.passCulled[r])) {
        usedInActivePass = true;
        firstUse = std::min(firstUse, r);
        lastUse = std::max(lastUse, r);
      }
    }

    if (!usedInActivePass) {
      continue;
    }

    ResourceLifetimeInterval interval = {};
    interval.stableResourceId = res.stableResourceId;
    interval.resourceName = res.resourceName;
    interval.tag = res.tag;
    interval.firstUsePassIndex = firstUse;
    interval.lastUsePassIndex = lastUse;
    interval.isTransient = true;
    interval.descriptor = res.descriptor;

    // Estimate size
    size_t estimatedSize = res.descriptor.estimatedSizeBytes;
    if (estimatedSize == 0 && res.descriptor.kind == ResourceKind::Texture2D) {
      uint32_t width = res.descriptor.extentPolicy.width > 0
                           ? res.descriptor.extentPolicy.width
                           : static_cast<uint32_t>(m_screenWidth);
      uint32_t height = res.descriptor.extentPolicy.height > 0
                            ? res.descriptor.extentPolicy.height
                            : static_cast<uint32_t>(m_screenHeight);
      if (res.descriptor.extentPolicy.scale > 0.0f && res.descriptor.extentPolicy.scale != 1.0f) {
        width = static_cast<uint32_t>(width * res.descriptor.extentPolicy.scale);
        height = static_cast<uint32_t>(height * res.descriptor.extentPolicy.scale);
      }
      size_t bpp = 4;
      switch (res.descriptor.format) {
      case ResourceFormat::R8: bpp = 1; break;
      case ResourceFormat::R16F: bpp = 2; break;
      case ResourceFormat::RG16F: bpp = 4; break;
      case ResourceFormat::RGBA8: bpp = 4; break;
      case ResourceFormat::RGBA16F: bpp = 8; break;
      case ResourceFormat::RGBA32F: bpp = 16; break;
      case ResourceFormat::R32F: bpp = 4; break;
      case ResourceFormat::RG32F: bpp = 8; break;
      case ResourceFormat::Depth24Stencil8: bpp = 4; break;
      case ResourceFormat::Depth32F: bpp = 4; break;
      default: bpp = 4; break;
      }
      // MSAA candidates are excluded above (H3), so sampleCount is 1 here;
      // the multiplication is defensive so the estimate stays correct if a
      // future pool gains MSAA backing.
      estimatedSize = static_cast<size_t>(width) * height * bpp *
                      std::max(1u, res.descriptor.mipLevels) *
                      std::max(1u, res.descriptor.arrayLayers) *
                      std::max(1u, res.descriptor.sampleCount);
    }
    interval.estimatedSizeBytes = Align256(std::max<size_t>(256, estimatedSize));
    intervals.push_back(interval);
  }

  m_compiledPlan.aliasingTable.intervals = intervals;

  // 2. Check safety valve: if aliasing is disabled, generate exact allocation path
  if (!s_transientAliasingEnabled) {
    m_compiledPlan.aliasingTable.exactAllocationMode = true;
    size_t totalBytes = 0;
    uint32_t groupIndex = 0;
    for (const auto &inv : intervals) {
      TransientAliasingEntry entry = {};
      entry.originalResourceId = inv.stableResourceId;
      entry.resourceName = inv.resourceName;
      entry.aliasedToResourceId = inv.stableResourceId;
      entry.aliasedToResourceName = inv.resourceName;
      entry.aliasGroupIndex = groupIndex++;
      entry.byteOffset = 0;
      entry.allocatedSizeBytes = inv.estimatedSizeBytes;
      m_compiledPlan.aliasingTable.entries.push_back(entry);
      totalBytes += inv.estimatedSizeBytes;
    }
    m_compiledPlan.aliasingTable.totalVRAMEstimatedBytes = totalBytes;
    m_compiledPlan.aliasingTable.aliasedVRAMEstimatedBytes = totalBytes;
    m_compiledPlan.aliasingTable.memorySavingsRate = 0.0f;
    return;
  }

  // 3. First-fit 256B alignment aliasing for compatible non-overlapping intervals
  struct AliasGroup {
    uint32_t groupIndex = 0;
    uint64_t primaryResourceId = 0;
    std::string primaryResourceName;
    ResourceKind kind = ResourceKind::Texture2D;
    ResourceFormat format = ResourceFormat::RGBA8;
    // H3: the group key must cover the full descriptor compatibility: two
    // resources may share one backing store only when kind, format, sample
    // count, mip-chain depth AND usage flags all match. Comparing only
    // kind+format previously merged MSAA vs linear targets, differing mip
    // chains, and sample-vs-render-target usages into one group, which could
    // produce corrupted rendering through erroneous reuse.
    uint32_t sampleCount = 1;
    uint32_t mipLevels = 1;
    uint32_t usageFlags = ResourceUsage::ColorAttachment;
    size_t maxSizeBytes = 0;
    std::vector<size_t> intervalIndices;
  };

  std::vector<AliasGroup> groups;
  size_t totalBytes = 0;

  for (size_t i = 0; i < intervals.size(); ++i) {
    const auto &inv = intervals[i];
    totalBytes += inv.estimatedSizeBytes;

    // Find compatible existing group with no interval overlap
    bool placed = false;
    for (AliasGroup &group : groups) {
      // H3: full descriptor compatibility — kind, format, sample count, mip
      // depth and usage flags must all match before sharing is allowed.
      if (group.kind == inv.descriptor.kind && group.format == inv.descriptor.format &&
          group.sampleCount == inv.descriptor.sampleCount &&
          group.mipLevels == inv.descriptor.mipLevels &&
          group.usageFlags == inv.descriptor.usageFlags) {
        // Check non-overlapping with all intervals in this group
        bool overlaps = false;
        for (size_t groupInvIdx : group.intervalIndices) {
          const auto &other = intervals[groupInvIdx];
          if (!(inv.lastUsePassIndex < other.firstUsePassIndex || other.lastUsePassIndex < inv.firstUsePassIndex)) {
            overlaps = true;
            break;
          }
        }
        if (!overlaps) {
          group.intervalIndices.push_back(i);
          group.maxSizeBytes = std::max(group.maxSizeBytes, inv.estimatedSizeBytes);
          TransientAliasingEntry entry = {};
          entry.originalResourceId = inv.stableResourceId;
          entry.resourceName = inv.resourceName;
          entry.aliasedToResourceId = group.primaryResourceId;
          entry.aliasedToResourceName = group.primaryResourceName;
          entry.aliasGroupIndex = group.groupIndex;
          entry.byteOffset = 0;
          entry.allocatedSizeBytes = inv.estimatedSizeBytes;
          m_compiledPlan.aliasingTable.entries.push_back(entry);
          placed = true;
          break;
        }
      }
    }

    if (!placed) {
      AliasGroup newGroup = {};
      newGroup.groupIndex = static_cast<uint32_t>(groups.size());
      newGroup.primaryResourceId = inv.stableResourceId;
      newGroup.primaryResourceName = inv.resourceName;
      newGroup.kind = inv.descriptor.kind;
      newGroup.format = inv.descriptor.format;
      newGroup.sampleCount = inv.descriptor.sampleCount;
      newGroup.mipLevels = inv.descriptor.mipLevels;
      newGroup.usageFlags = inv.descriptor.usageFlags;
      newGroup.maxSizeBytes = inv.estimatedSizeBytes;
      newGroup.intervalIndices.push_back(i);

      TransientAliasingEntry entry = {};
      entry.originalResourceId = inv.stableResourceId;
      entry.resourceName = inv.resourceName;
      entry.aliasedToResourceId = inv.stableResourceId;
      entry.aliasedToResourceName = inv.resourceName;
      entry.aliasGroupIndex = newGroup.groupIndex;
      entry.byteOffset = 0;
      entry.allocatedSizeBytes = inv.estimatedSizeBytes;
      m_compiledPlan.aliasingTable.entries.push_back(entry);

      groups.push_back(std::move(newGroup));
    }
  }

  size_t aliasedBytes = 0;
  for (const auto &g : groups) {
    aliasedBytes += g.maxSizeBytes;
  }

  // H4: the interval graph above is a compile-time *potential*, not a runtime
  // realization. TransientResourcePool holds entries at frame granularity
  // (BeginFrame clears inUse, EndFrame retires), and Execute() has no
  // pass-boundary Acquire/Release wiring, so two same-frame non-overlapping
  // intervals never share backing memory in practice. The realized VRAM
  // savings from aliasing is therefore zero; real savings come only from the
  // pool's cross-frame exact-match entry cache (TransientResourcePool::
  // GetAliasedReuseCount), which is outside this table's accounting scope.
  // Report the realized rate (0) so the metric matches GetTrackedBytes() and
  // actual VRAM accounting, and keep the estimated totals as documented
  // potential that becomes real only if pass-granular aliasing (H4 option 1)
  // is implemented.
  m_compiledPlan.aliasingTable.totalVRAMEstimatedBytes = totalBytes;
  m_compiledPlan.aliasingTable.aliasedVRAMEstimatedBytes = aliasedBytes;
  m_compiledPlan.aliasingTable.memorySavingsRate = 0.0f;
}

void RenderGraph::AddValidationDiagnostic(
    ValidationDiagnostic::Severity severity, size_t passIndex,
    const std::string &passName, const std::string &resourceName,
    const std::string &message) {
  ValidationDiagnostic diagnostic = {};
  diagnostic.severity = severity;
  diagnostic.passIndex = passIndex;
  diagnostic.passName = passName;
  diagnostic.resourceName = resourceName;
  diagnostic.message = message;
  m_validationDiagnostics.push_back(std::move(diagnostic));
  if (severity == ValidationDiagnostic::Severity::Error) {
    m_hasValidationErrors = true;
  }
}

std::string RenderGraph::CompiledRenderPlan::DumpPlan() const {
  std::ostringstream ss;
  ss << "=== CompiledRenderPlan Dump ===\n";
  ss << "Status: " << (isValid ? "VALID" : "INVALID") << "\n";
  ss << "Pass Count: " << passOrder.size() << "\n";
  ss << "Pass Order:\n";
  for (size_t i = 0; i < passOrder.size(); ++i) {
    ss << "  [" << i << "] " << passOrder[i]
       << (cullingInfo.passCulled.size() > i && cullingInfo.passCulled[i] ? " [CULLED]" : "") << "\n";
  }

  ss << "Stable Pass IDs:\n";
  for (const auto &pass : passes) {
    ss << "  [" << pass.passIndex << "] " << pass.passName << " id=0x"
       << std::hex << pass.stablePassId << std::dec
       << (pass.isCulled ? " [CULLED]" : "") << "\n";
  }

  ss << "Pass Culling Info:\n";
  ss << "  Total Passes: " << cullingInfo.totalPassCount
     << ", Culled: " << cullingInfo.culledPassCount
     << ", Culling Rate: " << (cullingInfo.cullingRate * 100.0f) << "%\n";
  if (!cullingInfo.culledPassNames.empty()) {
    ss << "  Culled Passes: ";
    for (const auto &name : cullingInfo.culledPassNames) {
      ss << name << " ";
    }
    ss << "\n";
  }

  ss << "Transient Aliasing Table:\n";
  ss << "  Enabled: " << (aliasingTable.enabled ? "YES" : "NO")
     << ", Exact Allocation Mode: " << (aliasingTable.exactAllocationMode ? "YES" : "NO") << "\n";
  ss << "  Estimated Total VRAM (potential): " << aliasingTable.totalVRAMEstimatedBytes << " B"
     << ", Aliased VRAM (potential): " << aliasingTable.aliasedVRAMEstimatedBytes << " B\n";
  ss << "  Realized Savings: " << (aliasingTable.memorySavingsRate * 100.0f)
     << "% (frame-interval aliasing NOT realized at runtime: pool is "
        "frame-granular, see H4; cross-frame reuse is tracked by "
        "TransientResourcePool::GetAliasedReuseCount)\n";
  for (const auto &entry : aliasingTable.entries) {
    ss << "  - Resource: " << entry.resourceName << " -> Group #" << entry.aliasGroupIndex
       << " (AliasedTo: " << entry.aliasedToResourceName
       << ", Offset: " << entry.byteOffset
       << ", Size: " << entry.allocatedSizeBytes << " B)\n";
  }

  ss << "Resource States (" << resources.size() << "):\n";
  for (const auto &res : resources) {
    ss << "  - Resource: " << res.resourceName
       << " (Tag=" << static_cast<int>(res.tag) << ")\n";
    ss << "    Producer Pass Index: " << (res.hasProducer ? std::to_string(res.firstProducerPassIndex) : "None") << "\n";
    ss << "    Writers: ";
    for (size_t w : res.writerPassIndices) ss << w << " ";
    ss << "\n    Readers: ";
    for (size_t r : res.readerPassIndices) ss << r << " ";
    ss << "\n";
  }

  ss << "Dependency Edges (" << edges.size() << "):\n";
  for (const auto &edge : edges) {
    ss << "  Pass #" << edge.producerPassIndex << " (" << edge.producerPassName << ")"
       << " -> Pass #" << edge.consumerPassIndex << " (" << edge.consumerPassName << ")"
       << " via " << edge.resourceName << "\n";
  }

  ss << "Transitions (" << transitions.size() << "):\n";
  for (const auto &transition : transitions) {
    ss << "  - Resource " << transition.resourceName
       << " (id=" << transition.stableResourceId << ")"
       << " before pass #" << transition.consumerPassIndex
       << " bits=0x" << std::hex << transition.barrierBits << std::dec << "\n";
  }

  ss << "Phase Barriers (" << phaseBarriers.size() << "):\n";
  for (const auto &barrier : phaseBarriers) {
    ss << "  - Pass #" << barrier.passIndex << " (" << barrier.passName << ")"
       << " " << ToPipelineStageName(barrier.sourcePhase) << " -> "
       << ToPipelineStageName(barrier.targetPhase) << " bits=0x"
       << std::hex << barrier.barrierBits << std::dec << "\n";
  }

  ss << "Resource Bindings (" << bindings.size() << "):\n";
  for (const auto &binding : bindings) {
    ss << "  - Pass #" << binding.passIndex << " (" << binding.passName << ")"
       << " " << binding.resourceName << " kind="
       << static_cast<int>(binding.kind) << " point=" << binding.point
       << " access=0x" << std::hex << binding.access
       << " format=0x" << binding.format << std::dec << "\n";
  }

  ss << "Resource Imports (" << imports.size() << "):\n";
  for (const auto &import : imports) {
    ss << "  - Pass #" << import.passIndex << " (" << import.passName << ")"
       << " " << import.resourceName << " kind="
       << static_cast<int>(import.kind) << " format="
       << static_cast<int>(import.format) << " owner="
       << ToOwnerName(import.backingOwner)
       << " resizeScreen=" << (import.resizeFollowsScreen ? "y" : "n")
       << " resizeCapacity=" << (import.resizeFollowsCapacity ? "y" : "n")
       << " bindingPoint=" << import.bindingPoint
       << " imageUnit=" << import.imageUnit
       << " imageAccess=0x" << std::hex << import.imageAccess
       << " imageFormat=0x" << import.imageFormat
       << " colorAttachment=" << std::dec << import.colorAttachmentIndex << "\n";
  }

  if (!diagnostics.empty()) {
    ss << "Diagnostics (" << diagnostics.size() << "):\n";
    for (const auto &diag : diagnostics) {
      ss << "  [" << (diag.severity == ValidationDiagnostic::Severity::Error ? "ERROR" : "WARN")
         << "] Pass #" << diag.passIndex << " " << diag.passName
         << " (Resource: " << diag.resourceName << "): " << diag.message << "\n";
    }
  }

  ss << "===============================\n";
  return ss.str();
}

RenderGraph::BindingResolutionResult RenderGraph::ResolvePassBindings(
    size_t passIndex, const RenderContext &context) const {
  BindingResolutionResult result = {};
  result.allAdmitted = true;

  if (passIndex >= m_nodes.size()) {
    result.allAdmitted = false;
    ValidationDiagnostic diagnostic = {};
    diagnostic.severity = ValidationDiagnostic::Severity::Error;
    diagnostic.passIndex = passIndex;
    diagnostic.passName = "(binding)";
    diagnostic.resourceName = "(binding)";
    diagnostic.message =
        "binding resolution requested for an out-of-range pass index";
    result.diagnostics.push_back(std::move(diagnostic));
    return result;
  }

  const Node &node = m_nodes[passIndex];
  if (node.bindings.empty()) {
    // Vacuous admission: a pass with no declared bindings is fully admitted.
    return result;
  }

  // Index this pass's imports and the per-frame snapshot handles by tag.
  // Handles are copied from the snapshot; the graph never owns GL resources.
  std::unordered_map<RenderResourceTag, const ResourceImportInfo *> importsByTag;
  importsByTag.reserve(node.imports.size());
  for (const ResourceImportInfo &import : node.imports) {
    importsByTag[import.resourceTag] = &import;
  }

  std::unordered_map<RenderResourceTag, const ImportedBackingHandle *> snapshotByTag;
  snapshotByTag.reserve(context.importedBackings.size());
  std::unordered_set<RenderResourceTag> duplicateSnapshotTags;
  for (const ImportedBackingHandle &backing : context.importedBackings) {
    const auto [it, inserted] =
        snapshotByTag.emplace(backing.resourceTag, &backing);
    if (!inserted) {
      duplicateSnapshotTags.insert(backing.resourceTag);
    }
  }

  for (const ResourceBindingDeclaration &binding : node.bindings) {
    const std::string resourceName = ToResourceName(binding.resourceTag);

    // 1. Supported binding kinds only (B12 scope: BufferBase, ImageUnit).
    if (binding.kind != ResourceBindingKind::BufferBase &&
        binding.kind != ResourceBindingKind::ImageUnit) {
      result.allAdmitted = false;
      ValidationDiagnostic diagnostic = {};
      diagnostic.severity = ValidationDiagnostic::Severity::Warning;
      diagnostic.passIndex = passIndex;
      diagnostic.passName = node.passName;
      diagnostic.resourceName = resourceName;
      diagnostic.message =
          "binding kind is unsupported by graph-driven binding (B12); manual "
          "bind inside Execute stays authoritative";
      result.diagnostics.push_back(std::move(diagnostic));
      continue;
    }

    // 2. A matching ImportResource declaration is required for admission.
    const auto importIt = importsByTag.find(binding.resourceTag);
    if (importIt == importsByTag.end()) {
      result.allAdmitted = false;
      ValidationDiagnostic diagnostic = {};
      diagnostic.severity = ValidationDiagnostic::Severity::Error;
      diagnostic.passIndex = passIndex;
      diagnostic.passName = node.passName;
      diagnostic.resourceName = resourceName;
      diagnostic.message =
          "binding declared without a matching ImportResource; graph-driven "
          "bind denied (manual bind stays authoritative)";
      result.diagnostics.push_back(std::move(diagnostic));
      continue;
    }
    const ResourceImportInfo &import = *importIt->second;

    // 3. Tag/kind consistency: the imported backing kind must be compatible
    //    with the binding kind before any handle is looked up.
    const bool kindCompatible =
        (binding.kind == ResourceBindingKind::BufferBase &&
         IsBufferBindingKind(import.kind)) ||
        (binding.kind == ResourceBindingKind::ImageUnit &&
         IsImageBindingKind(import.kind));
    if (!kindCompatible) {
      result.allAdmitted = false;
      ValidationDiagnostic diagnostic = {};
      diagnostic.severity = ValidationDiagnostic::Severity::Error;
      diagnostic.passIndex = passIndex;
      diagnostic.passName = node.passName;
      diagnostic.resourceName = resourceName;
      diagnostic.message =
          "import kind is incompatible with the binding kind; graph-driven "
          "bind denied";
      result.diagnostics.push_back(std::move(diagnostic));
      continue;
    }

    // 4. Snapshot presence plus a valid, non-zero handle for the kind that the
    //    operation actually binds. A zero handle is NEVER bound.
    const auto snapshotIt = snapshotByTag.find(binding.resourceTag);
    if (snapshotIt == snapshotByTag.end()) {
      result.allAdmitted = false;
      ValidationDiagnostic diagnostic = {};
      diagnostic.severity = ValidationDiagnostic::Severity::Error;
      diagnostic.passIndex = passIndex;
      diagnostic.passName = node.passName;
      diagnostic.resourceName = resourceName;
      diagnostic.message =
          "no imported backing snapshot for this frame; graph-driven bind "
          "denied (zero handle is never bound)";
      result.diagnostics.push_back(std::move(diagnostic));
      continue;
    }
    if (duplicateSnapshotTags.contains(binding.resourceTag)) {
      result.allAdmitted = false;
      ValidationDiagnostic diagnostic = {};
      diagnostic.severity = ValidationDiagnostic::Severity::Error;
      diagnostic.passIndex = passIndex;
      diagnostic.passName = node.passName;
      diagnostic.resourceName = resourceName;
      diagnostic.message =
          "multiple imported backing snapshots exist for this tag; "
          "graph-driven bind denied";
      result.diagnostics.push_back(std::move(diagnostic));
      continue;
    }
    const ImportedBackingHandle &snapshot = *snapshotIt->second;
    const uint32_t handle = (binding.kind == ResourceBindingKind::BufferBase)
                                ? snapshot.bufferHandle
                                : snapshot.textureHandle;
    if (!snapshot.IsValidFor(import.kind) || handle == 0u) {
      result.allAdmitted = false;
      ValidationDiagnostic diagnostic = {};
      diagnostic.severity = ValidationDiagnostic::Severity::Error;
      diagnostic.passIndex = passIndex;
      diagnostic.passName = node.passName;
      diagnostic.resourceName = resourceName;
      diagnostic.message =
          "imported backing snapshot carries a zero/invalid handle for the "
          "admitted kind; graph-driven bind denied (zero handle is never bound)";
      result.diagnostics.push_back(std::move(diagnostic));
      continue;
    }

    // 5. Binding-surface consistency with the import metadata. Warning only:
    //    the manual bind inside Execute remains the authoritative surface.
    if (binding.kind == ResourceBindingKind::BufferBase &&
        import.bindingPoint != 0u && import.bindingPoint != binding.point) {
      ValidationDiagnostic diagnostic = {};
      diagnostic.severity = ValidationDiagnostic::Severity::Warning;
      diagnostic.passIndex = passIndex;
      diagnostic.passName = node.passName;
      diagnostic.resourceName = resourceName;
      diagnostic.message =
          "binding point does not match the import binding point; manual bind "
          "inside Execute stays authoritative";
      result.diagnostics.push_back(std::move(diagnostic));
    }
    if (binding.kind == ResourceBindingKind::ImageUnit) {
      if (import.imageUnit != 0u && import.imageUnit != binding.point) {
        ValidationDiagnostic diagnostic = {};
        diagnostic.severity = ValidationDiagnostic::Severity::Warning;
        diagnostic.passIndex = passIndex;
        diagnostic.passName = node.passName;
        diagnostic.resourceName = resourceName;
        diagnostic.message =
            "image unit does not match the import image unit; manual bind "
            "inside Execute stays authoritative";
        result.diagnostics.push_back(std::move(diagnostic));
      }
      if (import.imageAccess != 0u && import.imageAccess != binding.access) {
        ValidationDiagnostic diagnostic = {};
        diagnostic.severity = ValidationDiagnostic::Severity::Warning;
        diagnostic.passIndex = passIndex;
        diagnostic.passName = node.passName;
        diagnostic.resourceName = resourceName;
        diagnostic.message =
            "image access does not match the import image access; manual bind "
            "inside Execute stays authoritative";
        result.diagnostics.push_back(std::move(diagnostic));
      }
      if (import.imageFormat != 0u && import.imageFormat != binding.format) {
        ValidationDiagnostic diagnostic = {};
        diagnostic.severity = ValidationDiagnostic::Severity::Warning;
        diagnostic.passIndex = passIndex;
        diagnostic.passName = node.passName;
        diagnostic.resourceName = resourceName;
        diagnostic.message =
            "image format does not match the import image format; manual bind "
            "inside Execute stays authoritative";
        result.diagnostics.push_back(std::move(diagnostic));
      }
    }

    // Admitted: emit the resolved operation; the executor issues the GL call.
    ResolvedBindingOperation operation = {};
    operation.resourceTag = binding.resourceTag;
    operation.point = binding.point;
    if (binding.kind == ResourceBindingKind::BufferBase) {
      operation.kind = ResolvedBindingOperation::Kind::BindBufferBase;
      operation.handle = snapshot.bufferHandle;
    } else {
      operation.kind = ResolvedBindingOperation::Kind::BindImageTexture;
      operation.handle = snapshot.textureHandle;
      operation.access = binding.access;
      operation.format = binding.format;
    }
    result.operations.push_back(std::move(operation));
  }

  return result;
}

RenderGraph::BindingResolutionResult RenderGraph::ResolveActivePassBindings(
    const RenderContext &context) const {
  if (m_activeNodeIndex >= m_nodes.size()) {
    BindingResolutionResult result = {};
    result.allAdmitted = false;
    ValidationDiagnostic diagnostic = {};
    diagnostic.severity = ValidationDiagnostic::Severity::Error;
    diagnostic.passIndex = m_activeNodeIndex;
    diagnostic.passName = "(binding)";
    diagnostic.resourceName = "(binding)";
    diagnostic.message =
        "binding resolution requested outside RenderGraph::Execute (no active "
        "pass)";
    result.diagnostics.push_back(std::move(diagnostic));
    return result;
  }
  return ResolvePassBindings(m_activeNodeIndex, context);
}

bool RenderGraph::ApplyActivePassBindings(RenderContext &context) {
  if (m_activeNodeIndex >= m_nodes.size()) {
    LOG_ERROR(
        "RenderGraph: ApplyActivePassBindings called outside "
        "RenderGraph::Execute (no active pass)");
    return false;
  }

  const BindingResolutionResult result =
      ResolvePassBindings(m_activeNodeIndex, context);

  for (const ValidationDiagnostic &diagnostic : result.diagnostics) {
    m_runtimeBindingDiagnostics.push_back(diagnostic);
    if (diagnostic.severity == ValidationDiagnostic::Severity::Error) {
      LOG_ERROR(
          "RenderGraph: graph-driven bind denied (pass #{} '{}' resource={}): {}",
          diagnostic.passIndex, diagnostic.passName, diagnostic.resourceName,
          diagnostic.message);
    } else {
      LOG_WARN(
          "RenderGraph: graph-driven bind notice (pass #{} '{}' resource={}): {}",
          diagnostic.passIndex, diagnostic.passName, diagnostic.resourceName,
          diagnostic.message);
    }
  }

  // Issue only admitted operations via the existing GPUUtils binding APIs.
  // Unsupported / None kinds are never issued as GL binds.
  for (const ResolvedBindingOperation &operation : result.operations) {
    if (operation.kind == ResolvedBindingOperation::Kind::BindBufferBase) {
      NoMoreDay::utils::GPUUtils::BindBufferBase(operation.point,
                                                 operation.handle);
    } else if (operation.kind ==
               ResolvedBindingOperation::Kind::BindImageTexture) {
      NoMoreDay::utils::GPUUtils::BindImageTexture(
          operation.point, operation.handle, 0, false, 0, operation.access,
          operation.format);
    }
  }

  return result.allAdmitted;
}

} // namespace NoMoreDay::render::graph
