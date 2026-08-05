#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/BindingRegistry.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/lighting/ClusteredLightingState.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/passes/LightCullingPass.hpp"
#include "engine/render/passes/ShadowBuildPass.hpp"
#include "engine/render/passes/ShadowPreparePass.hpp"
#include "engine/render/passes/ShadowResolvePass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/FullscreenQuad.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "engine/resource/ResourceManager.hpp"

#include <entt/entt.hpp>

#include "raylib.h"
#include "rlgl.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// ===========================================================================
// Phase B B12 real-GL binding-equivalence fixture (2026-08-04)
//
// Purpose: unblock the B12 remaining gate ("real GL integration test proving
// graph-driven bind == manual bind equivalence") with a minimal contract-level
// fixture. This is NOT production visual evidence (that remains the
// NoMoreDay.exe --gpu-gate production gate) and is therefore grouped under the
// [GPU-Diagnostic] prefix: it runs in nmd.tests.gpu.diagnostic and is excluded
// from broad ci;nonperf / integration strata.
//
// The fixture builds a real hidden 1x1 GL context and drives the REAL
// RenderGraph::Execute path, which calls ApplyActivePassBindings before every
// pass Execute (RenderGraph.cpp:615). Two runs of the SAME compute shader with
// the SAME sentinel input data are compared:
//   - graph-driven-only: the pass declares BindBufferBase/BindImageUnit +
//     ImportResource; the graph admits them against the per-frame imported
//     backing snapshot and issues the binds via GPUUtils (the pass binds
//     nothing itself);
//   - manual-only: the same pass declares no bindings/imports (vacuous
//     admission, nothing graph-driven) and binds the same GL objects at the
//     same points manually inside Execute.
// Buffer and image data are read back and hashed; the hashes must match and
// must differ from the sentinel (proving the shader really wrote through the
// bound surfaces in both paths).
//
// The binding surface mirrors the real ShadowBuildPass contract: SSBO at
// ShadowCS::kOccluderBinding (binding point 15, GL_SHADER_STORAGE_BUFFER) and a
// GL_RG16F texture at ShadowCS::kSdfImageBinding (image unit 0, WRITE_ONLY).
// Resources use the real RenderResourceTag/OwnerTag pairs (ShadowOccluderSSBO /
// ShadowDistanceField owned by Shadow) so the resolved operations are identical
// in kind, point, access and format to the production pass.
//
// Fail-closed coverage: zero / missing / duplicate imported-backing snapshots
// must DENY the bind (no GPUUtils call), record the expected runtime
// diagnostic, and leave the test surface untouched (sentinel preserved).
// Registry leak check: the fixture only creates raw, unregistered GL objects;
// after cleanup the GPUResourceRegistry active record set (excluding the
// engine-owned GPUTimerQueryRing queries that Execute itself allocates) must be
// unchanged.
// ===========================================================================

namespace {

// ---- GL constants (matches the ShadowBuildPass/manual surface) ----
constexpr uint32_t kGLTexture2D = 0x0DE1;
constexpr uint32_t kGLShaderStorageBuffer = 0x90D2;
constexpr uint32_t kGLWriteOnly = 0x88B9;
constexpr uint32_t kGLRg16f = 0x822F;
constexpr uint32_t kGLRg = 0x8227;
constexpr uint32_t kGLFloat = 0x1406;
constexpr uint32_t kGLDynamicDraw = 0x88E8;
constexpr uint32_t kGLAllBarrierBits = 0xFFFFFFFF;

// Binding surface mirrors RenderConstants::ShadowCS (kOccluderBinding = 15,
// kSdfImageBinding = 0) so the resolved ops match the production pass.
constexpr uint32_t kBufferBinding = 15u;
constexpr uint32_t kImageUnit = 0u;

constexpr int kTexSize = 8;          // 8x8 image, 64 SSBO floats (8x8 workgroup)
constexpr int kFloatCount = kTexSize * kTexSize;
constexpr int kTexelChannelCount = 2;  // GL_RG readback of an RG16F image

constexpr float kSentinel = -9999.0f;
constexpr float kScale = 2.5f;

// ---- Minimal real GL context (same pattern as GPUHardwareValidationGateTest)
bool CreateMinimalGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "Graph Binding Equivalence GL Test Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return false;
  }
  // A compute fixture without GL 4.3 compute support is "unavailable", not a
  // pass or a failure.
  return NoMoreDay::utils::GPUUtils::CheckSupport().computeShaderSupported;
}

// FNV-1a 64 over raw bytes (deterministic data hash, no screenshot baseline).
uint64_t Fnv1a64(const void *data, size_t size) {
  const auto *bytes = static_cast<const uint8_t *>(data);
  uint64_t hash = 14695981039346656037ULL;
  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::vector<float> MakeSentinelFloats(int count) {
  return std::vector<float>(static_cast<size_t>(count), kSentinel);
}

// Compiles the equivalence compute shader (SSBO write at binding 15 + image
// write at unit 0). Returns 0 when compilation failed (a real failure on a
// compute-capable context).
uint32_t CompileEquivalenceComputeProgram() {
  const char *source =
      "#version 430\n"
      "layout(local_size_x = 8, local_size_y = 8) in;\n"
      "layout(std430, binding = 15) buffer EquivOutSSBO { float outData[]; };\n"
      "layout(binding = 0, rg16f) uniform writeonly image2D outImage;\n"
      "uniform float uScale;\n"
      "void main() {\n"
      "  uvec2 gid = gl_GlobalInvocationID.xy;\n"
      "  uint index = gid.y * 8u + gid.x;\n"
      "  outData[index] = uScale * (float(index) + 1.0);\n"
      "  imageStore(outImage, ivec2(gid),\n"
      "             vec4(uScale * (float(gid.x) + 1.0),\n"
      "                  uScale * (float(gid.y) + 1.0), 0.0, 1.0));\n"
      "}\n";
  const unsigned int shaderId = rlCompileShader(source, RL_COMPUTE_SHADER);
  if (shaderId == 0) {
    return 0;
  }
  const unsigned int programId = rlLoadComputeShaderProgram(shaderId);
  if (programId == 0) {
    return 0;
  }
  return programId;
}

// Creates a raw SSBO (unregistered) filled with sentinel data.
uint32_t CreateSentinelSsbo(const std::vector<float> &sentinel) {
  uint32_t buffer = 0;
  NoMoreDay::utils::GPUUtils::GenBuffers(1, &buffer);
  if (buffer == 0) {
    return 0;
  }
  NoMoreDay::utils::GPUUtils::BindBuffer(kGLShaderStorageBuffer, buffer);
  NoMoreDay::utils::GPUUtils::BufferData(
      kGLShaderStorageBuffer,
      static_cast<ptrdiff_t>(sentinel.size() * sizeof(float)),
      sentinel.data(), kGLDynamicDraw);
  return buffer;
}

// Creates a raw RG16F texture (unregistered) filled with sentinel data.
uint32_t CreateSentinelImageTexture(const std::vector<float> &sentinel) {
  uint32_t texture = 0;
  NoMoreDay::utils::GPUUtils::GenTextures(1, &texture);
  if (texture == 0) {
    return 0;
  }
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, texture);
  NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2D, 0x2801, 0x2600);  // MIN filter: NEAREST
  NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2D, 0x2800, 0x2600);  // MAG filter: NEAREST
  NoMoreDay::utils::GPUUtils::TexImage2D(
      kGLTexture2D, 0, static_cast<int>(kGLRg16f), kTexSize, kTexSize, 0,
      kGLRg, kGLFloat, sentinel.data());
  return texture;
}

// Refills an existing SSBO with the sentinel data (used between fail-closed
// sub-cases so a prior positive control cannot mask a failed bind).
void RefillSsboSentinel(uint32_t buffer, const std::vector<float> &sentinel) {
  NoMoreDay::utils::GPUUtils::BindBuffer(kGLShaderStorageBuffer, buffer);
  NoMoreDay::utils::GPUUtils::BufferSubData(
      kGLShaderStorageBuffer, 0,
      static_cast<ptrdiff_t>(sentinel.size() * sizeof(float)), sentinel.data());
}

// Reads the SSBO back into `out` (count floats).
void ReadbackSsbo(uint32_t buffer, std::vector<float> &out) {
  NoMoreDay::utils::GPUUtils::BindBuffer(kGLShaderStorageBuffer, buffer);
  NoMoreDay::utils::GPUUtils::GetBufferSubData(
      kGLShaderStorageBuffer, 0,
      static_cast<ptrdiff_t>(out.size() * sizeof(float)), out.data());
}

// Reads the RG16F texture back via glGetTexImage (GL_RG/GL_FLOAT).
bool ReadbackImageTexture(uint32_t texture, std::vector<float> &out) {
  using GlGetTexImageFn =
      void(APIENTRY *)(uint32_t, int, uint32_t, uint32_t, void *);
  auto glGetTexImage =
      reinterpret_cast<GlGetTexImageFn>(glfwGetProcAddress("glGetTexImage"));
  if (glGetTexImage == nullptr) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, texture);
  glGetTexImage(kGLTexture2D, 0, kGLRg, kGLFloat, out.data());
  return true;
}

// Test-local pass mirroring the ShadowBuildPass binding contract.
//
// Mode GraphDrivenOnly: Execute performs NO manual bind; it relies on the
// graph-driven binds that RenderGraph::Execute issues via
// ApplyActivePassBindings immediately before Execute.
// Mode ManualOnly: Execute manually binds the same handles at the same points.
// `declareBindings` controls whether Setup registers BindBufferBase /
// BindImageUnit + ImportResource (false => vacuous graph admission, so the
// graph-driven path binds nothing).
class BindingEquivalencePass : public NoMoreDay::render::graph::RenderPass {
public:
  enum class Mode { GraphDrivenOnly, ManualOnly };

  BindingEquivalencePass(bool declareBindings, Mode mode)
      : m_declareBindings(declareBindings), m_mode(mode) {}

  // Real GL handles owned by the fixture (raw, unregistered).
  uint32_t ssboHandle = 0;
  uint32_t textureHandle = 0;
  uint32_t computeProgram = 0;
  int scaleUniformLoc = -1;
  float scale = kScale;
  bool dispatchEnabled = true;

  void Setup(NoMoreDay::render::graph::RenderGraphBuilder &builder) override {
    using namespace NoMoreDay::render::graph;

    TypedResourceDescriptor ssboDesc;
    ssboDesc.name = "ShadowOccluderSSBO";
    ssboDesc.tag = RenderResourceTag::ShadowOccluderSSBO;
    ssboDesc.ownerTag = RenderOwnerTag::Shadow;
    ssboDesc.kind = ResourceKind::StorageBuffer;
    ssboDesc.lifetime = ResourceLifetime::Persistent;
    builder.DeclareResource(ssboDesc);

    TypedResourceDescriptor sdfDesc;
    sdfDesc.name = "ShadowDistanceField";
    sdfDesc.tag = RenderResourceTag::ShadowDistanceField;
    sdfDesc.ownerTag = RenderOwnerTag::Shadow;
    sdfDesc.kind = ResourceKind::Texture2D;
    sdfDesc.format = ResourceFormat::RG16F;
    sdfDesc.lifetime = ResourceLifetime::Persistent;
    builder.DeclareResource(sdfDesc);

    // The fixture compute shader WRITES both surfaces (SSBO outData + image
    // stores); each resource's first access must be a write so the single-pass
    // graph has a producer (no read-before-write).
    builder.Write(RenderResourceTag::ShadowOccluderSSBO, RenderOwnerTag::Shadow,
                  PipelineStage::Compute, ResourceUsage::StorageWrite);
    builder.Write(RenderResourceTag::ShadowDistanceField,
                  RenderOwnerTag::Shadow, PipelineStage::Compute,
                  ResourceUsage::StorageWrite);

    if (m_declareBindings) {
      builder.BindBufferBase(RenderResourceTag::ShadowOccluderSSBO,
                             kBufferBinding);
      builder.BindImageUnit(RenderResourceTag::ShadowDistanceField, kImageUnit,
                            kGLWriteOnly, kGLRg16f);

      ResourceImportInfo ssboImport;
      ssboImport.resourceTag = RenderResourceTag::ShadowOccluderSSBO;
      ssboImport.kind = ResourceKind::StorageBuffer;
      ssboImport.backingOwner = RenderOwnerTag::Shadow;
      ssboImport.bindingPoint = kBufferBinding;
      builder.ImportResource(ssboImport);

      ResourceImportInfo sdfImport;
      sdfImport.resourceTag = RenderResourceTag::ShadowDistanceField;
      sdfImport.kind = ResourceKind::Texture2D;
      sdfImport.format = ResourceFormat::RG16F;
      sdfImport.backingOwner = RenderOwnerTag::Shadow;
      sdfImport.imageUnit = kImageUnit;
      sdfImport.imageAccess = kGLWriteOnly;
      sdfImport.imageFormat = kGLRg16f;
      builder.ImportResource(sdfImport);
    }
  }

  void Execute(NoMoreDay::render::graph::RenderContext &) override {
    if (!dispatchEnabled || computeProgram == 0) {
      return;
    }
    if (m_mode == Mode::ManualOnly) {
      NoMoreDay::utils::GPUUtils::BindBufferBase(kBufferBinding, ssboHandle);
      NoMoreDay::utils::GPUUtils::BindImageTexture(
          kImageUnit, textureHandle, 0, false, 0, kGLWriteOnly, kGLRg16f);
    }
    rlEnableShader(computeProgram);
    if (scaleUniformLoc >= 0) {
      rlSetUniform(scaleUniformLoc, &scale, RL_SHADER_UNIFORM_FLOAT, 1);
    }
    NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(1u, 1u, 1u);
    rlDisableShader();
    // Make both the SSBO write and the image write visible to the readback
    // (glGetTexImage / glGetBufferSubData) deterministically.
    NoMoreDay::utils::GPUUtils::MemoryBarrier(kGLAllBarrierBits);
  }

  const char *GetName() const override { return "BindingEquivalencePass"; }

private:
  bool m_declareBindings;
  Mode m_mode;
};

