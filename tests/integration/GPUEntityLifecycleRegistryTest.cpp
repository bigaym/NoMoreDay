#include "doctest.h"

#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "raylib.h"
#include "rlgl.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// W5.7 (MS-8 / M0-B RG-3): local GL lifecycle evidence for the observer
// registry. Uses a real (hidden, 1x1) GL context, initializes GPUEntitySystem,
// verifies expected kind/owner records, performs one real frame advance, shuts
// down twice, and verifies every GPUEntitySystem-owned observer record returns
// to baseline with balanced created/destroyed counters and no GL diagnostics.
// This is local contract evidence only - it is NOT a substitute for W6
// hardware evidence and does not change the production NO-GO status.
namespace {
constexpr uint32_t kW5GlFramebuffer = 0x8D40;

bool W5EnsureGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "W5 GPUEntity Lifecycle Test Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  return NoMoreDay::utils::GPUUtils::IsInitialized();
}

std::vector<GLenum> W5DrainGlErrors() {
  std::vector<GLenum> errors;
  GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR) {
    errors.push_back(err);
  }
  return errors;
}

bool W5IsEntityRecord(const NoMoreDay::render::resources::GPUResourceRecord &rec) {
  using namespace NoMoreDay::render::resources;
  if (rec.name == "GPUEntityRenderShader" || rec.name == "GPUEntityQuadVAO" ||
      rec.name == "GPUEntityQuadVBO") {
    return true;
  }
  // Wrapper-owned records created by GPUEntitySystem (ComputeBuffer /
  // PersistentBuffer / PersistentBufferMapping) all share this prefix.
  if (rec.name == "ComputeBuffer" || rec.name == "PersistentBuffer" ||
      rec.name == "PersistentBufferMapping") {
    return true;
  }
  return false;
}

// RAII guard that temporarily hides a file so a later load fails, restoring it
// on scope exit. Used to inject a deterministic mid-init failure point.
class W5ScopedFileHider {
public:
  W5ScopedFileHider(std::filesystem::path from, std::filesystem::path to)
      : m_from(std::move(from)), m_to(std::move(to)), m_active(false) {
    std::error_code ec;
    if (std::filesystem::exists(m_from)) {
      std::filesystem::rename(m_from, m_to, ec);
      m_active = !ec;
    }
  }
  ~W5ScopedFileHider() {
    if (m_active) {
      std::error_code ec;
      std::filesystem::rename(m_to, m_from, ec);
    }
  }
  W5ScopedFileHider(const W5ScopedFileHider &) = delete;
  W5ScopedFileHider &operator=(const W5ScopedFileHider &) = delete;

private:
  std::filesystem::path m_from;
  std::filesystem::path m_to;
  bool m_active;
};
} // namespace

