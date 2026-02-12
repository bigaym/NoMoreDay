#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/passes/PostProcessPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"

namespace {
constexpr uint32_t kGLRgba16f = 0x881A;
}

TEST_CASE("[Unit] PostProcess - FramebufferManager Create/Destroy") {
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    NoMoreDay::utils::GPUUtils::Initialize();
  }

  auto handle =
      NoMoreDay::render::resources::FramebufferManager::Create(128, 128, kGLRgba16f);
  CHECK(handle.IsValid());
  CHECK(handle.width == 128);
  CHECK(handle.height == 128);

  NoMoreDay::render::resources::FramebufferManager::Destroy(handle);
  CHECK(!handle.IsValid());
}

TEST_CASE("[Unit] PostProcess - FramebufferManager Resize") {
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    NoMoreDay::utils::GPUUtils::Initialize();
  }

  auto handle =
      NoMoreDay::render::resources::FramebufferManager::Create(160, 90, kGLRgba16f);
  REQUIRE(handle.IsValid());

  NoMoreDay::render::resources::FramebufferManager::Resize(handle, 320, 180);
  CHECK(handle.IsValid());
  CHECK(handle.width == 320);
  CHECK(handle.height == 180);

  NoMoreDay::render::resources::FramebufferManager::Destroy(handle);
}

TEST_CASE("[Unit] PostProcess - QualityTier Phase1 Config") {
  auto &qm = NoMoreDay::render::core::QualityTierManager::Get();

  qm.ForceTier(NoMoreDay::render::core::QualityTier::Low);
  CHECK(qm.GetConfig().bloomMipLevels == 0);
  CHECK(!qm.GetConfig().fxaaEnabled);
  CHECK(!qm.GetConfig().vignetteEnabled);

  qm.ForceTier(NoMoreDay::render::core::QualityTier::Medium);
  CHECK(qm.GetConfig().bloomMipLevels == 3);
  CHECK(qm.GetConfig().fxaaEnabled);
  CHECK(qm.GetConfig().vignetteEnabled);

  qm.ForceTier(NoMoreDay::render::core::QualityTier::High);
  CHECK(qm.GetConfig().bloomMipLevels == 5);

  qm.ForceTier(NoMoreDay::render::core::QualityTier::Ultra);
  CHECK(qm.GetConfig().bloomMipLevels == 7);
}

TEST_CASE("[Unit] PostProcess - LowTier Bypass Does Not Emit Output") {
  auto &qm = NoMoreDay::render::core::QualityTierManager::Get();
  qm.ForceTier(NoMoreDay::render::core::QualityTier::Low);

  NoMoreDay::render::passes::PostProcessPass pass;
  REQUIRE(pass.Initialize());

  NoMoreDay::render::graph::RenderContext context = {};
  context.qualityManager = &qm;
  pass.Execute(context);

  CHECK(!pass.GetOutputBuffer().IsValid());
  pass.Shutdown();
}

TEST_CASE("[Unit] PostProcess - BloomMipChain Levels") {
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    NoMoreDay::utils::GPUUtils::Initialize();
  }

  auto &qm = NoMoreDay::render::core::QualityTierManager::Get();
  NoMoreDay::render::passes::PostProcessPass pass;
  REQUIRE(pass.Initialize());

  auto hdr =
      NoMoreDay::render::resources::FramebufferManager::Create(512, 512, kGLRgba16f);
  REQUIRE(hdr.IsValid());

  NoMoreDay::render::graph::RenderContext context = {};
  context.qualityManager = &qm;
  context.hdrSceneBuffer = hdr;

  qm.ForceTier(NoMoreDay::render::core::QualityTier::Medium); // 3 mips
  pass.Execute(context);
  CHECK(pass.GetBloomMipCountForTesting() == 3);

  qm.ForceTier(NoMoreDay::render::core::QualityTier::High); // 5 mips
  pass.Execute(context);
  CHECK(pass.GetBloomMipCountForTesting() == 5);

  qm.ForceTier(NoMoreDay::render::core::QualityTier::Ultra); // 7 mips
  pass.Execute(context);
  CHECK(pass.GetBloomMipCountForTesting() == 7);

  NoMoreDay::render::resources::FramebufferManager::Destroy(hdr);
  pass.Shutdown();
}
