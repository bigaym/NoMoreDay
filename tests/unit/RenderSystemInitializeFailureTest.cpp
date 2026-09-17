#include "doctest.h"

#include "engine/render/GPUABIContract.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/MaterialManager.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/core/DeviceCapabilityMatrix.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/resource/TextureArrayManager.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"

#include "TestCommon.hpp"

#include <filesystem>
#include <fstream>
#include <string>

TEST_CASE("[Unit] RenderSystem - Capability failure injection returns false and cleans up") {
  using namespace NoMoreDay::render;
  using namespace NoMoreDay::render::core;
  using namespace NoMoreDay::render::resources;

  // RenderSystem::Initialize 在能力门禁之前会执行 QualityTierManager::Initialize("settings.json")，
  // 即便本用例期望失败也会先写回基准分数；交由夹具快照/还原保证工作区无污染。
  TestSetupScope settingsGuard;

  const size_t initialResourceCount = GPUResourceRegistry::Get().GetStats().activeCount;

  auto &capMatrix = DeviceCapabilityMatrix::Get();
  capMatrix.ResetForTesting();

  // Inject capability failure report (missing required compute/SSBO features)
  CapabilityReport degradedReport = {};
  degradedReport.isGL43Supported = false;
  degradedReport.isComputeSupported = false;
  degradedReport.isSSBOSupported = false;
  degradedReport.isImageLoadStoreSupported = false;
  degradedReport.isMemoryBarrierSupported = false;
  capMatrix.SetProbeOverrideForTesting(degradedReport);

  // Attempt Initialize -> must fail closed (return false)
  const bool initResult = RenderSystem::Initialize();
  CHECK_FALSE(initResult);
  CHECK_FALSE(RenderSystem::IsInitialized());

  // Subsystems must be cleaned up and not left active
  CHECK_FALSE(MaterialManager::Get().IsInitialized());
  CHECK_FALSE(TextureArrayManager::Get().IsInitialized());
  CHECK_FALSE(lighting::LightManager::Get().IsInitialized());

  // Calling Shutdown() after failed init must be idempotent and safe
  CHECK_NOTHROW(RenderSystem::Shutdown());
  CHECK_NOTHROW(RenderSystem::Shutdown());
  CHECK_FALSE(RenderSystem::IsInitialized());

  // Verify no resource leak in registry
  const size_t postFailureResourceCount = GPUResourceRegistry::Get().GetStats().activeCount;
  CHECK(postFailureResourceCount <= initialResourceCount);

  // Reset testing probe override
  capMatrix.ResetForTesting();
}

TEST_CASE("[Unit] RenderSystem - ABI mismatch failure returns false and cleans up") {
  using namespace NoMoreDay::render;
  using namespace NoMoreDay::render::abi;
  using namespace NoMoreDay::render::resources;
  namespace fs = std::filesystem;

  // 本用例在 ABI 校验处即返回（RenderSystem.cpp:961-966 先于 :968-969 的写入），
  // 本身并非污染源；夹具为防御性保留，避免 Initialize 内部顺序调整后悄然回归为写入点。
  TestSetupScope settingsGuard;

  const size_t initialResourceCount = GPUResourceRegistry::Get().GetStats().activeCount;

  const fs::path dummyPath = fs::path("bin") / "tmp_nonexistent_abi.glslinc";
  SetGeneratedShaderABIManifestForTesting(dummyPath.string());

  // Attempt Initialize -> must fail due to missing/incompatible ABI manifest
  const bool initResult = RenderSystem::Initialize();
  CHECK_FALSE(initResult);
  CHECK_FALSE(RenderSystem::IsInitialized());

  // Subsystems must be cleaned up
  CHECK_FALSE(MaterialManager::Get().IsInitialized());
  CHECK_FALSE(TextureArrayManager::Get().IsInitialized());
  CHECK_FALSE(lighting::LightManager::Get().IsInitialized());

  // Shutdown must be safe and clean
  CHECK_NOTHROW(RenderSystem::Shutdown());
  CHECK_FALSE(RenderSystem::IsInitialized());

  // Verify no resource leak in registry
  const size_t postFailureResourceCount = GPUResourceRegistry::Get().GetStats().activeCount;
  CHECK(postFailureResourceCount <= initialResourceCount);

  // Reset manifest override
  ResetGeneratedShaderABIManifestForTesting();
}

TEST_CASE("[Unit] RenderSystem - Idempotent shutdown is safe when uninitialized") {
  using namespace NoMoreDay::render;

  RenderSystem::Shutdown();
  CHECK_FALSE(RenderSystem::IsInitialized());
  CHECK_FALSE(MaterialManager::Get().IsInitialized());
  CHECK_FALSE(TextureArrayManager::Get().IsInitialized());
  CHECK_FALSE(lighting::LightManager::Get().IsInitialized());

  RenderSystem::Shutdown();
  CHECK_FALSE(RenderSystem::IsInitialized());
}
