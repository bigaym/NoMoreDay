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

} // namespace

void RenderGraphBuilder::Read(const std::string &resourceName) {
  const RenderResourceTag inferredTag = ToResourceTag(resourceName);
  m_accesses.push_back({resourceName, ResourceAccess::Type::Read,
                         inferredTag, RenderOwnerTag::Unknown,
                         StableResourceId(resourceName)});
  m_typedAccesses.push_back({resourceName, inferredTag, PassAccessMode::Read,
                             PipelineStage::Fragment, ResourceUsage::ShaderRead,
                             0, RenderOwnerTag::Unknown});
}

void RenderGraphBuilder::Write(const std::string &resourceName) {
  const RenderResourceTag inferredTag = ToResourceTag(resourceName);
  m_accesses.push_back({resourceName, ResourceAccess::Type::Write,
                         inferredTag, RenderOwnerTag::Unknown,
                         StableResourceId(resourceName)});
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
    node.passName = (node.pass != nullptr && node.pass->GetName() != nullptr)
                        ? node.pass->GetName()
                        : "UnnamedPass";
    node.passIndex = index;
  }

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
    const uint32_t numericPassId = static_cast<uint32_t>(node.passIndex);
    debug::GPUTimerQueryRing::Get().BeginPass(numericPassId);

    const auto passId = (context.renderProfiler != nullptr)
                            ? debug::RenderProfiler::FromPassName(
                                  node.pass->GetName())
                            : std::optional<debug::RenderPassId>{};
    if (context.renderProfiler != nullptr && passId.has_value()) {
      context.renderProfiler->BeginPass(*passId);
    }

    node.pass->Execute(context);
    NoMoreDay::render::core::ApplyRlglFlushTemplate();

    if (context.renderProfiler != nullptr && passId.has_value()) {
      context.renderProfiler->EndPass(*passId);
    }
    debug::GPUTimerQueryRing::Get().EndPass(numericPassId);
  }

  debug::GPUTimerQueryRing::Get().EndFrame();
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
}

void RenderGraph::BuildCompiledPlan() {
  m_compiledPlan = {};
  m_compiledPlan.isValid = !m_hasValidationErrors;

  for (const Node &node : m_nodes) {
    m_compiledPlan.passOrder.push_back(node.passName);
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

} // namespace NoMoreDay::render::graph
