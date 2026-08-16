#include "doctest.h"

#include "engine/render/GPUABIContract.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/core/QualityTierManager.hpp"

TEST_CASE("[Integration] GPU ABI - Cross-tier shader/link and binding governance") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  auto &qualityManager = render::core::QualityTierManager::Get();
  qualityManager.Initialize("settings.json");
  const auto originalTier = qualityManager.GetTier();

  constexpr render::core::QualityTier kTiers[4] = {
      render::core::QualityTier::Low, render::core::QualityTier::Medium,
      render::core::QualityTier::High, render::core::QualityTier::Ultra};

  for (render::core::QualityTier tier : kTiers) {
    qualityManager.ForceTier(tier);

    CHECK(render::abi::ValidateGeneratedShaderABI(
        "assets/shaders/generated/gpu_abi.glslinc", false));
    CHECK(RenderConstants::BindingGovernance::HasUniqueGlobalBindings());

    CHECK(RenderSystem::Initialize());
    RenderSystem::Shutdown();
  }

  qualityManager.ForceTier(originalTier);
}

