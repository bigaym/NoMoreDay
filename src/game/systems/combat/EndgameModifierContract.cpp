#include "game/systems/combat/EndgameModifierContract.hpp"
#include "core/logging/Logger.hpp"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace NoMoreDay::systems {
namespace {

float ClampField(float value, float minValue, float maxValue) {
  return std::clamp(value, minValue, maxValue);
}

void AppendTraceId(uint32_t id, std::array<uint32_t, EndgameModifierTrace::kMaxTrackedIds> &ids,
                   size_t &count) {
  for (size_t i = 0; i < count; ++i) {
    if (ids[i] == id) {
      return;
    }
  }
  if (count < ids.size()) {
    ids[count++] = id;
  }
}

} // namespace

EndgameModifierRegistry &EndgameModifierRegistry::Get() {
  static EndgameModifierRegistry instance;
  return instance;
}

bool EndgameModifierRegistry::EnsureLoaded() {
  if (loaded_) {
    return true;
  }
  (void)LoadFromFile();
  return loaded_;
}

bool EndgameModifierRegistry::LoadFromFile(const std::string &path) {
  contracts_.clear();
  LoadBuiltins();

  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_WARN("EndgameModifierRegistry: failed to open {}, fallback to builtins.",
             path);
    loaded_ = true;
    return false;
  }

  nlohmann::json root;
  try {
    file >> root;
  } catch (const std::exception &e) {
    LOG_ERROR("EndgameModifierRegistry: invalid json in {}: {}", path, e.what());
    loaded_ = true;
    return false;
  }

  const auto modifiersIt = root.find("modifiers");
  if (modifiersIt == root.end() || !modifiersIt->is_array()) {
    LOG_WARN("EndgameModifierRegistry: {} missing modifiers array, keep builtins.",
             path);
    loaded_ = true;
    return false;
  }

  for (const auto &entry : *modifiersIt) {
    const uint32_t id = entry.value("id", 0u);
    if (id == 0u) {
      continue;
    }

    EndgameModifierContract contract;
    contract.id = id;
    contract.name = entry.value("name", std::string("Modifier-") + std::to_string(id));

    if (const auto offenseIt = entry.find("offense");
        offenseIt != entry.end() && offenseIt->is_object()) {
      contract.outgoing_damage_more = ClampField(
          offenseIt->value("damage_more", 0.0f), -0.95f, 20.0f);
      contract.outgoing_resistance_reduction =
          ClampField(offenseIt->value("resistance_reduction", 0.0f), 0.0f, 1.0f);
      contract.outgoing_armor_reduction =
          offenseIt->value("armor_reduction", 0.0f);
      contract.outgoing_global_damage_reduction_reduction = ClampField(
          offenseIt->value("global_damage_reduction_reduction", 0.0f), 0.0f,
          0.95f);
      contract.outgoing_ailment_magnitude_more = ClampField(
          offenseIt->value("ailment_magnitude_more", 0.0f), -0.95f, 20.0f);
      contract.outgoing_ailment_duration_more = ClampField(
          offenseIt->value("ailment_duration_more", 0.0f), -0.95f, 20.0f);
    }

    if (const auto defenseIt = entry.find("defense");
        defenseIt != entry.end() && defenseIt->is_object()) {
      contract.incoming_damage_taken_more = ClampField(
          defenseIt->value("damage_taken_more", 0.0f), -0.95f, 20.0f);
      contract.incoming_resistance_bonus =
          ClampField(defenseIt->value("resistance_bonus", 0.0f), -1.0f, 1.0f);
      contract.incoming_armor_bonus = defenseIt->value("armor_bonus", 0.0f);
      contract.incoming_global_damage_reduction_bonus = ClampField(
          defenseIt->value("global_damage_reduction_bonus", 0.0f), -0.95f,
          0.95f);
      contract.incoming_ailment_taken_more = ClampField(
          defenseIt->value("ailment_taken_more", 0.0f), -0.95f, 20.0f);
      contract.incoming_ailment_duration_bonus = ClampField(
          defenseIt->value("ailment_duration_bonus", 0.0f), -0.95f, 20.0f);
    }

    contracts_[contract.id] = contract;
  }

  loaded_ = true;
  return true;
}

void EndgameModifierRegistry::ResetForTests() {
  contracts_.clear();
  loaded_ = false;
}

const EndgameModifierContract *EndgameModifierRegistry::Find(uint32_t id) const {
  const auto iter = contracts_.find(id);
  if (iter != contracts_.end()) {
    return &iter->second;
  }
  return nullptr;
}

