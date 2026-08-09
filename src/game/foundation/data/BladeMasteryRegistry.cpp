#include "game/foundation/data/BladeMasteryRegistry.hpp"

#include "core/logging/Logger.hpp"
#include "game/foundation/data/TalentData.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

namespace NoMoreDay::data {

namespace {

BladeMasteryId ParseMasteryId(const std::string &value) {
  if (value == "sword_saint") {
    return BladeMasteryId::SwordSaint;
  }
  if (value == "heavenly_sword") {
    return BladeMasteryId::HeavenlySword;
  }
  if (value == "demon_blade") {
    return BladeMasteryId::DemonBlade;
  }
  return BladeMasteryId::None;
}

ProfessionID ParseProfessionId(const std::string &value) {
  if (value == "blade_ascendant") {
    return ProfessionID::BladeAscendant;
  }
  return ProfessionID::BladeAscendant;
}

BladeResourceKind ParseResourceKind(const std::string &value) {
  if (value == "sword_intent") {
    return BladeResourceKind::SwordIntent;
  }
  if (value == "sword_flow") {
    return BladeResourceKind::SwordFlow;
  }
  if (value == "spirit_blade_tier") {
    return BladeResourceKind::SpiritBladeTier;
  }
  if (value == "bloodthirst") {
    return BladeResourceKind::Bloodthirst;
  }
  return BladeResourceKind::None;
}

BladeMasteryProfile ParseProfile(const nlohmann::json &entry) {
  BladeMasteryProfile profile;
  profile.id = ParseMasteryId(entry.value("id", ""));
  profile.profession = ParseProfessionId(entry.value("profession", "blade_ascendant"));
  profile.name = entry.value("name", "");
  profile.description = entry.value("description", "");
  profile.resource_kind = ParseResourceKind(entry.value("resource_kind", ""));
  profile.unlock_level = entry.value("unlock_level", 50);
  profile.debug_unlock_level_override =
      entry.value("debug_unlock_level_override", 5);
  profile.signature_skill_id = entry.value("signature_skill_id", 0u);
  profile.max_resource = entry.value("max_resource", 10);
  profile.grace_period = entry.value("grace_period", 5.0f);
  profile.decay_interval = entry.value("decay_interval", 0.5f);
  return profile;
}

} // namespace

BladeMasteryRegistry &BladeMasteryRegistry::Get() {
  static BladeMasteryRegistry instance;
  return instance;
}

bool BladeMasteryRegistry::Load() {
  return LoadFromJson("assets/data/blade_masteries.json");
}

bool BladeMasteryRegistry::LoadFromJson(const std::string &path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    LOG_ERROR("Failed to open blade mastery data: {}", path);
    profiles_.clear();
    profile_index_.clear();
    return false;
  }

  nlohmann::json root;
  input >> root;

  profiles_.clear();
  profile_index_.clear();

  if (!root.contains("masteries") || !root["masteries"].is_array()) {
    LOG_ERROR("Blade mastery data missing 'masteries' array: {}", path);
    return false;
  }

  for (const auto &entry : root["masteries"]) {
    BladeMasteryProfile profile = ParseProfile(entry);
    if (profile.id == BladeMasteryId::None) {
      continue;
    }

    profile_index_[profile.id] = profiles_.size();
    profiles_.push_back(std::move(profile));
  }

  LOG_INFO("Loaded {} blade mastery profiles from {}", profiles_.size(), path);
  return !profiles_.empty();
}

const BladeMasteryProfile *BladeMasteryRegistry::GetProfile(BladeMasteryId id) const {
  const auto it = profile_index_.find(id);
  if (it == profile_index_.end()) {
    return nullptr;
  }
  return &profiles_[it->second];
}

const std::vector<BladeMasteryProfile> &BladeMasteryRegistry::GetAllProfiles() const {
  return profiles_;
}

} // namespace NoMoreDay::data
