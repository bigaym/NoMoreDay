#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace NoMoreDay::render::graph {

constexpr uint32_t RENDERGRAPH_CONTRACT_VERSION = 4;

enum class RenderResourceTag : uint8_t {
  Custom = 0,
  SceneHdrColor,
  SceneDepth,
  PostProcessLdrColor,
  DistortionLdrColor,
  FinalOutputColor,
  OccluderMask,
  DistanceField,
  EmissiveBuffer,
  RadianceMap,
  GIHistoryColor,
  LightBufferSSBO,
  TileLightIndexSSBO,
  VFXParticleSSBO,
  FluidParticleSSBO,
  GPUTextBufferSSBO,
  GPULootBufferSSBO,
};

enum class RenderOwnerTag : uint8_t {
  Unknown = 0,
  Scene,
  Lighting,
  HeightShadow,
  OccluderExtract,
  JFA,
  RadianceCascades,
  GIComposite,
  FluidSimulation,
  Volumetric,
  VFX,
  GPUText,
  GPULoot,
  UIWorld,
  PostProcess,
  Distortion,
  Composite,
};

constexpr const char *ToResourceName(RenderResourceTag resourceTag) {
  switch (resourceTag) {
  case RenderResourceTag::SceneHdrColor:
    return "SceneColor";
  case RenderResourceTag::SceneDepth:
    return "SceneDepth";
  case RenderResourceTag::PostProcessLdrColor:
    return "PostProcessColor";
  case RenderResourceTag::DistortionLdrColor:
    return "DistortionColor";
  case RenderResourceTag::FinalOutputColor:
    return "BackBuffer";
  case RenderResourceTag::OccluderMask:
    return "OccluderMask";
  case RenderResourceTag::DistanceField:
    return "DistanceField";
  case RenderResourceTag::EmissiveBuffer:
    return "EmissiveBuffer";
  case RenderResourceTag::RadianceMap:
    return "RadianceMap";
  case RenderResourceTag::GIHistoryColor:
    return "GIHistoryColor";
  case RenderResourceTag::LightBufferSSBO:
    return "LightBufferSSBO";
  case RenderResourceTag::TileLightIndexSSBO:
    return "TileLightIndexSSBO";
  case RenderResourceTag::VFXParticleSSBO:
    return "VFXParticleSSBO";
  case RenderResourceTag::FluidParticleSSBO:
    return "FluidParticleSSBO";
  case RenderResourceTag::GPUTextBufferSSBO:
    return "GPUTextBufferSSBO";
  case RenderResourceTag::GPULootBufferSSBO:
    return "GPULootBufferSSBO";
  case RenderResourceTag::Custom:
  default:
    return "";
  }
}

constexpr RenderResourceTag ToResourceTag(std::string_view resourceName) {
  if (resourceName == "SceneColor") {
    return RenderResourceTag::SceneHdrColor;
  }
  if (resourceName == "SceneDepth") {
    return RenderResourceTag::SceneDepth;
  }
  if (resourceName == "PostProcessColor") {
    return RenderResourceTag::PostProcessLdrColor;
  }
  if (resourceName == "DistortionColor") {
    return RenderResourceTag::DistortionLdrColor;
  }
  if (resourceName == "BackBuffer") {
    return RenderResourceTag::FinalOutputColor;
  }
  if (resourceName == "OccluderMask") {
    return RenderResourceTag::OccluderMask;
  }
  if (resourceName == "DistanceField") {
    return RenderResourceTag::DistanceField;
  }
  if (resourceName == "EmissiveBuffer") {
    return RenderResourceTag::EmissiveBuffer;
  }
  if (resourceName == "RadianceMap") {
    return RenderResourceTag::RadianceMap;
  }
  if (resourceName == "GIHistoryColor") {
    return RenderResourceTag::GIHistoryColor;
  }
  if (resourceName == "LightBufferSSBO") {
    return RenderResourceTag::LightBufferSSBO;
  }
  if (resourceName == "TileLightIndexSSBO") {
    return RenderResourceTag::TileLightIndexSSBO;
  }
  if (resourceName == "VFXParticleSSBO") {
    return RenderResourceTag::VFXParticleSSBO;
  }
  if (resourceName == "FluidParticleSSBO") {
    return RenderResourceTag::FluidParticleSSBO;
  }
  if (resourceName == "GPUTextBufferSSBO") {
    return RenderResourceTag::GPUTextBufferSSBO;
  }
  if (resourceName == "GPULootBufferSSBO") {
    return RenderResourceTag::GPULootBufferSSBO;
  }
  return RenderResourceTag::Custom;
}

