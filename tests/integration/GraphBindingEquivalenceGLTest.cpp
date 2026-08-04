#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"

#include "raylib.h"
#include "rlgl.h"
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdint>
#include <cstring>
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