// Drives one full graph Execute run and returns (ssboHash, imageHash, hashOk).
struct EquivalenceOutput {
  uint64_t ssboHash = 0;
  uint64_t imageHash = 0;
  bool readbackOk = false;
};

EquivalenceOutput RunBindingIteration(
    const std::vector<float> &sentinel, bool declareBindings,
    BindingEquivalencePass::Mode mode,
    const std::vector<NoMoreDay::render::graph::ImportedBackingHandle> &snapshot,
    uint32_t ssboHandle, uint32_t textureHandle, uint32_t computeProgram) {
  using namespace NoMoreDay::render::graph;

  EquivalenceOutput output = {};

  auto pass = std::make_shared<BindingEquivalencePass>(declareBindings, mode);
  pass->ssboHandle = ssboHandle;
  pass->textureHandle = textureHandle;
  pass->computeProgram = computeProgram;
  pass->scaleUniformLoc = rlGetLocationUniform(computeProgram, "uScale");
  pass->scale = kScale;

  RenderGraph graph;
  graph.AddPass(pass);
  graph.Build();

  RenderContext context = {};
  context.importedBackings = snapshot;

  graph.Execute(context);

  std::vector<float> bufferData = sentinel;
  ReadbackSsbo(ssboHandle, bufferData);
  output.ssboHash = Fnv1a64(bufferData.data(),
                            bufferData.size() * sizeof(float));

  std::vector<float> texelData(
      static_cast<size_t>(kTexelChannelCount * kTexSize * kTexSize), kSentinel);
  output.readbackOk = ReadbackImageTexture(textureHandle, texelData);
  output.imageHash =
      Fnv1a64(texelData.data(), texelData.size() * sizeof(float));

  return output;
}

// Registry leak check helper: count active records that are NOT owned by the
// engine-wide GPUTimerQueryRing (which RenderGraph::Execute allocates and never
// releases inside a test process).
size_t NonRingActiveResourceCount() {
  size_t count = 0;
  for (const auto &record :
       NoMoreDay::render::resources::GPUResourceRegistry::Get()
           .GetActiveResources()) {
    if (record.name.find("GPUTimerQueryRing") == std::string::npos) {
      ++count;
    }
  }
  return count;
}

}  // namespace

// ===========================================================================
// Real-GL equivalence: graph-driven-only vs manual-only on the same shader,
// same pass, same sentinel input. Output hashes must match, differ from the
// sentinel, and match the analytically expected values.
// ===========================================================================
TEST_CASE(
    "[GPU-Diagnostic] RenderGraph - B12 graph-driven vs manual binding real-GL equivalence") {
  using namespace NoMoreDay::render::graph;

  if (!CreateMinimalGpuContext()) {
    // Established GPU-fixture convention (no DOCTEST_SKIP in the vendored
    // doctest): an explicit "unavailable" failure, never a false pass.
    FAIL("Cannot create GPU context; skipping B12 graph-driven-vs-manual "
         "binding equivalence test (GL 4.3 compute unavailable on this host)");
  }

  const uint32_t computeProgram = CompileEquivalenceComputeProgram();
  REQUIRE(computeProgram != 0);

  const std::vector<float> sentinel = MakeSentinelFloats(kFloatCount);
  const uint64_t sentinelHash =
      Fnv1a64(sentinel.data(), sentinel.size() * sizeof(float));
  std::vector<float> sentinelTexels(
      static_cast<size_t>(kTexelChannelCount * kTexSize * kTexSize), kSentinel);
  const uint64_t sentinelImageHash =
      Fnv1a64(sentinelTexels.data(), sentinelTexels.size() * sizeof(float));

  const size_t registryActiveBefore = NonRingActiveResourceCount();

  uint32_t ssbo = CreateSentinelSsbo(sentinel);
  uint32_t texture = CreateSentinelImageTexture(sentinelTexels);
  REQUIRE(ssbo != 0);
  REQUIRE(texture != 0);

  // Per-frame imported backing snapshot carrying the real GL handles (the
  // graph copies them; it never owns them).
  std::vector<ImportedBackingHandle> snapshot = {
      {RenderResourceTag::ShadowOccluderSSBO, ssbo, 0u, 0u},
      {RenderResourceTag::ShadowDistanceField, 0u, texture, 0u},
  };

  // ---- Path A: graph-driven-only ----
  {
    auto pass =
        std::make_shared<BindingEquivalencePass>(true, BindingEquivalencePass::Mode::GraphDrivenOnly);
    pass->ssboHandle = ssbo;
    pass->textureHandle = texture;
    pass->computeProgram = computeProgram;
    pass->scaleUniformLoc = rlGetLocationUniform(computeProgram, "uScale");
    pass->scale = kScale;

    RenderGraph graph;
    graph.AddPass(pass);
    graph.Build();
    CHECK(!graph.HasValidationErrors());

    RenderContext context = {};
    context.importedBackings = snapshot;

    // Resolver contract on the real handles: exactly the two production-surface
    // operations (BufferBase at 15, ImageUnit at 0) are admitted.
    const auto resolution = graph.ResolvePassBindings(0u, context);
    REQUIRE(resolution.allAdmitted);
    REQUIRE(resolution.operations.size() == 2u);
    CHECK_EQ(resolution.operations[0].kind,
             RenderGraph::ResolvedBindingOperation::Kind::BindBufferBase);
    CHECK_EQ(resolution.operations[0].point, kBufferBinding);
    CHECK_EQ(resolution.operations[0].handle, ssbo);
    CHECK_EQ(resolution.operations[1].kind,
             RenderGraph::ResolvedBindingOperation::Kind::BindImageTexture);
    CHECK_EQ(resolution.operations[1].point, kImageUnit);
    CHECK_EQ(resolution.operations[1].handle, texture);
    CHECK_EQ(resolution.operations[1].access, kGLWriteOnly);
    CHECK_EQ(resolution.operations[1].format, kGLRg16f);

    graph.Execute(context);

    // The real Execute path (ApplyActivePassBindings before pass Execute) ran
    // with zero denied/unsupported bindings on this frame.
    CHECK(graph.GetRuntimeBindingDiagnostics().empty());

    std::vector<float> bufferData = sentinel;
    ReadbackSsbo(ssbo, bufferData);
    std::vector<float> texelData(
        static_cast<size_t>(kTexelChannelCount * kTexSize * kTexSize), kSentinel);
    const bool imageOk = ReadbackImageTexture(texture, texelData);

    const uint64_t bufferHash =
        Fnv1a64(bufferData.data(), bufferData.size() * sizeof(float));
    const uint64_t imageHash =
        Fnv1a64(texelData.data(), texelData.size() * sizeof(float));

    // The graph-driven binds alone must have been effective: output differs
    // from the sentinel (writes actually landed through the bound surfaces).
    CHECK(bufferHash != sentinelHash);
    CHECK(imageHash != sentinelImageHash);
    REQUIRE(imageOk);

    // And they must match the analytically expected values exactly.
    bool bufferMatches = true;
    for (int i = 0; i < kFloatCount; ++i) {
      if (std::fabs(bufferData[static_cast<size_t>(i)] -
                    kScale * (static_cast<float>(i) + 1.0f)) > 1e-3f) {
        bufferMatches = false;
      }
    }
    CHECK(bufferMatches);

    bool imageMatches = true;
    for (int y = 0; y < kTexSize; ++y) {
      for (int x = 0; x < kTexSize; ++x) {
        const size_t idx =
            static_cast<size_t>((y * kTexSize + x) * kTexelChannelCount);
        if (std::fabs(texelData[idx] - kScale * (static_cast<float>(x) + 1.0f)) >
                1e-3f ||
            std::fabs(texelData[idx + 1] -
                      kScale * (static_cast<float>(y) + 1.0f)) > 1e-3f) {
          imageMatches = false;
        }
      }
    }
    CHECK(imageMatches);
  }

  // ---- Path B: manual-only (no bindings/imports declared -> vacuous graph
  // admission; Execute binds the same handles at the same points manually).
  const EquivalenceOutput manualOutput = RunBindingIteration(
      sentinel, false, BindingEquivalencePass::Mode::ManualOnly,
      /* empty snapshot */ {}, ssbo, texture, computeProgram);
  REQUIRE(manualOutput.readbackOk);

  // ---- Equivalence: graph-driven-only must equal manual-only ----
  // (Re-run path A through the shared helper to produce a comparable hash.)
  const EquivalenceOutput graphDrivenOutput = RunBindingIteration(
      sentinel, true, BindingEquivalencePass::Mode::GraphDrivenOnly, snapshot,
      ssbo, texture, computeProgram);
  REQUIRE(graphDrivenOutput.readbackOk);

  CHECK_EQ(graphDrivenOutput.ssboHash, manualOutput.ssboHash);
  CHECK_EQ(graphDrivenOutput.imageHash, manualOutput.imageHash);
  // Non-triviality: neither path produced the untouched sentinel.
  CHECK(graphDrivenOutput.ssboHash != sentinelHash);
  CHECK(manualOutput.ssboHash != sentinelHash);
  CHECK(graphDrivenOutput.imageHash != sentinelImageHash);
  CHECK(manualOutput.imageHash != sentinelImageHash);

  // ---- Cleanup: raw GL objects released; registry active set unchanged ----
  NoMoreDay::utils::GPUUtils::DeleteBuffers(1, &ssbo);
  NoMoreDay::utils::GPUUtils::DeleteTextures(1, &texture);
  rlUnloadShaderProgram(computeProgram);

  const size_t registryActiveAfter = NonRingActiveResourceCount();
  CHECK_EQ(registryActiveAfter, registryActiveBefore);
}

// ===========================================================================
// Fail-closed on real GL: zero / missing / duplicate imported-backing
// snapshots must DENY the graph-driven bind inside the real Execute path,
// record the expected runtime diagnostic, and leave the test surface at its
// sentinel state (no GPUUtils bind ever touched it).
// ===========================================================================
TEST_CASE(
    "[GPU-Diagnostic] RenderGraph - B12 fail-closed snapshots never bind on real GL") {
  using namespace NoMoreDay::render::graph;

  if (!CreateMinimalGpuContext()) {
    // Established GPU-fixture convention (no DOCTEST_SKIP in the vendored
    // doctest): an explicit "unavailable" failure, never a false pass.
    FAIL("Cannot create GPU context; skipping B12 fail-closed snapshot binding "
         "test (GL 4.3 compute unavailable on this host)");
  }

  const uint32_t computeProgram = CompileEquivalenceComputeProgram();
  REQUIRE(computeProgram != 0);

  const std::vector<float> sentinel = MakeSentinelFloats(kFloatCount);
  std::vector<float> sentinelTexels(
      static_cast<size_t>(kTexelChannelCount * kTexSize * kTexSize), kSentinel);

  uint32_t ssbo = CreateSentinelSsbo(sentinel);
  uint32_t texture = CreateSentinelImageTexture(sentinelTexels);
  REQUIRE(ssbo != 0);
  REQUIRE(texture != 0);

  // Positive control first: with a valid snapshot the graph-driven bind is
  // admitted and the surface is overwritten (the fixture itself works).
  {
    std::vector<ImportedBackingHandle> validSnapshot = {
        {RenderResourceTag::ShadowOccluderSSBO, ssbo, 0u, 0u},
        {RenderResourceTag::ShadowDistanceField, 0u, texture, 0u},
    };
    auto pass = std::make_shared<BindingEquivalencePass>(
        true, BindingEquivalencePass::Mode::GraphDrivenOnly);
    pass->ssboHandle = ssbo;
    pass->textureHandle = texture;
    pass->computeProgram = computeProgram;
    pass->scaleUniformLoc = rlGetLocationUniform(computeProgram, "uScale");
    pass->scale = kScale;

    RenderGraph graph;
    graph.AddPass(pass);
    graph.Build();
    CHECK(!graph.HasValidationErrors());
    RenderContext context = {};
    context.importedBackings = validSnapshot;
    graph.Execute(context);
    CHECK(graph.GetRuntimeBindingDiagnostics().empty());

    std::vector<float> bufferData = sentinel;
    ReadbackSsbo(ssbo, bufferData);
    const uint64_t hash =
        Fnv1a64(bufferData.data(), bufferData.size() * sizeof(float));
    const uint64_t sentinelHash =
        Fnv1a64(sentinel.data(), sentinel.size() * sizeof(float));
    CHECK(hash != sentinelHash);
  }

  // ---- Fail-closed sub-case 1: zero handles in the snapshot ----
  {
    std::vector<ImportedBackingHandle> zeroSnapshot = {
        {RenderResourceTag::ShadowOccluderSSBO, 0u, 0u, 0u},
        {RenderResourceTag::ShadowDistanceField, 0u, 0u, 0u},
    };
    RefillSsboSentinel(ssbo, sentinel);
    auto pass = std::make_shared<BindingEquivalencePass>(
        true, BindingEquivalencePass::Mode::GraphDrivenOnly);
    pass->ssboHandle = ssbo;
    pass->textureHandle = texture;
    pass->computeProgram = computeProgram;
    pass->scaleUniformLoc = rlGetLocationUniform(computeProgram, "uScale");
    pass->scale = kScale;
    pass->dispatchEnabled = false;  // do not run the shader; surface must be untouched

    RenderGraph graph;
    graph.AddPass(pass);
    graph.Build();

    RenderContext context = {};
    context.importedBackings = zeroSnapshot;
    const auto resolution = graph.ResolvePassBindings(0u, context);
    CHECK_FALSE(resolution.allAdmitted);
    CHECK(resolution.operations.empty());  // zero handles must never become GL binds

    graph.Execute(context);

    // The real Execute path denied both bindings without issuing a GL bind.
    CHECK_FALSE(graph.GetRuntimeBindingDiagnostics().empty());
    size_t zeroHandleDenials = 0;
    for (const auto &diagnostic : graph.GetRuntimeBindingDiagnostics()) {
      if (diagnostic.severity ==
              RenderGraph::ValidationDiagnostic::Severity::Error &&
          diagnostic.message.find("zero/invalid handle") !=
              std::string::npos) {
        ++zeroHandleDenials;
      }
    }
    CHECK_EQ(zeroHandleDenials, 2u);

    // Surface untouched: no bind ever reached it.
    std::vector<float> bufferData = sentinel;
    ReadbackSsbo(ssbo, bufferData);
    bool untouched = true;
    for (const float value : bufferData) {
      if (value != kSentinel) {
        untouched = false;
      }
    }
    CHECK(untouched);
  }

  // ---- Fail-closed sub-case 2: missing snapshot for the image resource ----
  {
    std::vector<ImportedBackingHandle> partialSnapshot = {
        {RenderResourceTag::ShadowOccluderSSBO, ssbo, 0u, 0u},
        // ShadowDistanceField deliberately absent.
    };
    RefillSsboSentinel(ssbo, sentinel);
    auto pass = std::make_shared<BindingEquivalencePass>(
        true, BindingEquivalencePass::Mode::GraphDrivenOnly);
    pass->ssboHandle = ssbo;
    pass->textureHandle = texture;
    pass->computeProgram = computeProgram;
    pass->scaleUniformLoc = rlGetLocationUniform(computeProgram, "uScale");
    pass->scale = kScale;
    pass->dispatchEnabled = false;

    RenderGraph graph;
    graph.AddPass(pass);
    graph.Build();

    RenderContext context = {};
    context.importedBackings = partialSnapshot;
    const auto resolution = graph.ResolvePassBindings(0u, context);
    CHECK_FALSE(resolution.allAdmitted);
    REQUIRE(resolution.operations.size() == 1u);  // only the buffer binding admitted
    CHECK_EQ(resolution.operations[0].handle, ssbo);

    graph.Execute(context);

    bool missingDenied = false;
    for (const auto &diagnostic : graph.GetRuntimeBindingDiagnostics()) {
      if (diagnostic.severity ==
              RenderGraph::ValidationDiagnostic::Severity::Error &&
          diagnostic.resourceName == "ShadowDistanceField" &&
          diagnostic.message.find("no imported backing snapshot") !=
              std::string::npos) {
        missingDenied = true;
      }
    }
    CHECK(missingDenied);
  }

  // ---- Fail-closed sub-case 3: duplicate snapshot entries for one tag ----
  {
    std::vector<ImportedBackingHandle> duplicateSnapshot = {
        {RenderResourceTag::ShadowOccluderSSBO, ssbo, 0u, 0u},
        {RenderResourceTag::ShadowOccluderSSBO, ssbo + 1u, 0u, 0u},
        {RenderResourceTag::ShadowDistanceField, 0u, texture, 0u},
    };
    RefillSsboSentinel(ssbo, sentinel);
    auto pass = std::make_shared<BindingEquivalencePass>(
        true, BindingEquivalencePass::Mode::GraphDrivenOnly);
    pass->ssboHandle = ssbo;
    pass->textureHandle = texture;
    pass->computeProgram = computeProgram;
    pass->scaleUniformLoc = rlGetLocationUniform(computeProgram, "uScale");
    pass->scale = kScale;
    pass->dispatchEnabled = false;

    RenderGraph graph;
    graph.AddPass(pass);
    graph.Build();

    RenderContext context = {};
    context.importedBackings = duplicateSnapshot;
    graph.Execute(context);

    bool duplicateDenied = false;
    for (const auto &diagnostic : graph.GetRuntimeBindingDiagnostics()) {
      if (diagnostic.severity ==
              RenderGraph::ValidationDiagnostic::Severity::Error &&
          diagnostic.resourceName == "ShadowOccluderSSBO" &&
          diagnostic.message.find("multiple imported backing snapshots") !=
              std::string::npos) {
        duplicateDenied = true;
      }
    }
    CHECK(duplicateDenied);
  }

  NoMoreDay::utils::GPUUtils::DeleteBuffers(1, &ssbo);
  NoMoreDay::utils::GPUUtils::DeleteTextures(1, &texture);
  rlUnloadShaderProgram(computeProgram);
}