constexpr const char *ToOwnerName(RenderOwnerTag ownerTag) {
  switch (ownerTag) {
  case RenderOwnerTag::Scene:
    return "Scene";
  case RenderOwnerTag::Lighting:
    return "Lighting";
  case RenderOwnerTag::HeightShadow:
    return "HeightShadow";
  case RenderOwnerTag::OccluderExtract:
    return "OccluderExtract";
  case RenderOwnerTag::JFA:
    return "JFA";
  case RenderOwnerTag::RadianceCascades:
    return "RadianceCascades";
  case RenderOwnerTag::GIComposite:
    return "GIComposite";
  case RenderOwnerTag::FluidSimulation:
    return "FluidSimulation";
  case RenderOwnerTag::Volumetric:
    return "Volumetric";
  case RenderOwnerTag::VFX:
    return "VFX";
  case RenderOwnerTag::GPUText:
    return "GPUText";
  case RenderOwnerTag::GPULoot:
    return "GPULoot";
  case RenderOwnerTag::UIWorld:
    return "UIWorld";
  case RenderOwnerTag::PostProcess:
    return "PostProcess";
  case RenderOwnerTag::Distortion:
    return "Distortion";
  case RenderOwnerTag::Composite:
    return "Composite";
  case RenderOwnerTag::Unknown:
  default:
    return "Unknown";
  }
}

struct ResourceAccess {
  enum class Type {
    Read,
    Write,
  };

  std::string resourceName;
  Type type = Type::Read;
  RenderResourceTag resourceTag = RenderResourceTag::Custom;
  RenderOwnerTag ownerTag = RenderOwnerTag::Unknown;
  uint64_t stableResourceId = 0;
  // Set only by the string-based Read/Write(const std::string&) overloads.
  // Such accesses carry no typed Tag/Owner/Stage/Usage and are denied by
  // default at Build validation (see ValidateBuildContracts).
  bool isStringBasedAccess = false;
};

} // namespace NoMoreDay::render::graph

#include "engine/render/graph/RenderResourceDescriptor.hpp"

namespace NoMoreDay::render::graph {

// ---------------------------------------------------------------------------
// Stable pass identity (S0)
//
// Deterministic stable pass IDs are derived from a canonicalized pass name
// plus a versioned FNV-1a 64 hash. The canonicalization rules, hash algorithm,
// and version salt below are FIXED contracts: changing any of them migrates
// every existing pass to a new ID (rename/migration rule), and tests plus any
// recorded baselines must be updated in the same change.
//
//   canonicalization  : strip all whitespace, then lowercase
//                       (see CanonicalizePassName)
//   hash algorithm    : FNV-1a 64
//     offset basis    : 1469598103934665603
//     prime           : 1099511628211
//   seed bytes        : ASCII(kStablePassVersionSalt) ++ ASCII(canonicalName)
//   stablePassId      : uint32(FNV-1a 64(salt ++ canonicalName))  (low 32 bits)
//
// Reserved IDs (any collision fails closed at RenderGraph::Build):
//   0                : unassigned/invalid
//   0xFFFFFFFF       : frame-level aggregation slot; MUST equal
//                      GPUTimerQueryRing::kFramePassId
// ---------------------------------------------------------------------------
constexpr std::string_view kStablePassVersionSalt = "NMD-STABLEPASS-V1";
constexpr uint64_t kStablePassHashOffsetBasis = 1469598103934665603ull;
constexpr uint64_t kStablePassHashPrime = 1099511628211ull;
constexpr uint32_t kInvalidStablePassId = 0u;
constexpr uint32_t kFrameLevelStablePassId = 0xFFFFFFFFu;

constexpr uint32_t StablePassId(std::string_view canonicalName) {
  uint64_t hash = kStablePassHashOffsetBasis;
  for (const char character : kStablePassVersionSalt) {
    hash ^= static_cast<uint8_t>(character);
    hash *= kStablePassHashPrime;
  }
  for (const char character : canonicalName) {
    hash ^= static_cast<uint8_t>(character);
    hash *= kStablePassHashPrime;
  }
  return static_cast<uint32_t>(hash);
}

inline std::string CanonicalizePassName(std::string_view passName) {
  std::string canonical;
  canonical.reserve(passName.size());
  for (const char character : passName) {
    const unsigned char c = static_cast<unsigned char>(character);
    if (std::isspace(c) != 0) {
      continue;
    }
    canonical.push_back(static_cast<char>(std::tolower(c)));
  }
  return canonical;
}

class RenderGraphBuilder {
public:
  void Read(const std::string &resourceName);
  void Write(const std::string &resourceName);
  void Read(RenderResourceTag resourceTag, RenderOwnerTag ownerTag);
  void Write(RenderResourceTag resourceTag, RenderOwnerTag ownerTag);
  void Read(RenderResourceTag resourceTag, RenderOwnerTag ownerTag, PipelineStage stage, uint32_t usageFlags = ResourceUsage::ShaderRead);
  void Write(RenderResourceTag resourceTag, RenderOwnerTag ownerTag, PipelineStage stage, uint32_t usageFlags = ResourceUsage::ColorAttachment);
  void Read(const TypedPassAccess &access);
  void Write(const TypedPassAccess &access);
  void DeclareResource(const TypedResourceDescriptor &descriptor);
  void AddPassLocalBarrier(uint32_t barrierBits);

