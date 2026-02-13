#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/passes/DistortionPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"

#include <cstddef>

namespace {
constexpr uint32_t kGLRg16f = 0x822F;
}

TEST_CASE("[Unit] Distortion - GPUDistortionSource ABI Layout") {
  CHECK(sizeof(NoMoreDay::components::GPUDistortionSource) == 16);
  CHECK(offsetof(NoMoreDay::components::GPUDistortionSource, posX) == 0);
  CHECK(offsetof(NoMoreDay::components::GPUDistortionSource, posY) == 4);
  CHECK(offsetof(NoMoreDay::components::GPUDistortionSource, radius) == 8);
  CHECK(offsetof(NoMoreDay::components::GPUDistortionSource, strength) == 12);
}

TEST_CASE("[Unit] Distortion - Source Add Reset And Clamp") {
  NoMoreDay::render::passes::DistortionPass pass;
  CHECK(pass.GetActiveSourceCountForTesting() == 0);

  for (int i = 0; i < NoMoreDay::render::passes::DistortionPass::MAX_DISTORTION_SOURCES + 12;
       ++i) {
    pass.AddDistortionSource(10.0f + static_cast<float>(i), 20.0f, 30.0f, 0.2f);
  }

  CHECK(pass.GetActiveSourceCountForTesting() ==
        NoMoreDay::render::passes::DistortionPass::MAX_DISTORTION_SOURCES);

  pass.ResetSources();
  CHECK(pass.GetActiveSourceCountForTesting() == 0);
}

TEST_CASE("[Unit] Distortion - FramebufferManager RG16F Create Resize") {
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    NoMoreDay::utils::GPUUtils::Initialize();
  }

  auto handle =
      NoMoreDay::render::resources::FramebufferManager::Create(192, 128, kGLRg16f);
  REQUIRE(handle.IsValid());
  CHECK(handle.width == 192);
  CHECK(handle.height == 128);
  CHECK(handle.internalFormat == kGLRg16f);

  NoMoreDay::render::resources::FramebufferManager::Resize(handle, 384, 256);
  CHECK(handle.IsValid());
  CHECK(handle.width == 384);
  CHECK(handle.height == 256);

  NoMoreDay::render::resources::FramebufferManager::Destroy(handle);
  CHECK(!handle.IsValid());
}

TEST_CASE("[Unit] Distortion - Phase4 QualityTier Config") {
  auto &qm = NoMoreDay::render::core::QualityTierManager::Get();

  qm.ForceTier(NoMoreDay::render::core::QualityTier::Low);
  CHECK(qm.GetConfig().distortionEnabled == false);
  CHECK(qm.GetConfig().maxMaterials == 32);
  CHECK(qm.GetConfig().vfxSequenceDetail == 0);

  qm.ForceTier(NoMoreDay::render::core::QualityTier::Medium);
  CHECK(qm.GetConfig().distortionEnabled == false);
  CHECK(qm.GetConfig().maxMaterials == 64);
  CHECK(qm.GetConfig().vfxSequenceDetail == 1);

  qm.ForceTier(NoMoreDay::render::core::QualityTier::High);
  CHECK(qm.GetConfig().distortionEnabled == true);
  CHECK(qm.GetConfig().maxMaterials == 128);
  CHECK(qm.GetConfig().vfxSequenceDetail == 2);

  qm.ForceTier(NoMoreDay::render::core::QualityTier::Ultra);
  CHECK(qm.GetConfig().distortionEnabled == true);
  CHECK(qm.GetConfig().maxMaterials == 256);
  CHECK(qm.GetConfig().vfxSequenceDetail == 2);
}