// ===========================================================================
// B2/B3 real production-path gate: real ShadowBuildPass graph-driven-only
// bind + same-pass phase barrier on real GL (2026-08-04, semantics converged
// 2026-08-05)
//
// The B12 fixture above proves the resolver/executor contract on a synthetic
// mirror pass. This gate goes one level deeper: it drives the REAL
// ShadowPreparePass -> ShadowBuildPass -> ShadowResolvePass chain through a
// REAL RenderGraph::Execute on a hidden 1x1 GL context with a REAL
// QualityTierManager config (High tier -> SDF shadow mode), and verifies:
//   - the compiled plan carries the B2 same-pass phase barrier declaration
//     (ShadowBuildPass Compute->Fragment, bits == Barrier::Image|Barrier::Buffer
//     == 0x220) and the graph-generated cross-pass transitions
//     (ShadowOccluderSSBO Host->Compute, ShadowDistanceField Compute->Fragment);
//   - graph-driven-only binding is the SOLE binding surface (the manual
//     BindBufferBase / BindImageTexture inside Execute are removed): with a
//     valid imported-backing snapshot (built from the real handles the pass
//     created on the previous frame, mirroring RenderSystem's frame-N snapshot
//     of frame N-1 backings) the resolver admits exactly the two production
//     operations (BindBufferBase @ ShadowCS::kOccluderBinding, BindImageTexture
//     @ ShadowCS::kSdfImageBinding WRITE_ONLY GL_RG16F) and the SDF/mask output
//     is non-sentinel and analytically correct (signed distance -16 at the
//     occluder center);
//   - fail-closed: an empty / zero-handle snapshot is DENIED (runtime
//     diagnostics, zero graph-issued binds) and ShadowBuildPass skips its
//     dispatch instead of rendering garbage through unbound surfaces (no manual
//     safety net remains to fall back to), leaving the SDF bit-identical to the
//     graph-driven frame;
//   - zero GL errors across the frames and no GPUResourceRegistry active-record
//     growth attributable to this test after teardown.
//
// ARTIFACT / GATE CLASSIFICATION: this is a [GPU-Diagnostic] contract-level
// per-pass binding + barrier gate. It is NOT production visual evidence (there
// is no screenshot/hash baseline and the production gate artifact remains NO_GO
// per B8), and it does NOT authorize any further production change; it is the
// graph-driven-only production-path verification for B2/B3.
// ===========================================================================
namespace {

constexpr uint32_t kGLRgba16f = 0x881A;
constexpr uint32_t kGLRgba = 0x1908;
constexpr int kShadowGateSize = 64;

// RGBA16F texture readback (GL_RGBA / GL_FLOAT) for the shadow mask.
bool ReadbackImageTextureRgba(uint32_t texture, std::vector<float> &out) {
  using GlGetTexImageFn = void(APIENTRY *)(uint32_t, int, uint32_t, uint32_t, void *);
  auto glGetTexImage =
      reinterpret_cast<GlGetTexImageFn>(glfwGetProcAddress("glGetTexImage"));
  if (glGetTexImage == nullptr) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, texture);
  glGetTexImage(kGLTexture2D, 0, kGLRgba, kGLFloat, out.data());
  return true;
}

using GlGetErrorFn = uint32_t(APIENTRY *)();
GlGetErrorFn GetGlErrorFn() {
  static GlGetErrorFn fn =
      reinterpret_cast<GlGetErrorFn>(glfwGetProcAddress("glGetError"));
  return fn;
}

uint32_t GlError() {
  GlGetErrorFn fn = GetGlErrorFn();
  return fn != nullptr ? fn() : 0u;
}

void DrainGlErrors() {
  while (GlError() != 0u) {
  }
}

int CountAndDrainGlErrors() {
  int count = 0;
  while (GlError() != 0u) {
    ++count;
  }
  return count;
}

// Temp settings for the real QualityTierManager path (same pattern as
// ShadowPipelineTierFallbackIntegrationTest).
std::filesystem::path MakeGateSettingsPath(const std::string &name) {
  const std::filesystem::path dir =
      std::filesystem::path("bin") / "tmp_shadow_gate";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir / name;
}

void WriteGateSettingsJson(const std::filesystem::path &path) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  REQUIRE(out.is_open());
  out << nlohmann::json(
              {{"renderQualityTier", "High"},
               {"render",
                {{"v3",
                  {{"enabled", true},
                   {"shadowEnabled", true},
                   {"shadowMode", "sdf"},
                   {"maxShadowedLights", 8},
                   {"shadowAtlasSize", 2048}}}}}})
              .dump(2);
}

// Hash of an all-sentinel buffer of the given float count (non-triviality
// control for the readbacks).
uint64_t HashAllSentinel(size_t floatCount) {
  std::vector<float> data(floatCount, kSentinel);
  return Fnv1a64(data.data(), data.size() * sizeof(float));
}

// Locates the compiled pass index by name (defensive against topological
// reorder).
size_t FindCompiledPassIndex(
    const NoMoreDay::render::graph::RenderGraph::CompiledRenderPlan &plan,
    const std::string &passName) {
  for (const auto &pass : plan.passes) {
    if (pass.passName == passName) {
      return pass.passIndex;
    }
  }
  return static_cast<size_t>(-1);
}

// Count of active registry records whose name contains `needle` (shared
// records such as the FullscreenQuad VAO are excluded from the leak delta).
size_t CountRegistryRecordsNamed(const std::string &needle) {
  size_t count = 0;
  for (const auto &record :
       NoMoreDay::render::resources::GPUResourceRegistry::Get()
           .GetActiveResources()) {
    if (record.name.find(needle) != std::string::npos) {
      ++count;
    }
  }
  return count;
}

// Leak-check baseline: active registry records that are not the engine-owned
// GPUTimerQueryRing (allocated per Execute, never released inside a test
// process). The "ResourceManager*" name filter remains as a cross-test safety
// margin for unrelated fixtures that may leave manager shader records behind.
// This fixture's own two records are unregistered by resources.unloadAll() in
// teardown (see the real ShadowBuildPass gate test case).
size_t LeakCheckResourceCount() {
  size_t count = 0;
  for (const auto &record :
       NoMoreDay::render::resources::GPUResourceRegistry::Get()
           .GetActiveResources()) {
    if (record.name.find("GPUTimerQueryRing") != std::string::npos) {
      continue;
    }
    if (record.name.find("ResourceManager") != std::string::npos) {
      continue;
    }
    ++count;
  }
  return count;
}

}  // namespace

// ===========================================================================
// Phase B B4 LightCulling per-pass GL gate helpers (2026-08-05)
//
// TEST_CASE "B4 contract" drives the REAL LightCullingPass through a real
// RenderGraph::Execute on the hidden 1x1 GL context and asserts the compiled
// plan surface (6 imports at the LightCulling binding-domain points 0-5, 6
// BindBufferBase observers the production Setup declares at the same points
// 0-5, LightBounds Host->Compute + 4 cluster Compute->Fragment transitions at
// 0x2000) plus real cluster readback determinism across empty/valid/zero
// imported-backing snapshots. Since the production manual binds are removed
// (2026-08-05), empty/zero snapshots FAIL CLOSED (the pass skips its dispatch;
// there is no manual safety net) and the valid snapshot is the sole rendering
// path, admitting exactly the 6 graph-driven operations.
//
// TEST_CASE "B4 6-point surface" uses a test-local LightCullingSurfacePass
// that mirrors the production Execute surface (6 descriptors, 6
// BindBufferBase observers at points 0-5, 6 ImportResource with the real
// Lighting/LightCulling owners). Its ManualOnly authority keeps the fixture's
// own reference baseline (the mirror pass binds manually inside Execute), so
// the graph-driven-only vs manual-only equivalence of the real 6-point
// BufferBase surface can still be proven on real GL (bit-identical readback
// hashes) at the executor level, independent of the removed production binds.
// ===========================================================================
namespace {

constexpr uint32_t kLightCullingStorageBarrierBit = 0x00002000u;
constexpr uint32_t kMaxClusteredLights = 4096u;

// Byte-level SSBO readback (counter / index / packed buffers).
bool ReadbackSsboBytes(uint32_t buffer, void *out, size_t bytes) {
  if (buffer == 0u || out == nullptr) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::BindBuffer(kGLShaderStorageBuffer, buffer);
  NoMoreDay::utils::GPUUtils::GetBufferSubData(
      kGLShaderStorageBuffer, 0, static_cast<ptrdiff_t>(bytes), out);
  return true;
}

// Deterministic cluster "counts" hash: per-cluster (pointCount, spotCount,
// areaCount). The header.offset field is EXCLUDED because the shader allocates
// it via atomicAdd(counter.writeCursor, ...) whose per-dispatch order is not
// deterministic; the three count fields are per-cluster deterministic.
uint64_t HashClusterCounts(
    const std::vector<NoMoreDay::components::GPUClusterHeader> &headers) {
  std::vector<uint32_t> counts;
  counts.reserve(headers.size() * 3u);
  for (const auto &header : headers) {
    counts.push_back(header.pointCount);
    counts.push_back(header.spotCount);
    counts.push_back(header.areaCount);
  }
  return Fnv1a64(counts.data(), counts.size() * sizeof(uint32_t));
}

// Reads the 32-byte cluster counter buffer (writeCursor + overflow fields;
// reserved words stay zero, so the whole 32B is deterministic).
uint64_t HashCounterBuffer(uint32_t counterBufferId) {
  NoMoreDay::components::GPUClusterCounters counter = {};
  if (!ReadbackSsboBytes(counterBufferId, &counter, sizeof(counter))) {
    return 0u;
  }
  return Fnv1a64(&counter, sizeof(counter));
}

// Reads index[0..written) — all written entries reference the single test
// light, so the content is deterministic regardless of the racy offsets.
uint64_t HashIndexRange(uint32_t indexBufferId, uint32_t written) {
  if (indexBufferId == 0u || written == 0u) {
    return 0u;
  }
  std::vector<uint8_t> bytes(static_cast<size_t>(written) * sizeof(uint32_t));
  if (!ReadbackSsboBytes(indexBufferId, bytes.data(), bytes.size())) {
    return 0u;
  }
  return Fnv1a64(bytes.data(), bytes.size());
}

// Reads packed[0..written) (64B per entry). All entries come from the same
// single light, so the content is deterministic.
uint64_t HashPackedRange(uint32_t packedBufferId, uint32_t written) {
  if (packedBufferId == 0u || written == 0u) {
    return 0u;
  }
  std::vector<uint8_t> bytes(static_cast<size_t>(written) *
                             sizeof(NoMoreDay::components::GPUClusterPackedLight));
  if (!ReadbackSsboBytes(packedBufferId, bytes.data(), bytes.size())) {
    return 0u;
  }
  return Fnv1a64(bytes.data(), bytes.size());
}

// Test-local consumer mirroring the B7 fragment side: reads the four cluster
// buffers in the Fragment stage (no GL work) so the graph generates the
// Compute->Fragment transitions with the storage barrier bits.
class ClusterConsumerPass : public NoMoreDay::render::graph::RenderPass {
public:
  void Setup(NoMoreDay::render::graph::RenderGraphBuilder &builder) override {
    using namespace NoMoreDay::render::graph;
    builder.Read(RenderResourceTag::ClusterHeaderSSBO, RenderOwnerTag::LightCulling,
                 PipelineStage::Fragment, ResourceUsage::StorageRead);
    builder.Read(RenderResourceTag::ClusterLightIndexSSBO, RenderOwnerTag::LightCulling,
                 PipelineStage::Fragment, ResourceUsage::StorageRead);
    builder.Read(RenderResourceTag::ClusterPackedLightSSBO, RenderOwnerTag::LightCulling,
                 PipelineStage::Fragment, ResourceUsage::StorageRead);
    builder.Read(RenderResourceTag::ClusterCounterSSBO, RenderOwnerTag::LightCulling,
                 PipelineStage::Fragment, ResourceUsage::StorageRead);
  }

