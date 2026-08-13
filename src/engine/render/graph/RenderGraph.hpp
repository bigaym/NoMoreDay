#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include <array>
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
  ParticleEmissive,
  RadianceMap,
  GIHistoryColor,
  LightBufferSSBO,
  TileLightIndexSSBO,
  VFXParticleSSBO,
  FluidParticleSSBO,
  GPUTextBufferSSBO,
  GPULootBufferSSBO,
  ShadowAtlas,
  ShadowDistanceField,
  ShadowMask,
  ShadowOccluderSSBO,
  ClusterHeaderSSBO,
  ClusterLightIndexSSBO,
  ClusterPackedLightSSBO,
  ClusterCounterSSBO,
  LightBoundsSSBO,
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
  Shadow,
  LightCulling,
};

struct RenderResourceNameEntry {
  RenderResourceTag tag;
  const char *name;
};

inline constexpr std::array<RenderResourceNameEntry,
                            static_cast<size_t>(RenderResourceTag::LightBoundsSSBO) + 1>
    kRenderResourceNames = {{
        {RenderResourceTag::Custom, ""},
        {RenderResourceTag::SceneHdrColor, "SceneColor"},
        {RenderResourceTag::SceneDepth, "SceneDepth"},
        {RenderResourceTag::PostProcessLdrColor, "PostProcessColor"},
        {RenderResourceTag::DistortionLdrColor, "DistortionColor"},
        {RenderResourceTag::FinalOutputColor, "BackBuffer"},
        {RenderResourceTag::OccluderMask, "OccluderMask"},
        {RenderResourceTag::DistanceField, "DistanceField"},
        {RenderResourceTag::EmissiveBuffer, "EmissiveBuffer"},
        {RenderResourceTag::ParticleEmissive, "ParticleEmissive"},
        {RenderResourceTag::RadianceMap, "RadianceMap"},
        {RenderResourceTag::GIHistoryColor, "GIHistoryColor"},
        {RenderResourceTag::LightBufferSSBO, "LightBufferSSBO"},
        {RenderResourceTag::TileLightIndexSSBO, "TileLightIndexSSBO"},
        {RenderResourceTag::VFXParticleSSBO, "VFXParticleSSBO"},
        {RenderResourceTag::FluidParticleSSBO, "FluidParticleSSBO"},
        {RenderResourceTag::GPUTextBufferSSBO, "GPUTextBufferSSBO"},
        {RenderResourceTag::GPULootBufferSSBO, "GPULootBufferSSBO"},
        {RenderResourceTag::ShadowAtlas, "ShadowAtlas"},
        {RenderResourceTag::ShadowDistanceField, "ShadowDistanceField"},
        {RenderResourceTag::ShadowMask, "ShadowMask"},
        {RenderResourceTag::ShadowOccluderSSBO, "ShadowOccluderSSBO"},
        {RenderResourceTag::ClusterHeaderSSBO, "ClusterHeaderSSBO"},
        {RenderResourceTag::ClusterLightIndexSSBO, "ClusterLightIndexSSBO"},
        {RenderResourceTag::ClusterPackedLightSSBO, "ClusterPackedLightSSBO"},
        {RenderResourceTag::ClusterCounterSSBO, "ClusterCounterSSBO"},
        {RenderResourceTag::LightBoundsSSBO, "LightBoundsSSBO"},
    }};

static_assert(
    [] {
      for (size_t i = 0; i < kRenderResourceNames.size(); ++i) {
        if (static_cast<size_t>(kRenderResourceNames[i].tag) != i) {
          return false;
        }
      }
      return true;
    }(),
    "kRenderResourceNames must be ordered by RenderResourceTag value");

constexpr const char *ToResourceName(RenderResourceTag resourceTag) {
  const size_t index = static_cast<size_t>(resourceTag);
  return index < kRenderResourceNames.size() ? kRenderResourceNames[index].name
                                              : "";
}

constexpr RenderResourceTag ToResourceTag(std::string_view resourceName) {
  for (const RenderResourceNameEntry &entry : kRenderResourceNames) {
    if (resourceName == entry.name) {
      return entry.tag;
    }
  }
  return RenderResourceTag::Custom;
}

