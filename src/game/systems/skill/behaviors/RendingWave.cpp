/**
 * @file RendingWave.cpp
 * @brief 裂空斩 (ID 2) - 模块化投射物波刃技能实现
 */
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "SevenStarSlashShared.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "raymath.h"

namespace NoMoreDay::skills {

namespace RendingWaveNodes {
constexpr uint32_t SwordQi = 200; constexpr uint32_t MultiWave = 210; constexpr uint32_t SplitBlade = 211; constexpr uint32_t Boomerang = 230;
constexpr uint32_t PullTrap = 232; constexpr uint32_t TimeLock = 233; constexpr uint32_t VoidConvert = 250; constexpr uint32_t IntentBurst = 252;
constexpr uint32_t IntentGain = 253; constexpr uint32_t ElementForm = 270;
} // namespace RendingWaveNodes
struct RendingWave : SkillBehaviorBase<RendingWave> {
  static constexpr uint32_t kSkillId = 2;
  static void DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
    auto *pos = registry.try_get<Position>(owner); auto *stats = registry.try_get<CombatStats>(owner); if (!pos || !stats) return;
    const auto link = seven_star_shared::ConsumeLinkBuffs(registry, owner, kSkillId, false, exec.cast_id);
    const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId);
    BakedSkillProfile localProfile;
    if (!profile && registry.all_of<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
        if (spec.skill_id == kSkillId) { SkillSpecializationBaker::Bake(registry, owner, kSkillId, &spec, localProfile, nullptr); profile = &localProfile; break; }
      }
    }
    Vector2 baseDir = Vector2Normalize(Vector2Subtract(exec.target_pos, {pos->x, pos->y}));
    float baseSpeed = 300.0f, baseRadius = profile ? profile->area_radius : 35.0f, baseLifetime = 1.2f;
    int extraWaves = profile ? (profile->projectile_count > 1 ? profile->projectile_count - 1 : 0) : 0;
    if (const auto *res = registry.try_get<BladeResourceComponent>(owner); res && res->kind == BladeResourceKind::SwordFlow) {
      int flow = std::clamp(res->current, 0, 10);
      baseSpeed *= (1.0f + flow * 0.04f); baseRadius *= (1.0f + flow * 0.03f);
      if (flow >= 5) extraWaves += 1; if (flow >= 8) extraWaves += 1; if (flow >= 10) extraWaves += 1;
    }
    bool boomerang = profile ? ((profile->delivery.feature_flags & (1 | 16)) != 0) : (exec.active_nodes.test(RendingWaveNodes::Boomerang % 100) || exec.active_nodes.test(RendingWaveNodes::PullTrap % 100));
    bool splitOnDeath = profile ? ((profile->delivery.feature_flags & 4) != 0) : exec.active_nodes.test(RendingWaveNodes::SplitBlade % 100);
    bool hoverAtApex = profile ? ((profile->delivery.feature_flags & 32) != 0) : exec.active_nodes.test(RendingWaveNodes::TimeLock % 100);
    bool isVoid = profile ? (profile->effective_tags == Tag::Void) : exec.active_nodes.test(RendingWaveNodes::VoidConvert % 100);
    Tag effTag = profile ? profile->effective_tags : (isVoid ? Tag::Void : Tag::Physical);
    if (profile ? ((profile->delivery.feature_flags & 128) != 0) : exec.active_nodes.test(RendingWaveNodes::IntentBurst % 100)) {
      const auto *res = registry.try_get<BladeResourceComponent>(owner);
      int spend = (res && res->kind == BladeResourceKind::SwordFlow) ? res->current : SkillConstants::DEFAULT_MAX_SWORD_INTENT;
      if (spend > 0 && SkillSystem::ConsumeSwordIntent(registry, owner, spend, kSkillId)) {
        exec.is_empowered = true;
        if (auto *act = registry.try_get<ActiveSkillsComponent>(owner)) {
          for (auto &slot : act->slots) { if (slot.id == 1) slot.cooldown = (spend >= SkillConstants::DEFAULT_MAX_SWORD_INTENT) ? 0.0f : std::max(0.0f, slot.cooldown - 1.5f); }
        }
      }
    }
    const Tag attunement = systems::BladeResourceService::GetHeavenlyAttunementElementTag(registry, owner);
    int totalCount = (1 + extraWaves) * (exec.is_empowered ? 2 : 1);
    float spread = 0.4f + (totalCount * 0.05f), startAngle = (totalCount > 1) ? -spread / 2.0f : 0.0f, angleStep = totalCount > 1 ? spread / (totalCount - 1) : 0.0f;
    for (int i = 0; i < totalCount; ++i) {
      Vector2 dir = Vector2Rotate(baseDir, startAngle + i * angleStep);
      auto proj_ent = registry.create(); registry.emplace<LocalLevelTag>(proj_ent);
      registry.emplace<Position>(proj_ent, pos->x + dir.x * (baseRadius * 0.6f), pos->y + dir.y * (baseRadius * 0.6f));
      registry.emplace<Velocity>(proj_ent, dir.x * baseSpeed, dir.y * baseSpeed);
      auto &proj = registry.emplace<Projectile>(proj_ent);
      float maxRange = (profile && profile->delivery.range > 0.0f) ? profile->delivery.range : 400.0f;
      proj.owner = owner; proj.cast_id = exec.cast_id; proj.speed = baseSpeed; proj.lifeTime = hoverAtApex ? (maxRange / baseSpeed) : baseLifetime;
      proj.radius = exec.is_empowered ? baseRadius * 1.5f : baseRadius; proj.pierce = true; proj.pierceCount = 99; proj.max_pierce = Projectile::kUnlimitedPiercing; proj.snapshot = *stats;
      if (link.consumed_any && !proj.snapshot.damage_multipliers.empty()) proj.snapshot.damage_multipliers.front() *= link.damage_multiplier;
      if (splitOnDeath) { proj.on_death = Projectile::OnDeathBehavior::Split; proj.split_count = 3; }
      if (hoverAtApex) proj.on_death = Projectile::OnDeathBehavior::Hover;
      proj.payload_context = {.base_damage_min = stats->min_weapon_damage, .base_damage_max = stats->max_weapon_damage, .crit_chance = stats->crit_chance / 100.0f, .crit_multiplier = stats->crit_damage, .more_damage = (exec.is_empowered ? 2.0f : 1.0f) * (profile ? profile->more_damage_mult : 1.0f), .effective_tags = effTag, .source_skill_id = kSkillId};
      registry.emplace<CombatStats>(proj_ent, proj.snapshot); registry.emplace<SkillComponent>(proj_ent, kSkillId, owner);
      if (attunement != Tag::None) registry.emplace_or_replace<SkillModifierComponent>(proj_ent).damage_modifiers.push_back({Tag::Physical, attunement, 0.5f, ModifierType::Convert});
      else if (auto *om = registry.try_get<SkillModifierComponent>(owner)) registry.emplace_or_replace<SkillModifierComponent>(proj_ent, *om);
      if (boomerang && !hoverAtApex) {
        auto &bc = registry.emplace<BoomerangComponent>(proj_ent);
        bc.owner = owner; bc.skill_id = kSkillId; bc.returnTimer = maxRange / baseSpeed; bc.phase = BoomerangPhase::Outward; bc.returnSpeed = baseSpeed * 1.2f;
        if (profile && (profile->delivery.feature_flags & 16)) { bc.pull_radius = (profile->delivery.pull_radius > 0.0f) ? profile->delivery.pull_radius : 120.0f; bc.pull_strength = 250.0f; }
      }
    }
  }

  static void DoHit(entt::registry &reg, entt::entity attacker, entt::entity, Tag, bool) {
    const auto *p = SkillSystem::GetBakedSkillProfile(reg, attacker, kSkillId);
    bool g = p ? ((p->delivery.feature_flags & 256) != 0) : false;
    if (!p && reg.all_of<ActiveSkillsComponent>(attacker)) {
      for (const auto &s : reg.get<ActiveSkillsComponent>(attacker).specialized_slots) { if (s.skill_id == kSkillId) { g = s.allocated_points.contains(RendingWaveNodes::IntentGain); break; } }
    }
    if (g) SkillSystem::GainSwordIntent(reg, attacker, 1, kSkillId);
  }
};

REGISTER_SKILL_BEHAVIOR(RendingWave)
void RegisterRendingWave() {}
} // namespace NoMoreDay::skills