  void Execute(NoMoreDay::render::graph::RenderContext &) override {}

  const char *GetName() const override { return "ClusterConsumerPass"; }
};

// Test-local mirror of the production LightCullingPass Execute surface, with a
// switchable binding authority so the same real shader/buffer setup can be
// driven graph-driven-only or manual-only. Setup declares the full 6-point
// BufferBase surface (points 0-5) plus the six real imports.
class LightCullingSurfacePass : public NoMoreDay::render::graph::RenderPass {
public:
  enum class BindingAuthority { ManualOnly, GraphDrivenOnly };

  explicit LightCullingSurfacePass(BindingAuthority authority)
      : m_bindingAuthority(authority) {}

  // Fail-closed control: when false, Execute skips the dispatch so a denied
  // graph-driven frame cannot write through a stale GL binding.
  bool dispatchEnabled = true;

  void Setup(NoMoreDay::render::graph::RenderGraphBuilder &builder) override {
    using namespace NoMoreDay::render::graph;

    const auto declareClusterBuffer = [&builder](RenderResourceTag tag,
                                                 const char *name) {
      TypedResourceDescriptor desc;
      desc.name = name;
      desc.tag = tag;
      desc.ownerTag = RenderOwnerTag::LightCulling;
      desc.kind = ResourceKind::StorageBuffer;
      desc.lifetime = ResourceLifetime::Persistent;
      builder.DeclareResource(desc);
    };
    declareClusterBuffer(RenderResourceTag::ClusterHeaderSSBO, "ClusterHeaderSSBO");
    declareClusterBuffer(RenderResourceTag::ClusterLightIndexSSBO, "ClusterLightIndexSSBO");
    declareClusterBuffer(RenderResourceTag::ClusterPackedLightSSBO, "ClusterPackedLightSSBO");
    declareClusterBuffer(RenderResourceTag::ClusterCounterSSBO, "ClusterCounterSSBO");
    declareClusterBuffer(RenderResourceTag::LightBoundsSSBO, "LightBoundsSSBO");

    TypedResourceDescriptor lightBufferDesc;
    lightBufferDesc.name = "LightBufferSSBO";
    lightBufferDesc.tag = RenderResourceTag::LightBufferSSBO;
    lightBufferDesc.ownerTag = RenderOwnerTag::Lighting;
    lightBufferDesc.kind = ResourceKind::StorageBuffer;
    lightBufferDesc.lifetime = ResourceLifetime::Persistent;
    builder.DeclareResource(lightBufferDesc);

    // Producer accesses (each first access is a write; LightBounds and
    // LightBuffer are uploaded by the host then read by the compute dispatch,
    // mirroring production).
    builder.Write(RenderResourceTag::ClusterHeaderSSBO, RenderOwnerTag::LightCulling,
                  PipelineStage::Compute, ResourceUsage::StorageWrite);
    builder.Write(RenderResourceTag::ClusterLightIndexSSBO, RenderOwnerTag::LightCulling,
                  PipelineStage::Compute, ResourceUsage::StorageWrite);
    builder.Write(RenderResourceTag::ClusterPackedLightSSBO, RenderOwnerTag::LightCulling,
                  PipelineStage::Compute, ResourceUsage::StorageWrite);
    builder.Write(RenderResourceTag::ClusterCounterSSBO, RenderOwnerTag::LightCulling,
                  PipelineStage::Compute, ResourceUsage::StorageWrite);
    builder.Write(RenderResourceTag::LightBoundsSSBO, RenderOwnerTag::LightCulling,
                  PipelineStage::Host, ResourceUsage::StorageWrite);
    builder.Read(RenderResourceTag::LightBoundsSSBO, RenderOwnerTag::LightCulling,
                 PipelineStage::Compute, ResourceUsage::StorageRead);
    builder.Write(RenderResourceTag::LightBufferSSBO, RenderOwnerTag::Lighting,
                  PipelineStage::Host, ResourceUsage::StorageWrite);
    builder.Read(RenderResourceTag::LightBufferSSBO, RenderOwnerTag::Lighting,
                 PipelineStage::Compute, ResourceUsage::StorageRead);

    // 6 BindBufferBase observers at the LightCulling binding-domain points.
    builder.BindBufferBase(RenderResourceTag::LightBufferSSBO, 0u);
    builder.BindBufferBase(RenderResourceTag::ClusterHeaderSSBO, 1u);
    builder.BindBufferBase(RenderResourceTag::ClusterLightIndexSSBO, 2u);
    builder.BindBufferBase(RenderResourceTag::LightBoundsSSBO, 3u);
    builder.BindBufferBase(RenderResourceTag::ClusterCounterSSBO, 4u);
    builder.BindBufferBase(RenderResourceTag::ClusterPackedLightSSBO, 5u);

    // 6 imports mirroring production ownership.
    const auto importClusterBuffer = [&builder](RenderResourceTag tag,
                                                uint32_t point) {
      ResourceImportInfo import;
      import.resourceTag = tag;
      import.kind = ResourceKind::StorageBuffer;
      import.backingOwner = RenderOwnerTag::LightCulling;
      import.resizeFollowsCapacity = true;
      import.bindingPoint = point;
      builder.ImportResource(import);
    };
    importClusterBuffer(RenderResourceTag::ClusterHeaderSSBO, 1u);
    importClusterBuffer(RenderResourceTag::ClusterLightIndexSSBO, 2u);
    importClusterBuffer(RenderResourceTag::ClusterPackedLightSSBO, 5u);
    importClusterBuffer(RenderResourceTag::ClusterCounterSSBO, 4u);
    importClusterBuffer(RenderResourceTag::LightBoundsSSBO, 3u);

    ResourceImportInfo lightBufferImport;
    lightBufferImport.resourceTag = RenderResourceTag::LightBufferSSBO;
    lightBufferImport.kind = ResourceKind::StorageBuffer;
    lightBufferImport.backingOwner = RenderOwnerTag::Lighting;
    lightBufferImport.resizeFollowsCapacity = true;
    lightBufferImport.bindingPoint = 0u;
    builder.ImportResource(lightBufferImport);
  }

  void Execute(NoMoreDay::render::graph::RenderContext &context) override {
    ++m_frameIndex;
    if (context.qualityManager == nullptr || context.camera == nullptr ||
        context.resources == nullptr) {
      return;
    }
    const auto &config = context.qualityManager->GetConfig();
    if (!config.v3Enabled || !config.dynamicLightingEnabled ||
        !config.clusteredLightingEnabled) {
      return;
    }
    if (!context.hdrSceneBuffer.IsValid()) {
      return;
    }
    if (!m_initialized && !Initialize(*context.resources)) {
      return;
    }

    auto &clusterState = NoMoreDay::render::lighting::ClusteredLightingState::Get();
    const auto grid =
        NoMoreDay::render::lighting::ClusteredLightingState::ComputeClusterGridDimensions(
            static_cast<uint32_t>(std::max(0, context.hdrSceneBuffer.width)),
            static_cast<uint32_t>(std::max(0, context.hdrSceneBuffer.height)),
            config.clusterTileSize, config.clusterZSliceCount);
    if (grid.clusterCount == 0u) {
      return;
    }

    const auto &records =
        NoMoreDay::render::lighting::LightManager::Get().GetActiveLightRecordsCpu();
    const uint32_t lightCount = static_cast<uint32_t>(records.size());
    if (!clusterState.BeginFrame(
            m_frameIndex,
            static_cast<uint32_t>(std::max(0, context.hdrSceneBuffer.width)),
            static_cast<uint32_t>(std::max(0, context.hdrSceneBuffer.height)),
            config.clusterTileSize, config.clusterZSliceCount, lightCount)) {
      return;
    }

    std::vector<NoMoreDay::components::GPULightBounds> bounds;
    bounds.reserve(static_cast<size_t>(lightCount));
    for (uint32_t i = 0; i < lightCount; ++i) {
      const auto &light = records[static_cast<size_t>(i)].gpuLight;
      const float minX = light.posX - light.radius;
      const float minY = light.posY - light.radius;
      const float maxX = light.posX + light.radius;
      const float maxY = light.posY + light.radius;
      const int32_t minLayer =
          NoMoreDay::render::lighting::ClusteredLightingState::MapWorldYToRenderLayer(
              minY, NoMoreDay::render::lighting::ClusteredLightingState::kDefaultLayerBandWorldUnits);
      const int32_t maxLayer =
          NoMoreDay::render::lighting::ClusteredLightingState::MapWorldYToRenderLayer(
              maxY, NoMoreDay::render::lighting::ClusteredLightingState::kDefaultLayerBandWorldUnits);
      bounds.push_back({
          .minXY = {minX, minY},
          .maxXY = {maxX, maxY},
          .minLayer = static_cast<float>(std::min(minLayer, maxLayer)),
          .maxLayer = static_cast<float>(std::max(minLayer, maxLayer)),
          .lightIndex = i,
          .reserved = static_cast<uint32_t>(records[static_cast<size_t>(i)].priority),
      });
    }
    if (!clusterState.UploadLightBounds(bounds)) {
      return;
    }

    uint32_t lightListBinding = 0u;
    uint32_t headerBinding = 0u;
    uint32_t indexBinding = 0u;
    uint32_t packedLightBinding = 0u;
    uint32_t boundsBinding = 0u;
    uint32_t counterBinding = 0u;
    if (!NoMoreDay::render::core::BindingRegistry::TryResolve(
            NoMoreDay::render::core::BindingDomain::LightCulling, "LIGHT_LIST_IN", lightListBinding) ||
        !NoMoreDay::render::core::BindingRegistry::TryResolve(
            NoMoreDay::render::core::BindingDomain::LightCulling, "CLUSTER_HEADER_OUT", headerBinding) ||
        !NoMoreDay::render::core::BindingRegistry::TryResolve(
            NoMoreDay::render::core::BindingDomain::LightCulling, "CLUSTER_INDEX_OUT", indexBinding) ||
        !NoMoreDay::render::core::BindingRegistry::TryResolve(
            NoMoreDay::render::core::BindingDomain::LightCulling, "CLUSTER_LIGHT_OUT", packedLightBinding) ||
        !NoMoreDay::render::core::BindingRegistry::TryResolve(
            NoMoreDay::render::core::BindingDomain::LightCulling, "LIGHT_BOUNDS_IN", boundsBinding) ||
        !NoMoreDay::render::core::BindingRegistry::TryResolve(
            NoMoreDay::render::core::BindingDomain::LightCulling, "CLUSTER_COUNTER", counterBinding)) {
      return;
    }

    const uint32_t lightBufferId =
        NoMoreDay::render::lighting::LightManager::Get().GetLightBufferId();
    if (lightBufferId == 0u) {
      return;
    }

    rlEnableShader(m_shader.id);
    const int clusterX = static_cast<int>(grid.tilesX);
    const int clusterY = static_cast<int>(grid.tilesY);
    const int clusterZ = static_cast<int>(grid.slicesZ);
    if (m_clusterGridXLoc >= 0) {
      rlSetUniform(m_clusterGridXLoc, &clusterX, RL_SHADER_UNIFORM_INT, 1);
    }
    if (m_clusterGridYLoc >= 0) {
      rlSetUniform(m_clusterGridYLoc, &clusterY, RL_SHADER_UNIFORM_INT, 1);
    }
    if (m_clusterGridZLoc >= 0) {
      rlSetUniform(m_clusterGridZLoc, &clusterZ, RL_SHADER_UNIFORM_INT, 1);
    }
    const float zoom = std::max(context.camera->zoom, 0.0001f);
    const float tileSizeWorld = static_cast<float>(config.clusterTileSize) / zoom;
    const float cameraOffset[2] = {
        context.camera->target.x - (context.camera->offset.x / zoom),
        context.camera->target.y - (context.camera->offset.y / zoom),
    };
    if (m_tileSizeWorldLoc >= 0) {
      rlSetUniform(m_tileSizeWorldLoc, &tileSizeWorld, RL_SHADER_UNIFORM_FLOAT, 1);
    }
    if (m_cameraOffsetLoc >= 0) {
      rlSetUniform(m_cameraOffsetLoc, cameraOffset, RL_SHADER_UNIFORM_VEC2, 1);
    }
    if (m_lightCountLoc >= 0) {
      const int lightCountInt = static_cast<int>(lightCount);
      rlSetUniform(m_lightCountLoc, &lightCountInt, RL_SHADER_UNIFORM_INT, 1);
    }
    if (m_maxLightsPerClusterLoc >= 0) {
      const int maxPerCluster =
          static_cast<int>(NoMoreDay::render::core::kMaxLightsPerCluster);
      rlSetUniform(m_maxLightsPerClusterLoc, &maxPerCluster, RL_SHADER_UNIFORM_INT, 1);
    }
    if (m_maxTotalClusteredLightsLoc >= 0) {
      const int maxTotal =
          static_cast<int>(NoMoreDay::render::core::kMaxTotalClusteredLights);
      rlSetUniform(m_maxTotalClusteredLightsLoc, &maxTotal, RL_SHADER_UNIFORM_INT, 1);
    }

    if (m_bindingAuthority == BindingAuthority::ManualOnly) {
      NoMoreDay::utils::GPUUtils::BindBufferBase(lightListBinding, lightBufferId);
      NoMoreDay::utils::GPUUtils::BindBufferBase(headerBinding,
                                                 clusterState.GetClusterHeaderBufferId());
      NoMoreDay::utils::GPUUtils::BindBufferBase(indexBinding,
                                                 clusterState.GetClusterLightIndexBufferId());
      NoMoreDay::utils::GPUUtils::BindBufferBase(packedLightBinding,
                                                 clusterState.GetClusterPackedLightBufferId());
      NoMoreDay::utils::GPUUtils::BindBufferBase(boundsBinding,
                                                 clusterState.GetLightBoundsBufferId());
      NoMoreDay::utils::GPUUtils::BindBufferBase(counterBinding,
                                                 clusterState.GetCounterBufferId());
    }

    if (dispatchEnabled) {
      NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
          (grid.tilesX + 7u) / 8u, (grid.tilesY + 7u) / 8u,
          (grid.slicesZ + 0u) / 1u);
    }
    rlDisableShader();
    // Host readback sync (mirrors production: the explicit storage barrier is
    // retained for ReadBackClusterHeaders determinism).
    NoMoreDay::utils::GPUUtils::MemoryBarrier(kLightCullingStorageBarrierBit);
    clusterState.ReadBackClusterHeaders();
  }