// Lifecycle: Init registers the expected kinds (ShaderProgram/VertexArray/
// VertexBuffer plus wrapper-owned StorageBuffer/PersistentMapping), and two
// Shutdown calls return every GPUEntitySystem-owned observer record and every
// aggregate counter to the pre-Init baseline (the registry was Reset before the
// baseline snapshot, so that baseline is zero) with no GL diagnostics.
// The exact-one frame advance is covered by the dedicated advancement test
// below; this test keeps the ledger untouched by the global render path so the
// baseline-restore assertion is exact.
TEST_CASE("[Integration] W5 - GPUEntitySystem lifecycle balances registry observers") {
  using namespace NoMoreDay;
  using namespace NoMoreDay::render;
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::systems;

  if (!W5EnsureGpuContext()) {
    FAIL("Cannot create GPU context; skipping W5 GPUEntitySystem lifecycle test");
  }
  (void)W5DrainGlErrors();

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  // Baseline captured BEFORE Init so the balance assertions below measure
  // exactly the records GPUEntitySystem creates and must destroy.
  const GPUResourceSnapshot baseline = registry.TakeSnapshot();
  CHECK(baseline.activeResourceCount == 0);
  CHECK(baseline.currentTotalBytes == 0);

  ResourceManager resources;
  uint32_t shaderHandle = 0;
  uint32_t vaoHandle = 0;
  uint32_t vboHandle = 0;
  uint64_t createdByInit = 0;
  {
    GPUEntitySystem system;
    system.Init(resources, 2048);
    createdByInit =
        registry.GetStats().totalCreatedCount - baseline.totalCreatedCount;
    CHECK(createdByInit > 0);

    // Expected kinds observed after Init: the raw render shader, quad VAO and
    // quad VBO are registered by GPUEntitySystem itself; the persistent and
    // compute buffers register through their RAII wrappers. Concrete handles
    // are captured so post-Shutdown absence can be asserted directly.
    bool hasShaderProgram = false;
    bool hasVertexArray = false;
    bool hasVertexBuffer = false;
    size_t entityStorageBuffers = 0;
    size_t persistentMappings = 0;
    for (const auto &rec : registry.GetActiveResources()) {
      if (rec.name == "GPUEntityRenderShader" && rec.kind == ResourceKind::ShaderProgram) {
        hasShaderProgram = true;
        shaderHandle = rec.handle;
      }
      if (rec.name == "GPUEntityQuadVAO" && rec.kind == ResourceKind::VertexArray) {
        hasVertexArray = true;
        vaoHandle = rec.handle;
      }
      if (rec.name == "GPUEntityQuadVBO" && rec.kind == ResourceKind::VertexBuffer) {
        hasVertexBuffer = true;
        vboHandle = rec.handle;
      }
      if (rec.name == "PersistentBuffer" && rec.kind == ResourceKind::StorageBuffer) {
        ++entityStorageBuffers;
      }
      if (rec.name == "PersistentBufferMapping" &&
          rec.kind == ResourceKind::PersistentMapping) {
        ++persistentMappings;
      }
    }
    CHECK(hasShaderProgram);
    CHECK(hasVertexArray);
    CHECK(hasVertexBuffer);
    CHECK(shaderHandle != 0);
    CHECK(vaoHandle != 0);
    CHECK(vboHandle != 0);
    // 2 persistent buffers (entity data + physics output), each registered as a
    // StorageBuffer observer; the 5 grid compute buffers also register.
    CHECK(entityStorageBuffers == 2);
    // Persistent mappings appear only when the driver supports persistent
    // mapping; WARP/software GL falls back to Compat and registers none.
    CHECK((persistentMappings == 0 || persistentMappings == 2));

    // Explicit shutdown; a second call must be a context-safe no-op.
    system.Shutdown();
    system.Shutdown();
  }

  // Every GPUEntitySystem-owned observer record must be gone after shutdown.
  // Check concrete handles first, then fall back to the name-based filter so a
  // mismatched handle cannot hide a wrapper-owned record (ComputeBuffer /
  // PersistentBuffer / PersistentBufferMapping).
  bool entityHandlesGone = true;
  for (const auto &rec : registry.GetActiveResources()) {
    if (rec.handle == shaderHandle || rec.handle == vaoHandle ||
        rec.handle == vboHandle || W5IsEntityRecord(rec)) {
      entityHandlesGone = false;
    }
  }
  CHECK(entityHandlesGone);

  // Aggregate counters return to the pre-Init baseline (zero after Reset):
  // no active records and no tracked bytes remain from this system.
  const GPUResourceSnapshot afterShutdown = registry.TakeSnapshot();
  CHECK(afterShutdown.activeResourceCount == baseline.activeResourceCount);
  CHECK(afterShutdown.currentTotalBytes == baseline.currentTotalBytes);
  CHECK(afterShutdown.totalDestroyedCount - baseline.totalDestroyedCount >=
        createdByInit);

  // The five compute shaders stay with ResourceManager (never double-released
  // by GPUEntitySystem). Release them through the owner, as Game::cleanup does,
  // and only then drain GL diagnostics so shader unload errors surface here.
  resources.unloadAll();

  const std::vector<GLenum> errors = W5DrainGlErrors();
  for (GLenum err : errors) {
    CAPTURE(err);
  }
  CHECK(errors.empty());
}

// Partial-init / uninitialized safety: a GPUEntitySystem that was never
// initialized (or failed midway before acquiring render resources) must
// shutdown cleanly without issuing any GL release. This covers the "no
// post-context destructor GL call" contract for the zeroed-handle state.
TEST_CASE("[Integration] W5 - uninitialized GPUEntitySystem shutdown is a context-safe no-op") {
  using namespace NoMoreDay::systems;

  if (!W5EnsureGpuContext()) {
    FAIL("Cannot create GPU context; skipping W5 uninitialized shutdown test");
  }
  (void)W5DrainGlErrors();

  GPUEntitySystem system; // default-constructed: all handles zeroed
  CHECK_NOTHROW(system.Shutdown());
  CHECK_NOTHROW(system.Shutdown());

  const std::vector<GLenum> errors = W5DrainGlErrors();
  for (GLenum err : errors) {
    CAPTURE(err);
  }
  CHECK(errors.empty());
}