inline constexpr std::array<const char *,
                            static_cast<size_t>(RenderOwnerTag::LightCulling) + 1>
    kRenderOwnerNames = {
        "Unknown",       // Unknown
        "Scene",         // Scene
        "Lighting",      // Lighting
        "HeightShadow",  // HeightShadow
        "OccluderExtract", // OccluderExtract
        "JFA",           // JFA
        "RadianceCascades", // RadianceCascades
        "GIComposite",   // GIComposite
        "FluidSimulation", // FluidSimulation
        "Volumetric",    // Volumetric
        "VFX",           // VFX
        "GPUText",       // GPUText
        "GPULoot",       // GPULoot
        "UIWorld",       // UIWorld
        "PostProcess",   // PostProcess
        "Distortion",    // Distortion
        "Composite",     // Composite
        "Shadow",        // Shadow
        "LightCulling",  // LightCulling
    };

constexpr const char *ToOwnerName(RenderOwnerTag ownerTag) {
  const size_t index = static_cast<size_t>(ownerTag);
  return index < kRenderOwnerNames.size() ? kRenderOwnerNames[index]
                                          : "Unknown";
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
// Phase-aware barrier declaration (B2 contract, 2026-08-03)
//
// A pass may contain several GPU sub-phases inside a single Execute (e.g. a
// compute dispatch followed by in-pass hybrid fragment draws). Pass-entry
// barriers (AddPassLocalBarrier) and cross-pass graph transitions fire before
// Execute and therefore CANNOT cover such same-pass phase transitions.
//
// AddPhaseBarrier(source, target, bits) declares, at Setup time, that the pass
// requires a GL memory barrier when moving from `sourcePhase` to `targetPhase`
// inside its own Execute. The pass then calls
// RenderContext::EmitPhaseBarrier(source, target) at exactly that execution
// point; RenderGraph resolves the declared bits and issues the barrier. The
// declaration is descriptive only -- it never binds GL resources or changes
// ownership.
// ---------------------------------------------------------------------------
struct PhaseBarrierDeclaration {
  PipelineStage sourcePhase = PipelineStage::Compute;
  PipelineStage targetPhase = PipelineStage::Fragment;
  uint32_t barrierBits = 0;
};

// Resource binding kinds a pass may declare as an OBSERVATION of how it will
// bind a resource during Execute. These declarations do not issue any GL calls
// and do not take ownership; they give the graph visibility over backing
// binding points (buffer binding points / image units / texture units) until a
// future phase migrates the manual BindBufferBase/BindImageTexture calls.
enum class ResourceBindingKind : uint8_t {
  BufferBase = 0,     // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, point, buffer)
  ImageUnit,          // glBindImageTexture(unit, texture, level, layered, layer, access, format)
  TextureUnit,        // glActiveTexture(unit) + glBindTexture (sampler input)
  ColorAttachment,    // framebuffer color attachment index
};

struct ResourceBindingDeclaration {
  RenderResourceTag resourceTag = RenderResourceTag::Custom;
  ResourceBindingKind kind = ResourceBindingKind::BufferBase;
  uint32_t point = 0;  // binding point / texture unit / attachment index
  uint32_t access = 0; // image-unit access (GL_READ_ONLY/WRITE_ONLY/READ_WRITE); 0 otherwise
  uint32_t format = 0; // image-unit internal format; 0 otherwise
};

// ---------------------------------------------------------------------------
// External backing import contract (blocker: shadow/cluster/LightBuffer GL
// backing ownership).
//
// Many typed resources are reached during a pass Execute through GL backings
// that are created, resized, and released OUTSIDE the graph (FramebufferManager
// framebuffers, ComputeBuffer SSBOs, LightManager/ClusteredLightingState
// buffers). A pass declares that fact at Setup time via ImportResource. The
// declaration is OBSERVER-ONLY and matches the BindBufferBase/BindImageUnit
// observation contract:
//
//   - the graph never allocates, resizes, frees, or GL-binds imported backing;
//   - ownership remains with the named backingOwner (the pass/state that calls
//     FramebufferManager::Create/Resize/Destroy, ComputeBuffer::Create/Release,
//     or LightManager buffer management);
//   - bindingPoint/imageUnit/imageAccess/imageFormat/colorAttachmentIndex
//     document the manual GL surface the pass currently reaches the backing
//     with, so a future phase can swap the manual binds for graph-driven ones
//     without changing ownership.
//
// Validation fails closed on contradictory imports (duplicate in a pass,
// conflicting kind/format across passes, Transient descriptors, descriptor
// mismatches) and warns on unresolved surfaces (unknown owner, mismatch with a
// BindBufferBase/BindImageUnit observation). Resize contract: exactly one of
// the resize* flags should reflect how the owner governs the backing extent
// (FramebufferManager screen-size FBOs follow screen resize; capacity-backed
// SSBOs follow light/cluster capacity growth). Zero in both flags is allowed
// for fixed-size backings.
// ---------------------------------------------------------------------------
struct ResourceImportInfo {
  RenderResourceTag resourceTag = RenderResourceTag::Custom;
  ResourceKind kind = ResourceKind::StorageBuffer;      // real GL object kind
  ResourceFormat format = ResourceFormat::Unknown;      // real backing format (textures)
  RenderOwnerTag backingOwner = RenderOwnerTag::Unknown; // non-graph creator/owner

  // Resize lifecycle contract (owner side; the graph never resizes).
  bool resizeFollowsScreen = false;   // owner recreates backing on screen resize
  bool resizeFollowsCapacity = false; // owner recreates backing on capacity growth

  // Binding surface this pass uses to reach the backing during Execute.
  uint32_t bindingPoint = 0;     // SSBO/UBO binding point or texture unit
  uint32_t imageUnit = 0;        // image unit (glBindImageTexture)
  uint32_t imageAccess = 0;      // image access (GL_READ_ONLY/WRITE_ONLY/READ_WRITE)
  uint32_t imageFormat = 0;      // image internal format (e.g. GL_RG16F)
  uint32_t colorAttachmentIndex = 0; // framebuffer color attachment index
};

// Resolved external backing supplied by the owner at frame execution time.
// This is a handle snapshot only: the graph does not own or release any of
// these GL objects. Zero handles are invalid and must fail closed.
struct ImportedBackingHandle {
  RenderResourceTag resourceTag = RenderResourceTag::Custom;
  uint32_t bufferHandle = 0;
  uint32_t textureHandle = 0;
  uint32_t framebufferHandle = 0;

  bool IsValidFor(ResourceKind kind) const {
    switch (kind) {
    case ResourceKind::StorageBuffer:
    case ResourceKind::UniformBuffer:
      return bufferHandle != 0;
    case ResourceKind::Texture2D:
    case ResourceKind::Texture2DArray:
      return textureHandle != 0;
    case ResourceKind::Framebuffer:
      return framebufferHandle != 0;
    default:
      return false;
    }
  }
};

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

  // Declares a same-pass phase transition that requires a GL memory barrier
  // inside Execute. The pass must emit it at the exact execution point via
  // RenderContext::EmitPhaseBarrier(sourcePhase, targetPhase).
  void AddPhaseBarrier(PipelineStage sourcePhase, PipelineStage targetPhase,
                       uint32_t barrierBits);

  // Observes how the pass binds a declared resource during Execute. These are
  // descriptive declarations only: they never issue GL calls nor change
  // resource ownership.
  void BindBufferBase(RenderResourceTag resourceTag, uint32_t bindingPoint);
  void BindImageUnit(RenderResourceTag resourceTag, uint32_t unit,
                     uint32_t access, uint32_t format);
  // Observer-only declarations for the remaining binding kinds. The B12
  // graph-driven execution layer treats them as unsupported (diagnostic only)
  // until a future phase; the manual surface inside Execute stays
  // authoritative.
  void BindTextureUnit(RenderResourceTag resourceTag, uint32_t textureUnit);
  void BindColorAttachment(RenderResourceTag resourceTag, uint32_t attachmentIndex);

  // Declares that a typed resource is reached through GL backing created and
  // owned outside the graph (see ResourceImportInfo). Observer-only: the graph
  // must never allocate, resize, free, or GL-bind imported backing.
  void ImportResource(const ResourceImportInfo &import);

  const std::vector<ResourceAccess> &GetAccesses() const { return m_accesses; }
  const std::vector<TypedPassAccess> &GetTypedAccesses() const { return m_typedAccesses; }
  const std::vector<TypedResourceDescriptor> &GetDeclaredDescriptors() const { return m_declaredDescriptors; }
  const std::vector<uint32_t> &GetPassLocalBarriers() const { return m_passLocalBarriers; }
  const std::vector<PhaseBarrierDeclaration> &GetPhaseBarriers() const {
    return m_phaseBarriers;
  }
  const std::vector<ResourceBindingDeclaration> &GetBindings() const {
    return m_bindings;
  }
  const std::vector<ResourceImportInfo> &GetImports() const {
    return m_imports;
  }

private:
  std::vector<ResourceAccess> m_accesses;
  std::vector<TypedPassAccess> m_typedAccesses;
  std::vector<TypedResourceDescriptor> m_declaredDescriptors;
  std::vector<uint32_t> m_passLocalBarriers;
  std::vector<PhaseBarrierDeclaration> m_phaseBarriers;
  std::vector<ResourceBindingDeclaration> m_bindings;
  std::vector<ResourceImportInfo> m_imports;
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

struct CompiledPhaseBarrier {
  size_t passIndex = 0;
  std::string passName;
  PipelineStage sourcePhase = PipelineStage::Compute;
  PipelineStage targetPhase = PipelineStage::Fragment;
  uint32_t barrierBits = 0;
};

struct CompiledResourceBinding {
  size_t passIndex = 0;
  std::string passName;
  std::string resourceName;
  ResourceBindingKind kind = ResourceBindingKind::BufferBase;
  uint32_t point = 0;
  uint32_t access = 0;
  uint32_t format = 0;
};

struct CompiledResourceImport {
  size_t passIndex = 0;
  std::string passName;
  std::string resourceName;
  RenderResourceTag resourceTag = RenderResourceTag::Custom;
  ResourceKind kind = ResourceKind::StorageBuffer;
  ResourceFormat format = ResourceFormat::Unknown;
  RenderOwnerTag backingOwner = RenderOwnerTag::Unknown;
  bool resizeFollowsScreen = false;
  bool resizeFollowsCapacity = false;
  uint32_t bindingPoint = 0;
  uint32_t imageUnit = 0;
  uint32_t imageAccess = 0;
  uint32_t imageFormat = 0;
  uint32_t colorAttachmentIndex = 0;
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
    std::vector<CompiledPhaseBarrier> phaseBarriers;
    std::vector<CompiledResourceBinding> bindings;
    std::vector<CompiledResourceImport> imports;
    std::vector<ValidationDiagnostic> diagnostics;

    std::string DumpPlan() const;
  };

  // -------------------------------------------------------------------------
  // B12 graph-driven binding admission/execution contract (2026-08-04)
  //
  // RenderGraph can resolve, for the pass currently executing, the real GL
  // binds that match its compiled binding declarations (BindBufferBase /
  // BindImageUnit) against the per-frame imported backing snapshot supplied by
  // the owners through RenderContext. The graph never allocates, resizes,
  // releases, or owns GL handles: handles are copied from the snapshot at
  // admission time and only the existing GPUUtils binding APIs are invoked.
  //
  // Admission is EXPLICIT and FAIL-CLOSED. A binding is executed only when ALL
  // of the following hold:
  //   - the pass declared a matching ImportResource for the same tag,
  //   - the import kind is compatible with the binding kind (tag/kind agree),
  //   - the RenderContext snapshot carries a non-zero handle for that tag and
  //     ImportedBackingHandle::IsValidFor(import.kind) returns true.
  // Anything missing, inconsistent, or zero-handed is DENIED: no GL bind is
  // issued, a validation/runtime diagnostic is recorded, and the caller
  // receives false. Unsupported binding kinds (TextureUnit / ColorAttachment)
  // are also denied with an explicit "unsupported" diagnostic. Manual binds
  // inside pass Execute remain the authoritative surface and stay untouched;
  // graph-driven binds are behavior-equivalent duplicates of them.
  // -------------------------------------------------------------------------
  struct ResolvedBindingOperation {
    enum class Kind : uint8_t {
      None = 0,
      BindBufferBase,   // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, point, handle)
      BindImageTexture, // glBindImageTexture(unit, handle, 0, false, 0, access, format)
      Unsupported,      // binding kind outside the B12 admission scope
    };

    Kind kind = Kind::None;
    RenderResourceTag resourceTag = RenderResourceTag::Custom;
    uint32_t point = 0;  // SSBO binding point or image unit
    uint32_t handle = 0; // real GL handle from the snapshot; NOT owned by the graph
    uint32_t access = 0; // image-unit access (BindImageTexture)
    uint32_t format = 0; // image internal format (BindImageTexture)
  };

  // Result of resolving a pass's binding declarations against the snapshot.
  // `operations` holds only ADMITTED operations in declaration order; denied
  // or unsupported bindings never appear as GL binds and are reported through
  // `diagnostics` + `allAdmitted == false`.
  struct BindingResolutionResult {
    std::vector<ResolvedBindingOperation> operations;
    std::vector<ValidationDiagnostic> diagnostics;
    bool allAdmitted = false; // true when every supported binding was admitted
  };

  void AddPass(std::shared_ptr<RenderPass> pass);
  void Clear();
  void Build();
  void Execute(RenderContext &context);
  void OnResize(int width, int height);

  // Phase D (RG-1): infers the owner of the pass that most recently declared a
  // typed Write to `resourceTag` (last writer in insertion/execution order),
  // or RenderOwnerTag::Unknown when no graph pass writes it. Requires the pass
  // accesses to be collected by Build(); returns Unknown on an unbuilt graph.
  // Replaces RenderSystem's manual sceneHdrOwner/ldrOwner tracking.
  RenderOwnerTag FindLastWriterOwner(RenderResourceTag resourceTag) const;
  static void SetValidationEnabled(bool enabled);
  static bool IsValidationEnabled();
  static void SetTransientAliasingEnabled(bool enabled);
  static bool IsTransientAliasingEnabled();

  // Emits the GL barrier declared via RenderGraphBuilder::AddPhaseBarrier(...)
  // for the pass currently executing. Only valid between the start and end of
  // a pass Execute driven by RenderGraph::Execute. Returns false (with an
  // error log) when the phase pair was never declared for the active pass.
  bool EmitActivePassPhaseBarrier(PipelineStage sourcePhase,
                                  PipelineStage targetPhase);

  // B12 graph-driven binding (see contract note above).
  //
  // Resolves the binding operations for the pass at `passIndex` by admitting
  // each compiled binding declaration against a matching import and a valid
  // snapshot handle. Pure resolution: no GL calls, no ownership change, no
  // graph state mutation — fully unit-testable without a GL context.
  BindingResolutionResult ResolvePassBindings(size_t passIndex,
                                              const RenderContext &context) const;

  // Same as ResolvePassBindings, but for the pass currently executing inside
  // RenderGraph::Execute. Fail-closed when no pass is active.
  BindingResolutionResult ResolveActivePassBindings(const RenderContext &context) const;

  // Executes the admitted binding operations of the currently active pass via
  // the existing GPUUtils binding APIs, immediately before pass Execute.
  // Denied / unsupported bindings are never bound and are recorded (and
  // logged) as runtime diagnostics. Returns true only when every supported
  // binding of the active pass was admitted and bound.
  bool ApplyActivePassBindings(RenderContext &context);

  // Runtime diagnostics recorded by ApplyActivePassBindings during the current
  // RenderGraph::Execute frame (accumulated across passes, cleared each frame).
  const std::vector<ValidationDiagnostic> &GetRuntimeBindingDiagnostics() const {
    return m_runtimeBindingDiagnostics;
  }

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
    RenderPassType passType = RenderPassType::Count;
    std::string passName;
    std::string canonicalPassName;
    size_t passIndex = 0;
    uint32_t stablePassId = 0;
    std::vector<ResourceAccess> accesses;
    std::vector<TypedPassAccess> typedAccesses;
    std::vector<TypedResourceDescriptor> declaredDescriptors;
    std::vector<uint32_t> passLocalBarriers;
    std::vector<PhaseBarrierDeclaration> phaseBarriers;
    std::vector<ResourceBindingDeclaration> bindings;
    std::vector<ResourceImportInfo> imports;
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
  size_t m_activeNodeIndex = static_cast<size_t>(-1);
  std::vector<ValidationDiagnostic> m_runtimeBindingDiagnostics;
  static bool s_validationEnabled;
  static bool s_transientAliasingEnabled;
};

} // namespace NoMoreDay::render::graph