  const char *GetName() const override { return "LightCullingSurfacePass"; }

private:
  bool Initialize(::ResourceManager &resources) {
    if (m_initialized) {
      return true;
    }
    using namespace entt::literals;
    m_shader = resources.loadComputeShader("light_culling_compute"_hs,
                                           "assets/shaders/lighting/light_culling.comp");
    if (m_shader.id == 0) {
      return false;
    }
    m_clusterGridXLoc = rlGetLocationUniform(m_shader.id, "uClusterGridX");
    m_clusterGridYLoc = rlGetLocationUniform(m_shader.id, "uClusterGridY");
    m_clusterGridZLoc = rlGetLocationUniform(m_shader.id, "uClusterGridZ");
    m_tileSizeWorldLoc = rlGetLocationUniform(m_shader.id, "uTileSizeWorld");
    m_cameraOffsetLoc = rlGetLocationUniform(m_shader.id, "uCameraOffset");
    m_lightCountLoc = rlGetLocationUniform(m_shader.id, "uLightCount");
    m_maxLightsPerClusterLoc =
        rlGetLocationUniform(m_shader.id, "uMaxLightsPerCluster");
    m_maxTotalClusteredLightsLoc =
        rlGetLocationUniform(m_shader.id, "uMaxTotalClusteredLights");
    m_initialized = true;
    return true;
  }

  BindingAuthority m_bindingAuthority;
  Shader m_shader = {};
  int m_clusterGridXLoc = -1;
  int m_clusterGridYLoc = -1;
  int m_clusterGridZLoc = -1;
  int m_tileSizeWorldLoc = -1;
  int m_cameraOffsetLoc = -1;
  int m_lightCountLoc = -1;
  int m_maxLightsPerClusterLoc = -1;
  int m_maxTotalClusteredLightsLoc = -1;
  uint32_t m_frameIndex = 0;
  bool m_initialized = false;
};

}  // namespace

TEST_CASE(
    "[GPU-Diagnostic] RenderGraph - real ShadowBuildPass graph-driven bind + "
    "phase barrier gate on real GL") {
  using namespace NoMoreDay;
  using namespace NoMoreDay::render::graph;

  if (!CreateMinimalGpuContext()) {
    // Established GPU-fixture convention (no DOCTEST_SKIP in the vendored
    // doctest): an explicit "unavailable" failure, never a false pass.
    FAIL("Cannot create GPU context; skipping real ShadowBuildPass gate (GL "
         "4.3 compute unavailable on this host)");
  }

  // --- Real production config through the real QualityTierManager singleton:
  // High tier maps to SDF shadow mode, which exercises the B2/B3 binding +
  // phase-barrier surface without the Hybrid atlas-tile path. This mutates the
  // global config exactly like the existing ShadowPipelineTierFallback
  // integration test (accepted; no restore).
  const auto settingsPath = MakeGateSettingsPath("shadow_build_gate.json");
  WriteGateSettingsJson(settingsPath);
  auto &qm = render::core::QualityTierManager::Get();
  qm.Initialize(settingsPath.string(), true);
  qm.ForceTier(render::core::QualityTier::High);
  REQUIRE(qm.GetConfig().v3Enabled);
  REQUIRE(qm.GetConfig().shadowEnabled);
  REQUIRE(qm.GetConfig().shadowMode == render::core::ShadowMode::SDF);

  entt::registry registry;
  ResourceManager resources;

  // Registry baseline measured BEFORE creating any backing owned by this test
  // (the shared FullscreenQuad VAO record is tracked separately below).
  const size_t quadBefore = CountRegistryRecordsNamed("FullscreenQuadVAO");
  const size_t nonQuadBefore = LeakCheckResourceCount() - quadBefore;

  auto hdr = render::resources::FramebufferManager::Create(
      kShadowGateSize, kShadowGateSize, kGLRgba16f, false);
  REQUIRE(hdr.IsValid());

  Camera2D camera = {};
  camera.target = {32.0f, 32.0f};
  camera.offset = {0.0f, 0.0f};
  camera.zoom = 1.0f;

  // One occluder centered on the camera target: with offset=(0,0)/zoom=1 the
  // SDF shader maps the center texel to world (63.5,63.5)
  // (uv=(31.5,31.5)/64), radius 16 => signed distance -16 at the center and
  // positive away from it (both branches of the SDF are exercised).
  NoMoreDay::components::GPUShadowCaster occluders[1] = {};
  occluders[0].posX = 63.5f;
  occluders[0].posY = 63.5f;
  occluders[0].radius = 16.0f;
  occluders[0].occluderHeight = 8.0f;

  uint64_t sdfAdmittedHash = 0;
  uint64_t maskAdmittedHash = 0;

  DrainGlErrors();
  {
    auto preparePass = std::make_shared<render::passes::ShadowPreparePass>();
    auto buildPass = std::make_shared<render::passes::ShadowBuildPass>();
    auto resolvePass = std::make_shared<render::passes::ShadowResolvePass>();
    buildPass->SetPreparePass(preparePass.get());
    resolvePass->SetBuildPass(buildPass.get());
    REQUIRE(buildPass->Initialize(resources));

    RenderGraph graph;
    graph.AddPass(preparePass);
    graph.AddPass(buildPass);
    graph.AddPass(resolvePass);
    graph.Build();
    REQUIRE(!graph.HasValidationErrors());

    const auto &plan = graph.GetCompiledPlan();

    // ---- B2 same-pass phase barrier declaration: Compute->Fragment with the
    // Image|Buffer bits (0x220) exported by BuildCompiledPlan. ----
    {
      bool found = false;
      for (const auto &pb : plan.phaseBarriers) {
        if (pb.passName == "ShadowBuildPass" &&
            pb.sourcePhase == PipelineStage::Compute &&
            pb.targetPhase == PipelineStage::Fragment) {
          found = true;
          CHECK_EQ(pb.barrierBits,
                   static_cast<uint32_t>(RenderConstants::Barrier::Image) |
                       static_cast<uint32_t>(RenderConstants::Barrier::Buffer));
        }
      }
      REQUIRE(found);
    }

    // ---- Graph-generated cross-pass transitions. ----
    {
      bool occluderTransition = false;
      bool sdfTransition = false;
      for (const auto &transition : plan.transitions) {
        if (transition.resourceName == "ShadowOccluderSSBO" &&
            transition.previousStage == PipelineStage::Host &&
            transition.nextStage == PipelineStage::Compute) {
          occluderTransition = true;
          CHECK(transition.barrierBits != 0u);
        }
        if (transition.resourceName == "ShadowDistanceField" &&
            transition.previousStage == PipelineStage::Compute &&
            transition.nextStage == PipelineStage::Fragment) {
          sdfTransition = true;
          CHECK(transition.barrierBits != 0u);
        }
      }
      REQUIRE(occluderTransition);
      REQUIRE(sdfTransition);
    }

    // ---- Observer declarations landed in the compiled plan (2 bindings / 3
    // imports for ShadowBuildPass; 1 import for ShadowResolvePass). ----
    {
      size_t bindingCount = 0;
      size_t buildImportCount = 0;
      for (const auto &binding : plan.bindings) {
        if (binding.passName == "ShadowBuildPass") {
          ++bindingCount;
        }
      }
      for (const auto &import : plan.imports) {
        if (import.passName == "ShadowBuildPass") {
          ++buildImportCount;
        }
      }
      CHECK_EQ(bindingCount, 2u);
      CHECK_EQ(buildImportCount, 3u);
    }

    const size_t buildPassIndex =
        FindCompiledPassIndex(plan, "ShadowBuildPass");
    REQUIRE(buildPassIndex != static_cast<size_t>(-1));

    // ---- Frame A: empty snapshot -> the resolver denies the 2-point surface
    // (allAdmitted false, zero operations, 2 "no imported backing snapshot"
    // diagnostics) and ShadowBuildPass FAILS CLOSED: it skips the SDF dispatch
    // because the graph-driven binding was not admitted. The manual binds are
    // removed, so there is no safety net to fall back to; no GL bind ever
    // reached the surfaces and no output is produced. ShadowResolvePass follows
    // (build stage reported a failure), so the mask is not created either. ----
    {
      RenderContext context = {};
      context.registry = &registry;
      context.resources = &resources;
      context.qualityManager = &qm;
      context.camera = &camera;
      context.hdrSceneBuffer = hdr;
      context.occluders = occluders;
      context.occluderCount = 1u;

      const auto resolution =
          graph.ResolvePassBindings(buildPassIndex, context);
      CHECK_FALSE(resolution.allAdmitted);
      CHECK(resolution.operations.empty());

      graph.Execute(context);

      size_t missingDenials = 0;
      for (const auto &diagnostic : graph.GetRuntimeBindingDiagnostics()) {
        if (diagnostic.severity ==
                RenderGraph::ValidationDiagnostic::Severity::Error &&
            diagnostic.message.find("no imported backing snapshot") !=
                std::string::npos) {
          ++missingDenials;
        }
      }
      CHECK_EQ(missingDenials, 2u);

      // Fail-closed: no dispatch ran, the pass reports the denial instead of
      // rendering garbage through unbound surfaces.
      CHECK(buildPass->DidFailThisFrame());
      CHECK_FALSE(buildPass->SucceededThisFrame());
      // Backing objects are created before the guard (OnResize /
      // UploadOccluders), so the handles exist even though nothing was
      // dispatched; the resolve pass never created the mask.
      CHECK(buildPass->HasSdfField());
      CHECK_FALSE(resolvePass->HasShadowMask());
    }

    // ---- Frame B: valid per-frame imported-backing snapshot built from the
    // REAL handles the pass created during frame A (mirrors RenderSystem's
    // frame-N snapshot of frame N-1 backings). The resolver must admit exactly
    // the two production-surface operations and Execute must run with zero
    // runtime diagnostics. This is now the ONLY rendering path (manual binds
    // are removed), so the graph-driven output itself is the correctness
    // baseline: non-sentinel and analytically correct. The ShadowMask snapshot
    // entry still carries zero handles here (the mask is only created once the
    // build stage succeeds), but no pass declares a binding observer for
    // ShadowMask, so the resolver never consults that entry. ----
    const uint32_t occluderHandle = buildPass->GetOccluderBufferId();
    const uint32_t sdfTextureHandle = buildPass->GetSdfImageTexture();
    REQUIRE(occluderHandle != 0u);
    REQUIRE(sdfTextureHandle != 0u);

    std::vector<ImportedBackingHandle> validSnapshot = {
        {RenderResourceTag::ShadowOccluderSSBO, occluderHandle, 0u, 0u},
        {RenderResourceTag::ShadowDistanceField, 0u, sdfTextureHandle, 0u},
        {RenderResourceTag::ShadowMask, 0u, resolvePass->GetShadowMaskTexture(),
         resolvePass->GetShadowMaskFramebuffer()},
    };
    if (buildPass->HasShadowAtlas()) {
      validSnapshot.push_back(
          {RenderResourceTag::ShadowAtlas, 0u,
           buildPass->GetShadowAtlasTexture(),
           buildPass->GetShadowAtlasFramebuffer()});
    }

    {
      RenderContext context = {};
      context.registry = &registry;
      context.resources = &resources;
      context.qualityManager = &qm;
      context.camera = &camera;
      context.hdrSceneBuffer = hdr;
      context.occluders = occluders;
      context.occluderCount = 1u;
      context.importedBackings = validSnapshot;

      const auto resolution =
          graph.ResolvePassBindings(buildPassIndex, context);
      REQUIRE(resolution.allAdmitted);
      REQUIRE(resolution.operations.size() == 2u);
      CHECK_EQ(resolution.operations[0].kind,
               RenderGraph::ResolvedBindingOperation::Kind::BindBufferBase);
      CHECK_EQ(resolution.operations[0].point,
               RenderConstants::ShadowCS::kOccluderBinding);
      CHECK_EQ(resolution.operations[0].handle, occluderHandle);
      CHECK_EQ(resolution.operations[1].kind,
               RenderGraph::ResolvedBindingOperation::Kind::BindImageTexture);
      CHECK_EQ(resolution.operations[1].point,
               RenderConstants::ShadowCS::kSdfImageBinding);
      CHECK_EQ(resolution.operations[1].handle, sdfTextureHandle);
      CHECK_EQ(resolution.operations[1].access, kGLWriteOnly);
      CHECK_EQ(resolution.operations[1].format, kGLRg16f);

      graph.Execute(context);

      CHECK(graph.GetRuntimeBindingDiagnostics().empty());
      CHECK(buildPass->SucceededThisFrame());
      CHECK(resolvePass->HasShadowMask());

      std::vector<float> sdfData(
          static_cast<size_t>(2 * kShadowGateSize * kShadowGateSize),
          kSentinel);
      REQUIRE(ReadbackImageTexture(buildPass->GetSdfImageTexture(), sdfData));
      std::vector<float> maskData(
          static_cast<size_t>(4 * kShadowGateSize * kShadowGateSize),
          kSentinel);
      REQUIRE(ReadbackImageTextureRgba(resolvePass->GetShadowMaskTexture(),
                                       maskData));

      sdfAdmittedHash =
          Fnv1a64(sdfData.data(), sdfData.size() * sizeof(float));
      maskAdmittedHash =
          Fnv1a64(maskData.data(), maskData.size() * sizeof(float));
      // Non-triviality: the graph-driven binds alone were effective — the
      // surfaces were really written.
      CHECK_NE(sdfAdmittedHash, HashAllSentinel(sdfData.size()));
      CHECK_NE(maskAdmittedHash, HashAllSentinel(maskData.size()));

      // Analytic correctness: the signed-distance field must reach exactly
      // -radius at the occluder center (world (63.5,63.5); with
      // target=(32,32)/zoom=1 that is texel (31,31)). The minimum over the
      // whole field is -16.0 (every other texel sits at or outside the radius
      // boundary), which proves the shader read the bound occluder SSBO and
      // wrote through the bound image unit.
      float sdfMin = 1e10f;
      for (int i = 0; i < kShadowGateSize * kShadowGateSize; ++i) {
        sdfMin = std::min(sdfMin, sdfData[static_cast<size_t>(i) * 2u]);
      }
      CHECK(std::fabs(sdfMin - (-16.0f)) < 1e-3f);
    }

    // ---- Frame C: zero-handle snapshot -> the resolver fails closed (no
    // operations, allAdmitted false) and the runtime path records the expected
    // denial; ShadowBuildPass skips its dispatch (fail closed, no garbage), so
    // the SDF surface stays bit-identical to the graph-driven frame B. ----
    {
      std::vector<ImportedBackingHandle> zeroSnapshot = {
          {RenderResourceTag::ShadowOccluderSSBO, 0u, 0u, 0u},
          {RenderResourceTag::ShadowDistanceField, 0u, 0u, 0u},
      };
      RenderContext context = {};
      context.registry = &registry;
      context.resources = &resources;
      context.qualityManager = &qm;
      context.camera = &camera;
      context.hdrSceneBuffer = hdr;
      context.occluders = occluders;
      context.occluderCount = 1u;
      context.importedBackings = zeroSnapshot;

      const auto resolution =
          graph.ResolvePassBindings(buildPassIndex, context);
      CHECK_FALSE(resolution.allAdmitted);
      CHECK(resolution.operations.empty());

      graph.Execute(context);

      size_t zeroHandleDenials = 0;
      for (const auto &diagnostic : graph.GetRuntimeBindingDiagnostics()) {
        if (diagnostic.severity ==
                RenderGraph::ValidationDiagnostic::Severity::Error &&
            diagnostic.message.find("zero/invalid handle") !=
                std::string::npos) {
          ++zeroHandleDenials;
        }
      }
      CHECK_EQ(zeroHandleDenials, 2u);
      CHECK(buildPass->DidFailThisFrame());
      CHECK_FALSE(buildPass->SucceededThisFrame());

      // No garbage: the denied frame never dispatched, so the SDF surface is
      // exactly what the graph-driven frame B wrote.
      std::vector<float> sdfData(
          static_cast<size_t>(2 * kShadowGateSize * kShadowGateSize),
          kSentinel);
      REQUIRE(ReadbackImageTexture(buildPass->GetSdfImageTexture(), sdfData));
      const uint64_t sdfDeniedHash =
          Fnv1a64(sdfData.data(), sdfData.size() * sizeof(float));
      CHECK_EQ(sdfDeniedHash, sdfAdmittedHash);
    }

    // ---- GL error surface: the graph-driven frames introduced no GL errors. ----
    CHECK_EQ(CountAndDrainGlErrors(), 0);

    // ---- Teardown (real owners release their backings). ----
    buildPass->Shutdown();
    resolvePass->Shutdown();
  }  // graph + shared_ptr passes released here (destructors call Shutdown
     // again; idempotent)

  // Ownership teardown: ShadowBuildPass::Shutdown only drops its references
  // (it borrows manager-owned shaders; the pass must never UnloadShader them).
  // The ResourceManager remains the SOLE GL releaser: unloadAll() unregisters
  // the registry records and UnloadShaders each program exactly once. The
  // release ledger asserts both shaders were released — real ownership
  // teardown, not the old SetHeadless() escape hatch (which masked a
  // production double-free: pass Shutdown + unloadAll both calling
  // UnloadShader on the same program -> shader.locs heap corruption).
  resources.unloadAll();
  CHECK_EQ(resources.GetShaderReleaseCount(), 2u);

  render::resources::FramebufferManager::Destroy(hdr);
  render::resources::FullscreenQuad::Shutdown();

  // ---- Registry snapshot: no active-record growth attributable to this test
  // after teardown (shared FullscreenQuad VAO tracked separately; engine-owned
  // GPUTimerQueryRing excluded by LeakCheckResourceCount; this fixture's
  // ResourceManager shader records were unregistered by unloadAll above). ----
  const size_t quadAfter = CountRegistryRecordsNamed("FullscreenQuadVAO");
  const size_t nonQuadAfter = LeakCheckResourceCount() - quadAfter;
  CHECK_EQ(nonQuadAfter, nonQuadBefore);
}

