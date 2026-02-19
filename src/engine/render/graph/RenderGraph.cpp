#include "engine/render/graph/RenderGraph.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/render/core/ScopedGLState.hpp"
#include "engine/render/debug/RenderProfiler.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "rlgl.h"

#include <algorithm>
#include <optional>
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
           ownerTag == RenderOwnerTag::Volumetric ||
           ownerTag == RenderOwnerTag::VFX ||
           ownerTag == RenderOwnerTag::UIWorld;
  case RenderResourceTag::Custom:
    return true;
  case RenderResourceTag::SceneDepth:
  case RenderResourceTag::PostProcessLdrColor:
  case RenderResourceTag::DistortionLdrColor:
  case RenderResourceTag::FinalOutputColor:
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
  Volumetric = 5,
  VFX = 6,
  GPUText = 7,
  GPULoot = 8,
  UIWorld = 9,
  PostProcess = 10,
  Distortion = 11,
  Composite = 12,
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
                        inferredTag, RenderOwnerTag::Unknown});
}

void RenderGraphBuilder::Write(const std::string &resourceName) {
  const RenderResourceTag inferredTag = ToResourceTag(resourceName);
  m_accesses.push_back({resourceName, ResourceAccess::Type::Write,
                        inferredTag, RenderOwnerTag::Unknown});
}

void RenderGraphBuilder::Read(RenderResourceTag resourceTag,
                              RenderOwnerTag ownerTag) {
  m_accesses.push_back(
      {ToResourceName(resourceTag), ResourceAccess::Type::Read, resourceTag, ownerTag});
}

void RenderGraphBuilder::Write(RenderResourceTag resourceTag,
                               RenderOwnerTag ownerTag) {
  m_accesses.push_back({ToResourceName(resourceTag), ResourceAccess::Type::Write,
                        resourceTag, ownerTag});
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
  m_hasValidationErrors = false;
  m_isBuilt = false;
}

void RenderGraph::Build() {
  m_validationDiagnostics.clear();
  m_hasValidationErrors = false;

  for (size_t index = 0; index < m_nodes.size(); ++index) {
    Node &node = m_nodes[index];
    RenderGraphBuilder builder;
    node.pass->Setup(builder);
    node.accesses = builder.GetAccesses();
    node.passName = (node.pass != nullptr && node.pass->GetName() != nullptr)
                        ? node.pass->GetName()
                        : "UnnamedPass";
    node.passIndex = index;
  }

  if (s_validationEnabled) {
    ValidateBuildContracts();
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
  for (Node &node : m_nodes) {
    // Standardized pass boundary: flush previous batched draws and restore GL
    // state after each pass to reduce cross-pass state leakage.
    NoMoreDay::render::core::ApplyRlglFlushTemplate();
    const NoMoreDay::render::core::ScopedGLState scopedState;
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
  }
}

void RenderGraph::ValidateBuildContracts() {
  // Pass order contract:
  // Scene -> Shadow -> LightCulling -> Lighting -> HeightShadow -> Volumetric ->
  // VFX -> GPUText -> GPULoot -> UIWorld -> PostProcess -> Distortion -> Composite
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

} // namespace NoMoreDay::render::graph