EndgameModifierResolution EndgameModifierRegistry::ResolveForEntities(
    const entt::registry &registry, entt::entity source,
    entt::entity target) const {
  EndgameModifierResolution resolution;
  if (!loaded_) {
    return resolution;
  }

  auto applyOutgoing = [&](entt::entity entity) {
    if (!registry.valid(entity)) {
      return;
    }
    const auto *runtime = registry.try_get<EndgameModifierRuntimeComponent>(entity);
    if (!runtime) {
      return;
    }
    for (const uint32_t modifierId : runtime->outgoing_modifier_ids) {
      const auto *contract = Find(modifierId);
      if (!contract) {
        continue;
      }
      resolution.aggregate.outgoing_damage_more += contract->outgoing_damage_more;
      resolution.aggregate.outgoing_resistance_reduction +=
          contract->outgoing_resistance_reduction;
      resolution.aggregate.outgoing_armor_reduction +=
          contract->outgoing_armor_reduction;
      resolution.aggregate.outgoing_global_damage_reduction_reduction +=
          contract->outgoing_global_damage_reduction_reduction;
      resolution.aggregate.outgoing_ailment_magnitude_more +=
          contract->outgoing_ailment_magnitude_more;
      resolution.aggregate.outgoing_ailment_duration_more +=
          contract->outgoing_ailment_duration_more;
      AppendTraceId(modifierId, resolution.trace.source_ids,
                    resolution.trace.source_count);
    }
  };

  auto applyIncoming = [&](entt::entity entity) {
    if (!registry.valid(entity)) {
      return;
    }
    const auto *runtime = registry.try_get<EndgameModifierRuntimeComponent>(entity);
    if (!runtime) {
      return;
    }
    for (const uint32_t modifierId : runtime->incoming_modifier_ids) {
      const auto *contract = Find(modifierId);
      if (!contract) {
        continue;
      }
      resolution.aggregate.incoming_damage_taken_more +=
          contract->incoming_damage_taken_more;
      resolution.aggregate.incoming_resistance_bonus +=
          contract->incoming_resistance_bonus;
      resolution.aggregate.incoming_armor_bonus += contract->incoming_armor_bonus;
      resolution.aggregate.incoming_global_damage_reduction_bonus +=
          contract->incoming_global_damage_reduction_bonus;
      resolution.aggregate.incoming_ailment_taken_more +=
          contract->incoming_ailment_taken_more;
      resolution.aggregate.incoming_ailment_duration_bonus +=
          contract->incoming_ailment_duration_bonus;
      AppendTraceId(modifierId, resolution.trace.target_ids,
                    resolution.trace.target_count);
    }
  };

  applyOutgoing(source);
  applyIncoming(target);
  return resolution;
}

void EndgameModifierRegistry::LoadBuiltins() {
  auto setBuiltin = [&](const EndgameModifierContract &contract) {
    contracts_[contract.id] = contract;
  };

  EndgameModifierContract extraDamage;
  extraDamage.id = EndgameModifierIds::ExtraDamage;
  extraDamage.name = "ExtraDamage";
  extraDamage.outgoing_damage_more = 0.25f;
  setBuiltin(extraDamage);

  EndgameModifierContract resistanceRend;
  resistanceRend.id = EndgameModifierIds::ResistanceRend;
  resistanceRend.name = "ResistanceRend";
  resistanceRend.outgoing_resistance_reduction = 0.25f;
  setBuiltin(resistanceRend);

  EndgameModifierContract ailmentAmplification;
  ailmentAmplification.id = EndgameModifierIds::AilmentAmplification;
  ailmentAmplification.name = "AilmentAmplification";
  ailmentAmplification.outgoing_ailment_magnitude_more = 0.30f;
  ailmentAmplification.outgoing_ailment_duration_more = 0.20f;
  setBuiltin(ailmentAmplification);

  EndgameModifierContract armorBreaker;
  armorBreaker.id = EndgameModifierIds::ArmorBreaker;
  armorBreaker.name = "ArmorBreaker";
  armorBreaker.outgoing_armor_reduction = 560.0f;
  setBuiltin(armorBreaker);

  EndgameModifierContract enduringWard;
  enduringWard.id = EndgameModifierIds::EnduringWard;
  enduringWard.name = "EnduringWard";
  enduringWard.incoming_resistance_bonus = 0.15f;
  enduringWard.incoming_global_damage_reduction_bonus = 0.10f;
  setBuiltin(enduringWard);
}

} // namespace NoMoreDay::systems