// ===========================================================================
// Phase B B4 contract gate: the REAL LightCullingPass through a real
// RenderGraph::Execute on the hidden 1x1 GL context (2026-08-05).
//
// Drives the real production pass (Setup + Execute) with a real
// QualityTierManager clustered config, a real LightManager light upload and the
// real ClusteredLightingState buffers, then verifies:
//   - the compiled plan carries the 6 LightCulling binding-domain imports at
//     points 0-5 (LightBufferSSBO owner Lighting; the 5 cluster buffers owner
//     LightCulling; all resizeFollowsCapacity), the LightBounds Host->Compute
//     transition and the 4 cluster Compute->Fragment transitions at barrierBits
//     == 0x00002000, and the 6 BindBufferBase observers the production Setup
//     now declares at the same points 0-5 (matching the import binding points
//     and the BindingRegistry LightCulling symbols);
//   - graph-driven-only binding is the SOLE binding surface (the 6 manual
//     BindBufferBase calls inside Execute are removed): on an empty
//     imported-backing snapshot the resolver fails closed (6 "no imported
//     backing snapshot" denials) and the pass FAILS CLOSED (no dispatch, no
//     cluster data — no manual safety net remains); on a valid snapshot of the
//     real handles the resolver admits EXACTLY the 6 graph-driven BufferBase
//     operations at points 0-5 with the real handles, zero runtime
//     diagnostics, and the cluster counts are deterministic and non-trivial;
//     a zero-handle snapshot fails closed again and leaves the cluster counts
//     bit-identical (no garbage written);
//   - zero GL errors and no registry growth after teardown.
//
// This is a contract-level per-pass gate, NOT production visual evidence (no
// 1280x720 GO is fabricated, no 1x1 contract fixture is treated as visual
// proof). Deterministic assertions exclude header.offset (allocated by
// atomicAdd across the dispatch; the per-cluster counts are deterministic).
// ===========================================================================
TEST_CASE(
    "[GPU-Diagnostic] RenderGraph - B4 real LightCullingPass contract gate on "
    "real GL") {
  using namespace NoMoreDay;
  using namespace NoMoreDay::render::graph;

  if (!CreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping real LightCullingPass gate (GL "
         "4.3 compute unavailable on this host)");
  }

  // Real production config through the real QualityTierManager singleton:
  // High tier + clustered v3 lighting (mutates the global config exactly like
  // the B2/B3 gate and the ShadowPipelineTierFallback integration test).
  const auto settingsPath = MakeGateSettingsPath("light_culling_gate.json");
  WriteGateSettingsJson(settingsPath);
  auto &qm = render::core::QualityTierManager::Get();
  qm.Initialize(settingsPath.string(), true);
  qm.ForceTier(render::core::QualityTier::High);
  {
    auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());
    cfg.v3Enabled = true;
    cfg.dynamicLightingEnabled = true;
    cfg.clusteredLightingEnabled = true;
    cfg.clusteredLightingV4Enabled = true;
    cfg.clusterTileSize = render::core::kDefaultClusterTileSize;
    cfg.clusterZSliceCount = render::core::kDefaultClusterZSliceCount;
    cfg.maxLights = static_cast<int>(kMaxClusteredLights);
  }
  REQUIRE(qm.GetConfig().v3Enabled);
  REQUIRE(qm.GetConfig().clusteredLightingEnabled);

  entt::registry registry;
  ResourceManager resources;

  const size_t quadBefore = CountRegistryRecordsNamed("FullscreenQuadVAO");
  const size_t nonQuadBefore = LeakCheckResourceCount() - quadBefore;

  auto hdr = render::resources::FramebufferManager::Create(
      kShadowGateSize, kShadowGateSize, kGLRgba16f, false);
  REQUIRE(hdr.IsValid());

  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};
  camera.zoom = 1.0f;

  // One point light at (16,16) radius 40: bounds (-24,-24)-(56,56), layers
  // [-1,0] which intersects z-slices 3 ([-8,-1]) and 4 ([0,7]) on all 4 XY
  // tiles (2x2 grid on the 64x64 HDR buffer) -> 8 clusters each pointCount=1,
  // writeCursor=8, overflow=0 (deterministic; see the light_culling.comp
  // analysis in the plan). View culling must be disabled for testing because
  // the 1x1 window would otherwise reject every light.
  std::vector<components::GPULight> lights(1);
  lights[0].posX = 16.0f;
  lights[0].posY = 16.0f;
  lights[0].radius = 40.0f;
  lights[0].intensity = 1.0f;
  lights[0].priority = 50u;
  lights[0].lightType = static_cast<uint32_t>(components::LightType::PointLight);

  auto &lightManager = render::lighting::LightManager::Get();
  lightManager.Initialize();
  lightManager.SetDisableViewCullingForTesting(true);
  lightManager.UpdateCandidates(lights, camera,
                                static_cast<int>(kMaxClusteredLights), 1);
  REQUIRE(lightManager.GetLightBufferId() != 0u);
  REQUIRE(lightManager.GetActiveLightRecordsCpu().size() == 1u);

  DrainGlErrors();

  uint64_t countsHashValid = 0;
  {
    auto lightCullingPass = std::make_shared<render::passes::LightCullingPass>();
    auto consumerPass = std::make_shared<ClusterConsumerPass>();

    RenderGraph graph;
    graph.AddPass(lightCullingPass);
    graph.AddPass(consumerPass);
    graph.Build();
    REQUIRE(!graph.HasValidationErrors());

    const auto &plan = graph.GetCompiledPlan();

    // ---- B4 compiled imports: 6 for the real pass, LightCulling binding
    // domain points 0-5, 5 cluster buffers owned by LightCulling and the
    // LightBuffer by Lighting, all resizeFollowsCapacity. ----
    {
      size_t importCount = 0;
      for (const auto &import : plan.imports) {
        if (import.passName == "LightCullingPass") {
          ++importCount;
        }
      }
      CHECK_EQ(importCount, 6u);

      const auto checkImport = [&plan](RenderResourceTag tag,
                                       uint32_t expectedPoint,
                                       RenderOwnerTag expectedOwner) {
        for (const auto &import : plan.imports) {
          if (import.passName != "LightCullingPass") {
            continue;
          }
          if (import.resourceTag == tag) {
            CHECK_EQ(import.bindingPoint, expectedPoint);
            CHECK(import.backingOwner == expectedOwner);
            CHECK(import.resizeFollowsCapacity);
            CHECK_EQ(import.kind, ResourceKind::StorageBuffer);
            return;
          }
        }
        FAIL("missing import for tag");
      };
      checkImport(RenderResourceTag::LightBufferSSBO, 0u, RenderOwnerTag::Lighting);
      checkImport(RenderResourceTag::ClusterHeaderSSBO, 1u, RenderOwnerTag::LightCulling);
      checkImport(RenderResourceTag::ClusterLightIndexSSBO, 2u, RenderOwnerTag::LightCulling);
      checkImport(RenderResourceTag::LightBoundsSSBO, 3u, RenderOwnerTag::LightCulling);
      checkImport(RenderResourceTag::ClusterCounterSSBO, 4u, RenderOwnerTag::LightCulling);
      checkImport(RenderResourceTag::ClusterPackedLightSSBO, 5u, RenderOwnerTag::LightCulling);
    }

    // ---- B4 compiled bindings: the REAL pass now declares 6 BindBufferBase
    // observers at the LightCulling binding-domain points 0-5 (matching the
    // import binding points and the BindingRegistry symbols resolved in
    // Execute), so the resolver can directly admit the real 6-point BufferBase
    // surface. ----
    {
      size_t bindingCount = 0;
      for (const auto &binding : plan.bindings) {
        if (binding.passName == "LightCullingPass") {
          ++bindingCount;
        }
      }
      CHECK_EQ(bindingCount, 6u);

      const auto checkBinding = [&plan](RenderResourceTag tag,
                                        uint32_t expectedPoint) {
        for (const auto &binding : plan.bindings) {
          if (binding.passName != "LightCullingPass") {
            continue;
          }
          if (binding.resourceName == ToResourceName(tag)) {
            CHECK_EQ(binding.kind, ResourceBindingKind::BufferBase);
            CHECK_EQ(binding.point, expectedPoint);
            return;
          }
        }
        FAIL("missing binding for tag");
      };
      checkBinding(RenderResourceTag::LightBufferSSBO, 0u);
      checkBinding(RenderResourceTag::ClusterHeaderSSBO, 1u);
      checkBinding(RenderResourceTag::ClusterLightIndexSSBO, 2u);
      checkBinding(RenderResourceTag::LightBoundsSSBO, 3u);
      checkBinding(RenderResourceTag::ClusterCounterSSBO, 4u);
      checkBinding(RenderResourceTag::ClusterPackedLightSSBO, 5u);
    }

    // ---- B4 compiled transitions: LightBounds Host->Compute (same pass,
    // CPU upload before compute read) and the 4 cluster buffers Compute->
    // Fragment with the storage barrier bits (0x2000). ----
    {
      bool lightBoundsTransition = false;
      size_t clusterFragmentTransitions = 0;
      for (const auto &transition : plan.transitions) {
        if (transition.resourceName == "LightBoundsSSBO" &&
            transition.previousStage == PipelineStage::Host &&
            transition.nextStage == PipelineStage::Compute) {
          lightBoundsTransition = true;
          CHECK(transition.barrierBits != 0u);
        }
        const bool isClusterBuffer =
            transition.resourceName == "ClusterHeaderSSBO" ||
            transition.resourceName == "ClusterLightIndexSSBO" ||
            transition.resourceName == "ClusterPackedLightSSBO" ||
            transition.resourceName == "ClusterCounterSSBO";
        if (isClusterBuffer &&
            transition.previousStage == PipelineStage::Compute &&
            transition.nextStage == PipelineStage::Fragment) {
          ++clusterFragmentTransitions;
          CHECK_EQ(transition.barrierBits, kLightCullingStorageBarrierBit);
        }
      }
      REQUIRE(lightBoundsTransition);
      REQUIRE_EQ(clusterFragmentTransitions, 4u);
    }

    // ---- Negative validation: no read-before-write / multiple writers /
    // cycle diagnostics for the real pass graph. ----
    {
      bool negativeError = false;
      for (const auto &diagnostic : graph.GetValidationDiagnostics()) {
        if (diagnostic.severity ==
            RenderGraph::ValidationDiagnostic::Severity::Error) {
          negativeError = true;
        }
      }
      CHECK_FALSE(negativeError);
    }

    const size_t lightPassIndex =
        FindCompiledPassIndex(plan, "LightCullingPass");
    REQUIRE(lightPassIndex != static_cast<size_t>(-1));

    auto &clusterState = render::lighting::ClusteredLightingState::Get();

    // ---- Frame A: empty imported-backing snapshot. The 6 declared observers
    // have no snapshot handles to admit, so the resolver fails closed (zero
    // operations, allAdmitted false, 6 "no imported backing snapshot" denials)
    // and LightCullingPass FAILS CLOSED: it skips the dispatch because the
    // graph-driven binding was not admitted (the manual binds are removed, so
    // no safety net remains). No cluster data is produced and nothing is
    // rendered; BeginFrame already allocated the real cluster buffers before
    // the guard, so the handles exist for frame B. ----
    {
      RenderContext context = {};
      context.registry = &registry;
      context.resources = &resources;
      context.qualityManager = &qm;
      context.camera = &camera;
      context.hdrSceneBuffer = hdr;

      const auto resolution = graph.ResolvePassBindings(lightPassIndex, context);
      CHECK_FALSE(resolution.allAdmitted);
      CHECK(resolution.operations.empty());
      size_t missingSnapshotDenials = 0;
      for (const auto &diagnostic : resolution.diagnostics) {
        if (diagnostic.severity ==
                RenderGraph::ValidationDiagnostic::Severity::Error &&
            diagnostic.message.find("no imported backing snapshot") !=
                std::string::npos) {
          ++missingSnapshotDenials;
        }
      }
      CHECK_EQ(missingSnapshotDenials, 6u);

      graph.Execute(context);

      CHECK(lightCullingPass->HadFailureThisFrame());
      CHECK_FALSE(lightCullingPass->SucceededThisFrame());
      CHECK_FALSE(lightCullingPass->IsClusterDataReadyForCurrentFrame());
      CHECK_EQ(clusterState.GetLastOverflowSum(), 0u);
    }

    // ---- Frame B: valid imported-backing snapshot built from the REAL
    // handles the pass created during frame A. The resolver now admits EXACTLY
    // the 6 declared BindBufferBase observers at the LightCulling binding
    // points 0-5 with the real handles (order follows Setup declaration);
    // Execute must run with zero runtime diagnostics. This is now the ONLY
    // rendering path (the manual binds are removed), so the graph-driven
    // cluster counts themselves are the correctness baseline: deterministic,
    // non-trivial and written through the bound surfaces. ----
    {
      std::vector<ImportedBackingHandle> validSnapshot = {
          {RenderResourceTag::LightBufferSSBO, lightManager.GetLightBufferId(),
           0u, 0u},
          {RenderResourceTag::ClusterHeaderSSBO,
           clusterState.GetClusterHeaderBufferId(), 0u, 0u},
          {RenderResourceTag::ClusterLightIndexSSBO,
           clusterState.GetClusterLightIndexBufferId(), 0u, 0u},
          {RenderResourceTag::ClusterPackedLightSSBO,
           clusterState.GetClusterPackedLightBufferId(), 0u, 0u},
          {RenderResourceTag::ClusterCounterSSBO,
           clusterState.GetCounterBufferId(), 0u, 0u},
          {RenderResourceTag::LightBoundsSSBO,
           clusterState.GetLightBoundsBufferId(), 0u, 0u},
      };

      RenderContext context = {};
      context.registry = &registry;
      context.resources = &resources;
      context.qualityManager = &qm;
      context.camera = &camera;
      context.hdrSceneBuffer = hdr;
      context.importedBackings = validSnapshot;

      const auto resolution = graph.ResolvePassBindings(lightPassIndex, context);
      REQUIRE(resolution.allAdmitted);
      REQUIRE(resolution.operations.size() == 6u);
      CHECK_EQ(resolution.operations[0].kind,
               RenderGraph::ResolvedBindingOperation::Kind::BindBufferBase);
      CHECK_EQ(resolution.operations[0].point, 0u);
      CHECK_EQ(resolution.operations[0].handle, lightManager.GetLightBufferId());
      CHECK_EQ(resolution.operations[1].point, 1u);
      CHECK_EQ(resolution.operations[1].handle,
               clusterState.GetClusterHeaderBufferId());
      CHECK_EQ(resolution.operations[2].point, 2u);
      CHECK_EQ(resolution.operations[2].handle,
               clusterState.GetClusterLightIndexBufferId());
      CHECK_EQ(resolution.operations[3].point, 3u);
      CHECK_EQ(resolution.operations[3].handle,
               clusterState.GetLightBoundsBufferId());
      CHECK_EQ(resolution.operations[4].point, 4u);
      CHECK_EQ(resolution.operations[4].handle,
               clusterState.GetCounterBufferId());
      CHECK_EQ(resolution.operations[5].point, 5u);
      CHECK_EQ(resolution.operations[5].handle,
               clusterState.GetClusterPackedLightBufferId());

      graph.Execute(context);

      CHECK(graph.GetRuntimeBindingDiagnostics().empty());
      CHECK(lightCullingPass->SucceededThisFrame());
      CHECK(lightCullingPass->IsClusterDataReadyForCurrentFrame());
      CHECK_EQ(clusterState.GetLastWrittenIndexCount(), 8u);
      CHECK_EQ(clusterState.GetLastOverflowSum(), 0u);
      countsHashValid =
          HashClusterCounts(clusterState.GetClusterHeadersReadback());
      // Non-triviality: the graph-driven binds alone were effective — the
      // pass really wrote per-cluster counts (8 clusters with pointCount=1),
      // not the all-zero cleared state.
      std::vector<components::GPUClusterHeader> zeros(
          clusterState.GetClusterHeadersReadback().size());
      CHECK_NE(countsHashValid, HashClusterCounts(zeros));
    }

    // ---- Frame C: zero-handle snapshot -> the resolver fails closed (zero
    // operations, allAdmitted false, 6 "zero/invalid handle" denials recorded
    // at runtime) and LightCullingPass skips its dispatch (fail closed, no
    // garbage), so the cluster counts stay bit-identical to the graph-driven
    // frame B. ----
    {
      std::vector<ImportedBackingHandle> zeroSnapshot = {
          {RenderResourceTag::LightBufferSSBO, 0u, 0u, 0u},
          {RenderResourceTag::ClusterHeaderSSBO, 0u, 0u, 0u},
          {RenderResourceTag::ClusterLightIndexSSBO, 0u, 0u, 0u},
          {RenderResourceTag::ClusterPackedLightSSBO, 0u, 0u, 0u},
          {RenderResourceTag::ClusterCounterSSBO, 0u, 0u, 0u},
          {RenderResourceTag::LightBoundsSSBO, 0u, 0u, 0u},
      };
      RenderContext context = {};
      context.registry = &registry;
      context.resources = &resources;
      context.qualityManager = &qm;
      context.camera = &camera;
      context.hdrSceneBuffer = hdr;
      context.importedBackings = zeroSnapshot;

      const auto resolution = graph.ResolvePassBindings(lightPassIndex, context);
      CHECK_FALSE(resolution.allAdmitted);
      CHECK(resolution.operations.empty());

      graph.Execute(context);

      size_t zeroHandleDenials = 0;
      for (const auto &diagnostic : graph.GetRuntimeBindingDiagnostics()) {
        if (diagnostic.severity ==
                RenderGraph::ValidationDiagnostic::Severity::Error &&
            diagnostic.message.find("zero/invalid handle") !=
                std::string::npos) {
          ++zeroHandleDenials;
        }
      }
      CHECK_EQ(zeroHandleDenials, 6u);
      CHECK(lightCullingPass->HadFailureThisFrame());
      CHECK_FALSE(lightCullingPass->SucceededThisFrame());
      CHECK_FALSE(lightCullingPass->IsClusterDataReadyForCurrentFrame());

      // No garbage: the denied frame never dispatched, so the cluster buffers
      // are exactly what the graph-driven frame B wrote.
      const uint64_t countsHashZero =
          HashClusterCounts(clusterState.GetClusterHeadersReadback());
      CHECK_EQ(countsHashZero, countsHashValid);
    }

    // ---- GL error surface: all three frames introduced no GL errors. ----
    CHECK_EQ(CountAndDrainGlErrors(), 0);
  }  // graph + shared_ptr pass released here; ~LightCullingPass calls its
     // (private) Shutdown, which only zeroes the borrowed shader reference.

  // Real owners release their backings.
  render::lighting::ClusteredLightingState::Get().Shutdown();
  render::lighting::LightManager::Get().Shutdown();

  // Ownership teardown: LightCullingPass borrows the manager-owned compute
  // shader (it never UnloadShaders it); ResourceManager is the sole releaser.
  resources.unloadAll();
  CHECK_EQ(resources.GetShaderReleaseCount(), 1u);

  render::resources::FramebufferManager::Destroy(hdr);

  // ---- Registry snapshot: no active-record growth attributable to this test
  // after teardown. ----
  const size_t quadAfter = CountRegistryRecordsNamed("FullscreenQuadVAO");
  const size_t nonQuadAfter = LeakCheckResourceCount() - quadAfter;
  CHECK_EQ(nonQuadAfter, nonQuadBefore);
}

