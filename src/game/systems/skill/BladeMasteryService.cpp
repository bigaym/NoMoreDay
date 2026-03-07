#include "game/systems/skill/BladeMasteryService.hpp"

#include "game/components/PlayerState.hpp"
#include "game/data/BladeMasteryRegistry.hpp"
#include "game/data/TalentData.hpp"
#include "game/systems/skill/BladeResourceService.hpp"

namespace NoMoreDay::systems {

namespace {

bool g_debug_unlock_override_enabled = false;

const BladeMasteryProfile *GetProfile(BladeMasteryId mastery_id) {
  return data::BladeMasteryRegistry::Get().GetProfile(mastery_id);
}

} // namespace

bool BladeMasteryService::IsDebugUnlockOverrideEnabled() {
  return g_debug_unlock_override_enabled;
}

void BladeMasteryService::SetDebugUnlockOverrideEnabled(bool enabled) {
  g_debug_unlock_override_enabled = enabled;
}

bool BladeMasteryService::HasBladeAscendantProfession(
    const entt::registry &registry, entt::entity entity) {
  const auto *astrolabe = registry.try_get<AstrolabeComponent>(entity);
  return astrolabe != nullptr &&
         astrolabe->mainProfession == static_cast<int>(ProfessionID::BladeAscendant);
}

int BladeMasteryService::GetCurrentLevel(const entt::registry &registry,
                                         entt::entity entity) {
  if (const auto *stats = registry.try_get<PlayerStats>(entity)) {
    return stats->level;
  }
  if (const auto *level = registry.try_get<PlayerLevel>(entity)) {
    return level->value;
  }
  return 1;
}

void BladeMasteryService::RefreshPlayerState(entt::registry &registry,
                                             entt::entity entity) {
  if (!HasBladeAscendantProfession(registry, entity)) {
    BladeResourceService::RemoveBladeResource(registry, entity);
    if (registry.all_of<BladeMasteryComponent>(entity)) {
      registry.remove<BladeMasteryComponent>(entity);
    }
    if (registry.all_of<BladeSignatureSkillComponent>(entity)) {
      registry.remove<BladeSignatureSkillComponent>(entity);
    }
    return;
  }

  auto &mastery = registry.get_or_emplace<BladeMasteryComponent>(entity);
  mastery.profession = ProfessionID::BladeAscendant;

  const BladeMasteryProfile *profile = GetProfile(mastery.selected);
  if (mastery.selected != BladeMasteryId::None &&
      !IsMasteryUnlocked(registry, entity, mastery.selected)) {
    mastery.selected = BladeMasteryId::None;
    profile = nullptr;
  }

  if (profile != nullptr) {
    const int level = GetCurrentLevel(registry, entity);
    mastery.debug_unlock_active =
        g_debug_unlock_override_enabled && level < profile->unlock_level &&
        level >= profile->debug_unlock_level_override;
  } else {
    mastery.debug_unlock_active = false;
  }

  const BladeResourceKind kind =
      profile != nullptr ? profile->resource_kind : BladeResourceKind::SwordIntent;
  const int max_resource =
      profile != nullptr ? profile->max_resource
                         : SkillConstants::DEFAULT_MAX_SWORD_INTENT;
  const float grace_period =
      profile != nullptr ? profile->grace_period
                         : SkillConstants::SWORD_INTENT_GRACE_PERIOD;
  const float decay_interval =
      profile != nullptr ? profile->decay_interval
                         : SkillConstants::SWORD_INTENT_DECAY_INTERVAL;
  BladeResourceService::EnsureBladeResource(registry, entity, kind, max_resource,
                                            grace_period, decay_interval);

  auto &signature =
      registry.get_or_emplace<BladeSignatureSkillComponent>(entity);
  if (profile != nullptr) {
    signature.skill_id = profile->signature_skill_id;
    signature.unlocked = true;
  } else {
    signature.skill_id = INVALID_SKILL_ID;
    signature.unlocked = false;
  }
}

bool BladeMasteryService::IsMasteryUnlocked(const entt::registry &registry,
                                            entt::entity entity,
                                            BladeMasteryId mastery_id) {
  if (!HasBladeAscendantProfession(registry, entity)) {
    return false;
  }

  const BladeMasteryProfile *profile = GetProfile(mastery_id);
  if (profile == nullptr) {
    return false;
  }

  const int level = GetCurrentLevel(registry, entity);
  if (level >= profile->unlock_level) {
    return true;
  }
  return g_debug_unlock_override_enabled &&
         level >= profile->debug_unlock_level_override;
}

bool BladeMasteryService::SelectMastery(entt::registry &registry,
                                        entt::entity entity,
                                        BladeMasteryId mastery_id) {
  if (!IsMasteryUnlocked(registry, entity, mastery_id)) {
    return false;
  }

  auto &mastery = registry.get_or_emplace<BladeMasteryComponent>(entity);
  mastery.profession = ProfessionID::BladeAscendant;
  mastery.selected = mastery_id;
  RefreshPlayerState(registry, entity);
  return true;
}

BladeMasteryId BladeMasteryService::GetSelectedMastery(
    const entt::registry &registry, entt::entity entity) {
  if (const auto *mastery = registry.try_get<BladeMasteryComponent>(entity)) {
    return mastery->selected;
  }
  return BladeMasteryId::None;
}

bool BladeMasteryService::IsSignatureSkillUnlocked(
    const entt::registry &registry, entt::entity entity, uint32_t skill_id) {
  const auto *signature =
      registry.try_get<BladeSignatureSkillComponent>(entity);
  return signature != nullptr && signature->unlocked &&
         signature->skill_id == skill_id;
}

} // namespace NoMoreDay::systems