// Advancement: every successful normal render advances the registry exactly
// once. Simulating the hardware-gate stress/toggle loop shape (render without
// any manual AdvanceFrame) must neither double-advance nor omit advancement.
TEST_CASE("[Integration] W5 - normal render advances registry exactly once per frame") {
  using namespace NoMoreDay;
  using namespace NoMoreDay::render;
  using namespace NoMoreDay::render::resources;

  if (!W5EnsureGpuContext()) {
    FAIL("Cannot create GPU context; skipping W5 frame advancement test");
  }
  (void)W5DrainGlErrors();

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  entt::registry enttReg;
  render::RenderFrameInput frameInput;
  Camera2D camera{};
  camera.zoom = 1.0f;

  const uint64_t frame0 = registry.GetFrameIndex();

  // Gate stress loop shape: render with no manual advancement.
  for (int i = 0; i < 3; ++i) {
    utils::GPUUtils::BindFramebuffer(kW5GlFramebuffer, 0);
    RenderSystem::render(enttReg, frameInput, camera);
  }
  CHECK(registry.GetFrameIndex() == frame0 + 3);

  // Gate toggle loop shape: render after a resize-style recreation; still one
  // advance per render.
  for (int i = 0; i < 2; ++i) {
    utils::GPUUtils::BindFramebuffer(kW5GlFramebuffer, 0);
    RenderSystem::render(enttReg, frameInput, camera);
  }
  CHECK(registry.GetFrameIndex() == frame0 + 5);
  CHECK(registry.TakeSnapshot().frameIndex == frame0 + 5);

  const std::vector<GLenum> errors = W5DrainGlErrors();
  for (GLenum err : errors) {
    CAPTURE(err);
  }
  CHECK(errors.empty());
}

// Partial-init rollback: when a required dependency is missing midway
// (injected here by hiding grid_scan.compute, the 5th grid shader, so the
// local persistent/compute buffers have already been acquired and registered),
// Init must release every successfully acquired buffer through the idempotent
// Shutdown, report the system uninitialized, and leave the registry at its
// pre-Init baseline. A fresh ResourceManager instance is used so the shader
// cache cannot mask the missing file; the guard restores the file afterwards.
TEST_CASE("[Integration] W5 - GPUEntitySystem partial-init failure rolls back to baseline") {
  using namespace NoMoreDay;
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::systems;

  if (!W5EnsureGpuContext()) {
    FAIL("Cannot create GPU context; skipping W5 partial-init rollback test");
  }
  (void)W5DrainGlErrors();

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();
  const GPUResourceSnapshot baseline = registry.TakeSnapshot();

  ResourceManager resources;
  {
    // Inject failure into the grid shader dependency set by hiding
    // grid_scan.compute. The guard restores the file on scope exit regardless
    // of the assertions below.
    W5ScopedFileHider hide("assets/shaders/grid_scan.compute",
                           "assets/shaders/grid_scan.compute.w5hidden");
    GPUEntitySystem system;
    system.Init(resources, 2048);

    // Partial-init rollback: the system reports itself uninitialized.
    CHECK(system.GetMaxEntities() == 0);

    // Every partially acquired record was released: none of the
    // GPUEntitySystem-owned names may remain in the ledger.
    bool leaked = false;
    for (const auto &rec : registry.GetActiveResources()) {
      if (W5IsEntityRecord(rec)) {
        leaked = true;
      }
    }
    CHECK(leaked == false);

    // A second Shutdown (e.g. from Game::cleanup after a failed init) stays a
    // context-safe no-op.
    system.Shutdown();
  }

  // Aggregate counters returned to the pre-Init baseline.
  const GPUResourceSnapshot afterRollback = registry.TakeSnapshot();
  CHECK(afterRollback.activeResourceCount == baseline.activeResourceCount);
  CHECK(afterRollback.currentTotalBytes == baseline.currentTotalBytes);

  resources.unloadAll();

  const std::vector<GLenum> errors = W5DrainGlErrors();
  for (GLenum err : errors) {
    CAPTURE(err);
  }
  CHECK(errors.empty());
}