// ===========================================================================
// Phase B B4 6-point BufferBase surface gate: the real light_culling compute
// surface through a graph-admitted 6-point BindBufferBase executor (2026-08-05).
//
// The production LightCullingPass declares the 6 BindBufferBase observers (see
// TEST_CASE "B4 contract") and, since 2026-08-05, graph-driven binding is its
// sole binding surface. This gate proves at the EXECUTOR level that the
// graph-driven binds produce bit-identical output to a manual baseline on the
// real 6-point surface. It uses the test-local LightCullingSurfacePass, which
// mirrors the production Setup surface (6 descriptors with the real
// Lighting/LightCulling owners, 6 BindBufferBase at points 0-5, 6
// ImportResource) and the production Execute (same shader, same
// BeginFrame/UploadLightBounds/uniform/dispatch/barrier/readback). The mirror
// pass's ManualOnly authority is the fixture's OWN reference baseline (it binds
// the real handles manually inside Execute) — it is not a production safety
// net. This gate verifies:
//   - the resolver admits EXACTLY the 6 BindBufferBase operations at points
//     0-5 whose handles equal the real buffers;
//   - graph-driven-only and manual-only frames produce deterministic, bit-
//     identical cluster readbacks (counts + 32B counter + written index range +
//     written packed range) that are non-trivial;
//   - a zero-handle snapshot fails closed (allAdmitted false, no operations,
//     6 zero/invalid-handle diagnostics) — the fixture's manual-only authority
//     still produces the identical reference output, proving the executor-level
//     equivalence of the two binding surfaces;
//   - zero GL errors and no registry growth after teardown.
// ===========================================================================
TEST_CASE(
    "[GPU-Diagnostic] RenderGraph - B4 real 6-point LightCulling BufferBase "
    "surface executor gate on real GL") {
  using namespace NoMoreDay;
  using namespace NoMoreDay::render::graph;

  if (!CreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping B4 6-point surface gate (GL 4.3 "
         "compute unavailable on this host)");
  }

  const auto settingsPath = MakeGateSettingsPath("light_culling_surface.json");
  WriteGateSettingsJson(settingsPath);
  auto &qm = render::core::QualityTierManager::Get();
  qm.Initialize(settingsPath.string(), true);
  qm.ForceTier(render::core::QualityTier::High);
  {
    auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());
    cfg.v3Enabled = true;
    cfg.dynamicLightingEnabled = true;
    cfg.clusteredLightingEnabled = true;
    cfg.clusteredLightingV4Enabled = true;
    cfg.clusterTileSize = render::core::kDefaultClusterTileSize;
    cfg.clusterZSliceCount = render::core::kDefaultClusterZSliceCount;
    cfg.maxLights = static_cast<int>(kMaxClusteredLights);
  }

  entt::registry registry;
  ResourceManager resources;

  const size_t quadBefore = CountRegistryRecordsNamed("FullscreenQuadVAO");
  const size_t nonQuadBefore = LeakCheckResourceCount() - quadBefore;

  auto hdr = render::resources::FramebufferManager::Create(
      kShadowGateSize, kShadowGateSize, kGLRgba16f, false);
  REQUIRE(hdr.IsValid());

  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};
  camera.zoom = 1.0f;

  std::vector<components::GPULight> lights(1);
  lights[0].posX = 16.0f;
  lights[0].posY = 16.0f;
  lights[0].radius = 40.0f;
  lights[0].intensity = 1.0f;
  lights[0].priority = 50u;
  lights[0].lightType = static_cast<uint32_t>(components::LightType::PointLight);

  auto &lightManager = render::lighting::LightManager::Get();
  lightManager.Initialize();
  lightManager.SetDisableViewCullingForTesting(true);
  lightManager.UpdateCandidates(lights, camera,
                                static_cast<int>(kMaxClusteredLights), 1);
  REQUIRE(lightManager.GetLightBufferId() != 0u);

  DrainGlErrors();

  uint64_t manualCountsHash = 0;
  uint64_t manualCounterHash = 0;
  uint64_t manualIndexHash = 0;
  uint64_t manualPackedHash = 0;
  {
    auto &clusterState = render::lighting::ClusteredLightingState::Get();

    // ---- Frame W: warm-up, manual-only authority, EMPTY snapshot. The first
    // Execute creates the real ClusteredLightingState buffers (BeginFrame ->
    // EnsureBufferCapacity) and loads the real compute shader through the
    // ResourceManager. The graph-driven admission is irrelevant here (the
    // snapshot is empty; the resolver records "no imported backing snapshot"
    // denials that are drained before the next frame); the manual binds stay
    // authoritative. ----
    {
      auto surfacePass = std::make_shared<LightCullingSurfacePass>(
          LightCullingSurfacePass::BindingAuthority::ManualOnly);

      RenderGraph graph;
      graph.AddPass(surfacePass);
      graph.Build();
      REQUIRE(!graph.HasValidationErrors());

      RenderContext context = {};
      context.registry = &registry;
      context.resources = &resources;
      context.qualityManager = &qm;
      context.camera = &camera;
      context.hdrSceneBuffer = hdr;

      graph.Execute(context);
    }

    // Real buffer handles now exist (created by the warm-up frame).
    REQUIRE(clusterState.GetClusterHeaderBufferId() != 0u);
    REQUIRE(clusterState.GetCounterBufferId() != 0u);
    REQUIRE(clusterState.GetLightBoundsBufferId() != 0u);

    // ---- Frame M: manual-only authority. The surface pass declares the 6
    // BindBufferBase observers; a valid snapshot built from the REAL handles is
    // supplied so the resolver admits all six, and Execute performs the manual
    // binds as the authoritative path. Output is read back as the manual
    // baseline. ----
    std::vector<ImportedBackingHandle> validSnapshot = {
        {RenderResourceTag::LightBufferSSBO, lightManager.GetLightBufferId(),
         0u, 0u},
        {RenderResourceTag::ClusterHeaderSSBO,
         clusterState.GetClusterHeaderBufferId(), 0u, 0u},
        {RenderResourceTag::ClusterLightIndexSSBO,
         clusterState.GetClusterLightIndexBufferId(), 0u, 0u},
        {RenderResourceTag::ClusterPackedLightSSBO,
         clusterState.GetClusterPackedLightBufferId(), 0u, 0u},
        {RenderResourceTag::ClusterCounterSSBO,
         clusterState.GetCounterBufferId(), 0u, 0u},
        {RenderResourceTag::LightBoundsSSBO,
         clusterState.GetLightBoundsBufferId(), 0u, 0u},
    };

    {
      auto surfacePass = std::make_shared<LightCullingSurfacePass>(
          LightCullingSurfacePass::BindingAuthority::ManualOnly);

      RenderGraph graph;
      graph.AddPass(surfacePass);
      graph.Build();
      REQUIRE(!graph.HasValidationErrors());

      const size_t surfacePassIndex =
          FindCompiledPassIndex(graph.GetCompiledPlan(), "LightCullingSurfacePass");
      REQUIRE(surfacePassIndex != static_cast<size_t>(-1));

      RenderContext context = {};
      context.registry = &registry;
      context.resources = &resources;
      context.qualityManager = &qm;
      context.camera = &camera;
      context.hdrSceneBuffer = hdr;
      context.importedBackings = validSnapshot;

      const auto resolution =
          graph.ResolvePassBindings(surfacePassIndex, context);
      REQUIRE(resolution.allAdmitted);
      REQUIRE(resolution.operations.size() == 6u);
      // The six admitted operations must be the real 6-point surface at points
      // 0-5 with the real buffer handles (order follows Setup declaration).
      CHECK_EQ(resolution.operations[0].kind,
               RenderGraph::ResolvedBindingOperation::Kind::BindBufferBase);
      CHECK_EQ(resolution.operations[0].point, 0u);
      CHECK_EQ(resolution.operations[0].handle,
               lightManager.GetLightBufferId());
      CHECK_EQ(resolution.operations[1].point, 1u);
      CHECK_EQ(resolution.operations[1].handle,
               clusterState.GetClusterHeaderBufferId());
      CHECK_EQ(resolution.operations[2].point, 2u);
      CHECK_EQ(resolution.operations[2].handle,
               clusterState.GetClusterLightIndexBufferId());
      CHECK_EQ(resolution.operations[3].point, 3u);
      CHECK_EQ(resolution.operations[3].handle,
               clusterState.GetLightBoundsBufferId());
      CHECK_EQ(resolution.operations[4].point, 4u);
      CHECK_EQ(resolution.operations[4].handle,
               clusterState.GetCounterBufferId());
      CHECK_EQ(resolution.operations[5].point, 5u);
      CHECK_EQ(resolution.operations[5].handle,
               clusterState.GetClusterPackedLightBufferId());

      graph.Execute(context);
      CHECK(graph.GetRuntimeBindingDiagnostics().empty());

      const uint32_t written = clusterState.GetLastWrittenIndexCount();
      CHECK_EQ(written, 8u);
      manualCountsHash =
          HashClusterCounts(clusterState.GetClusterHeadersReadback());
      manualCounterHash = HashCounterBuffer(clusterState.GetCounterBufferId());
      manualIndexHash = HashIndexRange(clusterState.GetClusterLightIndexBufferId(),
                                       written);
      manualPackedHash =
          HashPackedRange(clusterState.GetClusterPackedLightBufferId(), written);
    }

    // ---- Frame G: graph-driven-only authority on the SAME surface and the
    // SAME snapshot. The resolver admits the same 6 operations; Execute relies
    // on the graph-driven binds (no manual binds). Output must be bit-identical
    // to the manual baseline and non-trivial. ----
    {
      auto surfacePass = std::make_shared<LightCullingSurfacePass>(
          LightCullingSurfacePass::BindingAuthority::GraphDrivenOnly);

      RenderGraph graph;
      graph.AddPass(surfacePass);
      graph.Build();
      REQUIRE(!graph.HasValidationErrors());

      const size_t surfacePassIndex =
          FindCompiledPassIndex(graph.GetCompiledPlan(), "LightCullingSurfacePass");
      REQUIRE(surfacePassIndex != static_cast<size_t>(-1));

      RenderContext context = {};
      context.registry = &registry;
      context.resources = &resources;
      context.qualityManager = &qm;
      context.camera = &camera;
      context.hdrSceneBuffer = hdr;
      context.importedBackings = validSnapshot;

      const auto resolution =
          graph.ResolvePassBindings(surfacePassIndex, context);
      REQUIRE(resolution.allAdmitted);
      REQUIRE(resolution.operations.size() == 6u);
      CHECK_EQ(resolution.operations[0].point, 0u);
      CHECK_EQ(resolution.operations[0].handle,
               lightManager.GetLightBufferId());
      CHECK_EQ(resolution.operations[5].point, 5u);
      CHECK_EQ(resolution.operations[5].handle,
               clusterState.GetClusterPackedLightBufferId());

      graph.Execute(context);
      CHECK(graph.GetRuntimeBindingDiagnostics().empty());

      const uint32_t written = clusterState.GetLastWrittenIndexCount();
      CHECK_EQ(written, 8u);
      CHECK_EQ(HashClusterCounts(clusterState.GetClusterHeadersReadback()),
               manualCountsHash);
      CHECK_EQ(HashCounterBuffer(clusterState.GetCounterBufferId()),
               manualCounterHash);
      CHECK_EQ(HashIndexRange(clusterState.GetClusterLightIndexBufferId(),
                              written),
               manualIndexHash);
      CHECK_EQ(HashPackedRange(clusterState.GetClusterPackedLightBufferId(),
                               written),
               manualPackedHash);

      // Non-triviality: the cluster counts (8 clusters with pointCount=1) and
      // the counter (writeCursor=8) are not the all-zero cleared state.
      std::vector<components::GPUClusterHeader> zeros(
          clusterState.GetClusterHeadersReadback().size());
      CHECK_NE(manualCountsHash, HashClusterCounts(zeros));
      NoMoreDay::components::GPUClusterCounters zeroCounter = {};
      CHECK_NE(manualCounterHash,
               Fnv1a64(&zeroCounter, sizeof(zeroCounter)));
      CHECK_NE(manualPackedHash, 0u);
    }

    // ---- Frame Z: zero-handle snapshot fails closed at the resolver (no
    // operations, allAdmitted false, 6 zero/invalid-handle diagnostics) while
    // the fixture's manual-only authority still produces the bit-identical
    // reference baseline, proving the executor-level equivalence of the two
    // binding surfaces on the real 6-point surface. ----
    {
      std::vector<ImportedBackingHandle> zeroSnapshot = {
          {RenderResourceTag::LightBufferSSBO, 0u, 0u, 0u},
          {RenderResourceTag::ClusterHeaderSSBO, 0u, 0u, 0u},
          {RenderResourceTag::ClusterLightIndexSSBO, 0u, 0u, 0u},
          {RenderResourceTag::ClusterPackedLightSSBO, 0u, 0u, 0u},
          {RenderResourceTag::ClusterCounterSSBO, 0u, 0u, 0u},
          {RenderResourceTag::LightBoundsSSBO, 0u, 0u, 0u},
      };

      auto surfacePass = std::make_shared<LightCullingSurfacePass>(
          LightCullingSurfacePass::BindingAuthority::GraphDrivenOnly);
      surfacePass->dispatchEnabled = false;  // denied frame must not dispatch

      RenderGraph graph;
      graph.AddPass(surfacePass);
      graph.Build();
      REQUIRE(!graph.HasValidationErrors());

      const size_t surfacePassIndex =
          FindCompiledPassIndex(graph.GetCompiledPlan(), "LightCullingSurfacePass");
      REQUIRE(surfacePassIndex != static_cast<size_t>(-1));

      RenderContext context = {};
      context.registry = &registry;
      context.resources = &resources;
      context.qualityManager = &qm;
      context.camera = &camera;
      context.hdrSceneBuffer = hdr;
      context.importedBackings = zeroSnapshot;

      const auto resolution =
          graph.ResolvePassBindings(surfacePassIndex, context);
      CHECK_FALSE(resolution.allAdmitted);
      CHECK(resolution.operations.empty());

      graph.Execute(context);

      size_t zeroHandleDenials = 0;
      for (const auto &diagnostic : graph.GetRuntimeBindingDiagnostics()) {
        if (diagnostic.severity ==
                RenderGraph::ValidationDiagnostic::Severity::Error &&
            diagnostic.message.find("zero/invalid handle") !=
                std::string::npos) {
          ++zeroHandleDenials;
        }
      }
      CHECK_EQ(zeroHandleDenials, 6u);

      // Fixture reference baseline: same surface through the mirror pass's
      // manual-only authority (its own manual binds, not a production path).
      auto manualPass = std::make_shared<LightCullingSurfacePass>(
          LightCullingSurfacePass::BindingAuthority::ManualOnly);
      RenderGraph manualGraph;
      manualGraph.AddPass(manualPass);
      manualGraph.Build();
      REQUIRE(!manualGraph.HasValidationErrors());

      RenderContext manualContext = {};
      manualContext.registry = &registry;
      manualContext.resources = &resources;
      manualContext.qualityManager = &qm;
      manualContext.camera = &camera;
      manualContext.hdrSceneBuffer = hdr;
      manualContext.importedBackings = zeroSnapshot;

      manualGraph.Execute(manualContext);

      const uint32_t written = clusterState.GetLastWrittenIndexCount();
      CHECK_EQ(written, 8u);
      CHECK_EQ(HashClusterCounts(clusterState.GetClusterHeadersReadback()),
               manualCountsHash);
      CHECK_EQ(HashCounterBuffer(clusterState.GetCounterBufferId()),
               manualCounterHash);
      CHECK_EQ(HashPackedRange(clusterState.GetClusterPackedLightBufferId(),
                               written),
               manualPackedHash);
    }

    // ---- GL error surface: all frames introduced no GL errors. ----
    CHECK_EQ(CountAndDrainGlErrors(), 0);
  }

  render::lighting::ClusteredLightingState::Get().Shutdown();
  render::lighting::LightManager::Get().Shutdown();

  resources.unloadAll();
  CHECK_EQ(resources.GetShaderReleaseCount(), 1u);

  render::resources::FramebufferManager::Destroy(hdr);

  const size_t quadAfter = CountRegistryRecordsNamed("FullscreenQuadVAO");
  const size_t nonQuadAfter = LeakCheckResourceCount() - quadAfter;
  CHECK_EQ(nonQuadAfter, nonQuadBefore);
}
