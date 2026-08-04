#include "engine/render/graph/RenderGraph.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/render/core/ScopedGLState.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/debug/RenderProfiler.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "rlgl.h"

#include <algorithm>
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

namespace {

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

std::optional<PassContractStage> ResolvePassContractStage(std::string_view passName) {
  if (passName == "ScenePass") {
    return PassContractStage::Scene;
  }
  if (passName == "LightCullingPass") {
    return PassContractStage::LightCulling;
  }
  if (passName == "ShadowPreparePass" || passName == "ShadowBuildPass" ||
      passName == "ShadowResolvePass") {
    return PassContractStage::Shadow;
  }
  if (passName == "LightingPass") {
    return PassContractStage::Lighting;
  }
  if (passName == "HeightShadowPass") {
    return PassContractStage::HeightShadow;
  }
  if (passName == "OccluderExtractPass") {
    return PassContractStage::OccluderExtract;
  }
  if (passName == "JFAPass") {
    return PassContractStage::JFA;
  }
  if (passName == "RadianceCascadesPass") {
    return PassContractStage::RadianceCascades;
  }
  if (passName == "GICompositePass") {
    return PassContractStage::GIComposite;
  }
  if (passName == "FluidSimulationPass") {
    return PassContractStage::FluidSimulation;
  }
  if (passName == "VolumetricLightPass") {
    return PassContractStage::Volumetric;
  }
  if (passName == "VFXPass") {
    return PassContractStage::VFX;
  }
  if (passName == "GPUTextPass") {
    return PassContractStage::GPUText;
  }
  if (passName == "GPULootPass") {
    return PassContractStage::GPULoot;
  }
  if (passName == "UIWorldPass") {
    return PassContractStage::UIWorld;
  }
  if (passName == "PostProcessPass") {
    return PassContractStage::PostProcess;
  }
  if (passName == "DistortionPass") {
    return PassContractStage::Distortion;
  }
  if (passName == "CompositePass") {
    return PassContractStage::Composite;
  }
  return std::nullopt;
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

void RenderGraph::AddPass(std::shared_ptr<RenderPass> pass) {
  if (!pass) {
    return;
  }
  Node node = {};
  node.pass = std::move(pass);
  m_nodes.push_back(std::move(node));
  m_isBuilt = false;
}

void RenderGraph::Clear() {
  m_nodes.clear();
  m_validationDiagnostics.clear();
  m_compiledPlan = {};
  m_hasValidationErrors = false;
  m_isBuilt = false;
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

void RenderGraph::OnResize(int width, int height) {
  for (Node &node : m_nodes) {
    if (node.pass) {
      node.pass->OnResize(width, height);
    }
  }
}

void RenderGraph::Build() {
  m_validationDiagnostics.clear();
  m_hasValidationErrors = false;

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
    node.passName = (node.pass != nullptr && node.pass->GetName() != nullptr)
                        ? node.pass->GetName()
                        : "UnnamedPass";
    node.passIndex = index;
  }

  const bool identityContractFailed = ValidatePassIdentityContract();
  const bool legacyAccessRejected = RejectLegacyStringAccess();

  if (s_validationEnabled) {
    ValidateBuildContracts();
  }

  BuildCompiledPlan();

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

  for (Node &node : m_nodes) {
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
    const uint32_t stablePassId = node.stablePassId;
    debug::GPUTimerQueryRing::Get().BeginPass(stablePassId);

    if (context.renderProfiler != nullptr) {
      context.renderProfiler->BeginCpuPass(node.pass->GetName());
    }

    context.activeGraph = this;
    m_activeNodeIndex = node.passIndex;
    // B12 graph-driven binding execution: admit this pass's compiled binding
    // declarations against the imported backing snapshot and issue the real GL
    // binds via the existing GPUUtils APIs, immediately before pass Execute.
    // The graph never owns GL handles (they come from the RenderContext
    // snapshot), and manual binds inside Execute remain authoritative and
    // re-bind the same values (behavior-equivalent duplicates). Denied or
    // unsupported bindings are never bound here.
    ApplyActivePassBindings(context);
    node.pass->Execute(context);
    m_activeNodeIndex = static_cast<size_t>(-1);
    context.activeGraph = nullptr;
    NoMoreDay::render::core::ApplyRlglFlushTemplate();

    if (context.renderProfiler != nullptr) {
      context.renderProfiler->EndCpuPass();
    }
    debug::GPUTimerQueryRing::Get().EndPass(stablePassId);
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
    const auto stage = ResolvePassContractStage(node.passName);
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

  for (const auto &[resourceId, state] : resourceMap) {
    m_compiledPlan.resources.push_back(state);
  }

  m_compiledPlan.diagnostics = m_validationDiagnostics;
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
    ss << "  [" << i << "] " << passOrder[i] << "\n";
  }

  ss << "Stable Pass IDs:\n";
  for (const auto &pass : passes) {
    ss << "  [" << pass.passIndex << "] " << pass.passName << " id=0x"
       << std::hex << pass.stablePassId << std::dec << "\n";
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
