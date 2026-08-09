#include "doctest.h"
#include "game/foundation/data/BiomeRegistry.hpp"
#include "game/foundation/data/BiomeTypes.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace NoMoreDay;

namespace {
std::filesystem::path ResolveBiomeJsonPath() {
  constexpr std::array<const char *, 4> kCandidates = {
      "assets/data/biomes.json",
      "../assets/data/biomes.json",
      "../../assets/data/biomes.json",
      "../../../assets/data/biomes.json",
  };

  for (const char *candidate : kCandidates) {
    const auto path = std::filesystem::path(candidate);
    if (std::filesystem::exists(path)) {
      return std::filesystem::absolute(path);
    }
  }

  throw std::runtime_error("Unable to locate assets/data/biomes.json from test cwd");
}
} // namespace

TEST_CASE("[Unit] BiomeRegistry - V2 Load And Feature Flags") {
  const auto jsonPath = ResolveBiomeJsonPath();
  BiomeRegistry::Get().LoadFromJSON(jsonPath.string());

  constexpr std::array<BiomeID, 27> kExpectedBiomeIds = {
      BiomeID::Town,         BiomeID::Town_SwordImmortal, BiomeID::Town_Mage,
      BiomeID::Town_Mech,    BiomeID::Town_Shadow,        BiomeID::Town_Beast,
      BiomeID::Town_Radiant, BiomeID::Cave,               BiomeID::SunPrairie,
      BiomeID::IceTundra,    BiomeID::CrimsonWaste,       BiomeID::DustSea,
      BiomeID::VoidFlats,    BiomeID::EmeraldWet,         BiomeID::AshPlain,
      BiomeID::GloomSpire,   BiomeID::MagmaVeins,         BiomeID::JadeMine,
      BiomeID::DrownedLib,   BiomeID::ClockCore,          BiomeID::AncientCrypt,
      BiomeID::CrystalLab,   BiomeID::FloatingIsle,       BiomeID::CoralRuin,
      BiomeID::WhisperWood,  BiomeID::HolyArena,          BiomeID::HiveNest,
  };

  for (BiomeID id : kExpectedBiomeIds) {
    CHECK(BiomeRegistry::Get().HasBiome(id));
  }
  CHECK(BiomeRegistry::Get().HasBiome(BiomeID::SkyPalace));
  CHECK(BiomeRegistry::Get().HasBiome(BiomeID::AbyssalGap));

  CHECK(BiomeRegistry::Get().HasBiome("town"));
  CHECK(BiomeRegistry::Get().HasBiome("cave"));
  CHECK(BiomeRegistry::Get().HasBiome("floating_isle"));
  CHECK(BiomeRegistry::Get().HasBiome("abyssal_gap"));

  const auto &town = BiomeRegistry::Get().GetBiome(BiomeID::Town);
  CHECK(town.id == "town");
  CHECK(town.style == BiomeStyle::Town);
  CHECK(town.isSafeZone);
  CHECK_FALSE(town.hasFeature(BiomeFeature::AirWall));

  const auto &cave = BiomeRegistry::Get().GetBiome(BiomeID::Cave);
  CHECK(cave.id == "cave");
  CHECK(cave.style == BiomeStyle::Open);
  CHECK(cave.numericId == BiomeID::Cave);

  const auto &floating = BiomeRegistry::Get().GetBiome("floating_isle");
  CHECK(floating.numericId == BiomeID::FloatingIsle);
  CHECK(floating.style == BiomeStyle::Special);
  CHECK(floating.hasFeature(BiomeFeature::AirWall));
  CHECK_FALSE(floating.hasFeature(BiomeFeature::LimitedVision));

  const auto &abyss = BiomeRegistry::Get().GetBiome(BiomeID::AbyssalGap);
  CHECK(abyss.style == BiomeStyle::Special);
  CHECK(abyss.hasFeature(BiomeFeature::LimitedVision));
  CHECK(abyss.hasFeature(BiomeFeature::VisualFilter));
  CHECK(abyss.visionRadius == doctest::Approx(150.0f));
}

TEST_CASE("[Unit] Abyss fog filter - preserves scene and only softens edge") {
  constexpr std::array<const char *, 4> kCandidates = {
      "assets/shaders/filters/abyss_fog.fs",
      "../assets/shaders/filters/abyss_fog.fs",
      "../../assets/shaders/filters/abyss_fog.fs",
      "../../../assets/shaders/filters/abyss_fog.fs",
  };

  std::string source;
  for (const char *candidate : kCandidates) {
    std::ifstream in(candidate, std::ios::binary);
    if (!in) {
      continue;
    }
    source.assign(std::istreambuf_iterator<char>(in),
                  std::istreambuf_iterator<char>());
    break;
  }

  REQUIRE_FALSE(source.empty());
  CHECK(source.find("smoothstep(effectiveRadius * 0.7") != std::string::npos);
  CHECK(source.find("mix(base.rgb, fogColor, edgeStrength)") !=
        std::string::npos);
  CHECK(source.find("mix(fogColor, base.rgb, visibility)") ==
        std::string::npos);
  CHECK(source.find("sin(fragTexCoord") == std::string::npos);
}
