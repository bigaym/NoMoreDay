#include "doctest.h"

#include "engine/render/core/QualityTierManager.hpp"

TEST_CASE("[Unit] PostProcess ColorGrading - QualityTier Switch Path") {
  using NoMoreDay::render::core::QualityTier;
  auto &qualityManager = NoMoreDay::render::core::QualityTierManager::Get();

  qualityManager.ForceTier(QualityTier::Low);
  CHECK(!qualityManager.GetConfig().colorGradingEnabled);
  CHECK(qualityManager.GetConfig().colorGradingLutSize == 0);
  CHECK(qualityManager.GetConfig().vignetteEnabled == false);

  qualityManager.ForceTier(QualityTier::High);
  CHECK(qualityManager.GetConfig().colorGradingEnabled);
  CHECK(qualityManager.GetConfig().colorGradingLutSize == 16);
  CHECK(qualityManager.GetConfig().colorGradingIntensity == doctest::Approx(0.06f));
  CHECK(qualityManager.GetConfig().vignetteIntensity == doctest::Approx(0.02f));

  qualityManager.ForceTier(QualityTier::Ultra);
  CHECK(qualityManager.GetConfig().colorGradingEnabled);
  CHECK(qualityManager.GetConfig().colorGradingLutSize == 32);
  CHECK(qualityManager.GetConfig().colorGradingIntensity == doctest::Approx(0.09f));
  CHECK(qualityManager.GetConfig().vignetteIntensity == doctest::Approx(0.03f));
}

TEST_CASE("[Unit] PostProcess ColorGrading - LUT Size Boundaries") {
  NoMoreDay::render::core::RenderConfig config = {};

  config.colorGradingLutSize = 0;
  CHECK(config.colorGradingLutSize == 0);

  config.colorGradingLutSize = 16;
  CHECK(config.colorGradingLutSize == 16);

  config.colorGradingLutSize = 32;
  CHECK(config.colorGradingLutSize == 32);
}