  const std::vector<ResourceAccess> &GetAccesses() const { return m_accesses; }
  const std::vector<TypedPassAccess> &GetTypedAccesses() const { return m_typedAccesses; }
  const std::vector<TypedResourceDescriptor> &GetDeclaredDescriptors() const { return m_declaredDescriptors; }
  const std::vector<uint32_t> &GetPassLocalBarriers() const { return m_passLocalBarriers; }

private:
  std::vector<ResourceAccess> m_accesses;
  std::vector<TypedPassAccess> m_typedAccesses;
  std::vector<TypedResourceDescriptor> m_declaredDescriptors;
  std::vector<uint32_t> m_passLocalBarriers;
};

struct RenderContext;

struct ProducerConsumerEdge {
  size_t producerPassIndex = 0;
  std::string producerPassName;
  size_t consumerPassIndex = 0;
  std::string consumerPassName;
  std::string resourceName;
  RenderResourceTag resourceTag = RenderResourceTag::Custom;
};

struct CompiledResourceState {
  uint64_t stableResourceId = 0;
  std::string resourceName;
  RenderResourceTag tag = RenderResourceTag::Custom;
  size_t firstProducerPassIndex = 0;
  size_t lastConsumerPassIndex = 0;
  bool hasProducer = false;
  bool isExternal = false;
  std::vector<size_t> writerPassIndices;
  std::vector<size_t> readerPassIndices;
  TypedResourceDescriptor descriptor;
};

struct RenderTransition {
  uint64_t stableResourceId = 0;
  std::string resourceName;
  size_t previousPassIndex = 0;
  size_t consumerPassIndex = 0;
  PipelineStage previousStage = PipelineStage::Fragment;
  PipelineStage nextStage = PipelineStage::Fragment;
  PassAccessMode previousMode = PassAccessMode::Read;
  PassAccessMode nextMode = PassAccessMode::Read;
  ResourceKind resourceKind = ResourceKind::Texture2D;
  uint32_t barrierBits = 0;
};

class RenderGraph {
public:
  struct ValidationDiagnostic {
    enum class Severity {
      Warning,
      Error,
    };

    Severity severity = Severity::Warning;
    size_t passIndex = 0;
    std::string passName;
    std::string resourceName;
    std::string message;
  };

  struct CompiledPassState {
    uint32_t stablePassId = 0;
    std::string passName;
    size_t passIndex = 0;
  };

  struct CompiledRenderPlan {
    bool isValid = false;
    std::vector<std::string> passOrder;
    std::vector<CompiledPassState> passes;
    std::vector<ProducerConsumerEdge> edges;
    std::vector<CompiledResourceState> resources;
    std::vector<RenderTransition> transitions;
    std::vector<ValidationDiagnostic> diagnostics;

    std::string DumpPlan() const;
  };

  void AddPass(std::shared_ptr<RenderPass> pass);
  void Clear();
  void Build();
  void Execute(RenderContext &context);
  void OnResize(int width, int height);
  static void SetValidationEnabled(bool enabled);
  static bool IsValidationEnabled();
  static void SetTransientAliasingEnabled(bool enabled);
  static bool IsTransientAliasingEnabled();

  size_t GetPassCount() const { return m_nodes.size(); }
  const std::vector<ValidationDiagnostic> &GetValidationDiagnostics() const {
    return m_validationDiagnostics;
  }
  bool HasValidationErrors() const { return m_hasValidationErrors; }

  const CompiledRenderPlan &GetCompiledPlan() const { return m_compiledPlan; }
  std::string DumpCompiledPlan() const { return m_compiledPlan.DumpPlan(); }

private:
  struct Node {
    std::shared_ptr<RenderPass> pass;
    std::string passName;
    std::string canonicalPassName;
    size_t passIndex = 0;
    uint32_t stablePassId = 0;
    std::vector<ResourceAccess> accesses;
    std::vector<TypedPassAccess> typedAccesses;
    std::vector<TypedResourceDescriptor> declaredDescriptors;
    std::vector<uint32_t> passLocalBarriers;
  };

  bool ValidatePassIdentityContract();
  bool RejectLegacyStringAccess();
  void ValidateBuildContracts();
  void BuildCompiledPlan();
  void AddValidationDiagnostic(ValidationDiagnostic::Severity severity,
                               size_t passIndex,
                               const std::string &passName,
                               const std::string &resourceName,
                               const std::string &message);

  std::vector<Node> m_nodes;
  std::vector<ValidationDiagnostic> m_validationDiagnostics;
  CompiledRenderPlan m_compiledPlan;
  bool m_hasValidationErrors = false;
  bool m_isBuilt = false;
  static bool s_validationEnabled;
  static bool s_transientAliasingEnabled;
};

} // namespace NoMoreDay::render::graph
