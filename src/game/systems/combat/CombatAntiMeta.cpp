#include "game/systems/combat/CombatAntiMeta.hpp"
#include "core/logging/Logger.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

namespace NoMoreDay {
namespace {

using json = nlohmann::json;

struct AntiMetaState {
  bool loaded = false;
  DiminishingReturnsConfig diminishing = {};
};

AntiMetaState &GetState() {
  static AntiMetaState state;
  return state;
}

constexpr DiminishingReturnsConfig kDefaultDiminishing = {
    true,
    0.35f,
    0.45f,
};

constexpr CostAffixRuntimeConfig kCostAffixNone = {};

constexpr CostAffixRuntimeConfig kCostAffixGlassCannonCrit = {
    "Glass Cannon",
    "+50% Crit Chance",
    "-20% Attack Speed",
    StatType::CritChance,
    ModifierMode::Flat,
    50.0f,
    StatType::AttackSpeed,
    ModifierMode::Flat,
    -20.0f,
    Tag::Hit,
    0.12f,
};

constexpr CostAffixRuntimeConfig kCostAffixHeavyMomentum = {
    "Heavy Momentum",
    "+22% More Physical Damage",
    "-15 Move Speed",
    StatType::ArmorPenetration,
    ModifierMode::Flat,
    25.0f,
    StatType::MoveSpeed,
    ModifierMode::Flat,
    -15.0f,
    Tag::Physical,
    0.22f,
};

void LoadConfigIfNeeded() {
  AntiMetaState &state = GetState();
  if (state.loaded) {
    return;
  }
  state.loaded = true;
  state.diminishing = kDefaultDiminishing;

  std::ifstream file("assets/data/combat_anti_meta_layer.json");
  if (!file.is_open()) {
    LOG_WARN(
        "[CombatAntiMeta] config file missing, using defaults: "
        "assets/data/combat_anti_meta_layer.json");
    return;
  }

  try {
    json config;
    file >> config;
    if (!config.is_object()) {
      LOG_WARN("[CombatAntiMeta] invalid config object, using defaults");
      return;
    }

    if (config.contains("diminishing_returns") &&
        config.at("diminishing_returns").is_object()) {
      const json &dr = config.at("diminishing_returns");
      state.diminishing.enabled = dr.value("enabled", state.diminishing.enabled);
      state.diminishing.base = dr.value("base", state.diminishing.base);
      state.diminishing.scale = dr.value("scale", state.diminishing.scale);
    }

    state.diminishing.base = std::clamp(state.diminishing.base, 0.05f, 5.0f);
    state.diminishing.scale = std::clamp(state.diminishing.scale, 0.01f, 10.0f);
  } catch (const std::exception &e) {
    LOG_WARN("[CombatAntiMeta] parse failed, using defaults: {}", e.what());
    state.diminishing = kDefaultDiminishing;
  }
}

} // namespace

const DiminishingReturnsConfig &
CombatAntiMeta::GetDiminishingReturnsConfig() noexcept {
  LoadConfigIfNeeded();
  return GetState().diminishing;
}

float CombatAntiMeta::ApplyDiminishingReturns(float actual) noexcept {
  const DiminishingReturnsConfig &cfg = GetDiminishingReturnsConfig();
  const float safeActual = (std::max)(0.0f, actual);
  if (!cfg.enabled) {
    return safeActual;
  }
  if (cfg.scale <= 0.0f || cfg.base <= 0.0f) {
    return safeActual;
  }
  return cfg.base * (1.0f - std::exp(-safeActual / cfg.scale));
}

const CostAffixRuntimeConfig &
CombatAntiMeta::GetCostAffixConfig(CostAffixPreset preset) noexcept {
  switch (preset) {
  case CostAffixPreset::GlassCannonCrit:
    return kCostAffixGlassCannonCrit;
  case CostAffixPreset::HeavyMomentum:
    return kCostAffixHeavyMomentum;
  case CostAffixPreset::None:
  default:
    return kCostAffixNone;
  }
}

void CombatAntiMeta::ReloadForTests() noexcept {
  AntiMetaState &state = GetState();
  state.loaded = false;
  state.diminishing = kDefaultDiminishing;
}

} // namespace NoMoreDay
