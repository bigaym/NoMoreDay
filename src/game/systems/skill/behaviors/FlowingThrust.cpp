/**
 * @file FlowingThrust.cpp
 * @brief 流云刺 (ID 1) - 模块化突进穿透技能实现
 */
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "SevenStarSlashShared.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "raymath.h"
#include <algorithm>

namespace NoMoreDay::skills {

namespace FlowingThrustNodes {
constexpr uint32_t Pierce = 110; constexpr uint32_t Windrunner = 113; constexpr uint32_t Momentum = 114; constexpr uint32_t Shadow = 130;
constexpr uint32_t PrisonSlash = 133; constexpr uint32_t ElementShift = 170; constexpr uint32_t ElementBody = 171;
} // namespace FlowingThrustNodes

struct FlowingThrust : SkillBehaviorBase<FlowingThrust> {
  static constexpr uint32_t kSkillId = 1;

  static void DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
    auto *pos = registry.try_get<Position>(owner); auto *stats = registry.try_get<CombatStats>(owner); if (!pos) return;
    const auto link = seven_star_shared::ConsumeLinkBuffs(registry, owner, kSkillId, true, exec.cast_id);
    if (link.consume_returning_step) seven_star_shared::ApplyReturningStepOverride(registry, owner, kSkillId);
    const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId);
    BakedSkillProfile localProfile;
    if (!profile && registry.all_of<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
        if (spec.skill_id == kSkillId) { SkillSpecializationBaker::Bake(registry, owner, kSkillId, &spec, localProfile, nullptr); profile = &localProfile; break; }
      }
    }
    Vector2 startPos = {pos->x, pos->y}, dir = Vector2Normalize(Vector2Subtract(exec.target_pos, startPos));
    float speed = profile ? profile->delivery.speed : 400.0f;
    if (const auto *res = registry.try_get<BladeResourceComponent>(owner); res && res->kind == BladeResourceKind::SwordFlow) speed *= (1.0f + std::clamp(res->current, 0, 10) * 0.04f);
    if (auto *vel = registry.try_get<Velocity>(owner)) { vel->vx = dir.x * speed; vel->vy = dir.y * speed; }
    auto &mob = registry.emplace_or_replace<MobilityComponent>(owner); mob.owner = owner; mob.target_entity = owner; mob.direction = dir; mob.speed = speed; mob.duration = 0.375f; mob.leaves_motion_trail = true;
    if (auto *dash = registry.try_get<DashComponent>(owner)) { dash->isDashing = true; dash->dashTimer = 0.375f; dash->dirX = dir.x; dash->dirY = dir.y; dash->dashSpeed = speed; }
    bool forcePierce = profile ? ((profile->delivery.feature_flags & 1) != 0) : exec.active_nodes.test(FlowingThrustNodes::Pierce % 100);
    bool spawnShadow = profile ? ((profile->delivery.feature_flags & 2) != 0) : exec.active_nodes.test(FlowingThrustNodes::Shadow % 100);
    if (profile ? ((profile->delivery.feature_flags & 4) != 0) : exec.active_nodes.test(FlowingThrustNodes::Windrunner % 100)) {
      BuffEffect swift{.id = std::string(BuffIdToString(BuffId::SwordStep)), .duration = 2.0f, .remaining = 2.0f}; registry.get_or_emplace<ActiveEffectsComponent>(owner).AddOrRefresh(swift); registry.emplace_or_replace<PhaseTag>(owner);
    }
    if ((profile ? ((profile->effective_tags & (Tag::Fire | Tag::Cold | Tag::Lightning)) != Tag::None) : exec.active_nodes.test(FlowingThrustNodes::ElementBody % 100)) && profile && profile->effective_tags != Tag::Physical) {
      BuffEffect body{.id = std::string(BuffIdToString(BuffId::FlowingThrustElementBody)), .name = "Element Body", .type = BuffType::Shield, .duration = 1.5f, .remaining = 1.5f};
      body.modifiers.push_back({.value = 8.0f, .type = StatType::ResistAll, .mode = ModifierMode::Flat}); registry.get_or_emplace<ActiveEffectsComponent>(owner).AddOrRefresh(body);
    }
    if (!registry.any_of<ShadowComponent>(owner) && !registry.any_of<ShadowLifetime>(owner)) {
      if (spawnShadow) {
        SkillSystem::SpawnShadowEcho(registry, owner, kSkillId, startPos, exec.target_pos, stats, 0.3f, 0.15f, 1.5f, {50, 0, 80, 180}, exec.active_nodes);
      } else if (const auto *res = registry.try_get<BladeResourceComponent>(owner); res && res->kind == BladeResourceKind::SwordFlow && res->current >= 5) {
        SkillSystem::SpawnShadowEcho(registry, owner, kSkillId, startPos, exec.target_pos, stats, 0.22f, 0.08f, 1.0f, {120, 240, 255, 180}, exec.active_nodes);
        if (res->current >= 8) SkillSystem::SpawnShadowEcho(registry, owner, kSkillId, startPos, exec.target_pos, stats, 0.18f, 0.16f, 0.9f, {180, 255, 255, 170}, exec.active_nodes);
        if (res->current >= 10) SkillSystem::SpawnShadowEcho(registry, owner, kSkillId, startPos, exec.target_pos, stats, 0.14f, 0.24f, 0.85f, {255, 240, 180, 180}, exec.active_nodes);
      }
    }
    auto proj_ent = registry.create(); registry.emplace<LocalLevelTag>(proj_ent);
    registry.emplace<Position>(proj_ent, pos->x + dir.x * 24.0f, pos->y + dir.y * 24.0f);
    registry.emplace<Velocity>(proj_ent, dir.x * speed, dir.y * speed);
    auto &proj = registry.emplace<Projectile>(proj_ent); proj.owner = owner; proj.cast_id = exec.cast_id; proj.speed = speed; proj.lifeTime = 0.375f;
    proj.radius = exec.is_empowered ? 35.0f : 20.0f; proj.pierce = true; proj.pierceCount = forcePierce ? 999 : 99; if (forcePierce) proj.max_pierce = Projectile::kUnlimitedPiercing;
    float moreDamageMult = (profile ? profile->more_damage_mult : 1.0f) * (exec.is_empowered ? 1.5f : 1.0f);
    if (stats) {
      proj.snapshot = *stats;
      for (auto &mult : proj.snapshot.damage_multipliers) mult *= moreDamageMult;
      proj.payload_context = {.base_damage_min = stats->min_weapon_damage, .base_damage_max = stats->max_weapon_damage, .crit_chance = stats->crit_chance, .crit_multiplier = stats->crit_damage, .more_damage = moreDamageMult, .effective_tags = profile ? profile->effective_tags : Tag::Physical, .source_skill_id = kSkillId};
      registry.emplace<CombatStats>(proj_ent, proj.snapshot);
    }
    registry.emplace<SkillComponent>(proj_ent, kSkillId, owner);
    auto &ds = registry.emplace<DirectStrikeComponent>(proj_ent); ds.owner = owner; ds.cast_id = exec.cast_id; ds.skill_id = kSkillId; ds.radius = proj.radius; ds.lifetime = proj.lifeTime;
  }

  static void DoHit(entt::registry &reg, entt::entity attacker, entt::entity, Tag, bool is_crit) {
    if (const auto *res = reg.try_get<BladeResourceComponent>(attacker); res && res->kind == BladeResourceKind::SwordFlow) {
      (void)SkillSystem::GainSwordIntent(reg, attacker, 1, kSkillId);
      (void)systems::BladeResourceService::TryConsumeSwordFlowRestartWindow(reg, attacker, kSkillId);
      if (auto *act = reg.try_get<ActiveSkillsComponent>(attacker)) { for (auto &s : act->slots) { if (s.id == 2 && s.cooldown > 0.0f) s.cooldown = std::max(0.0f, s.cooldown - 0.75f); } }
    }
    if (is_crit) {
      if (auto *act = reg.try_get<ActiveSkillsComponent>(attacker)) {
        for (const auto &spec : act->specialized_slots) {
          if (spec.skill_id == kSkillId) {
            if (auto it = spec.allocated_points.find(FlowingThrustNodes::PrisonSlash); it != spec.allocated_points.end() && it->second > 0 && GetRandomValue(0, 100) < 20 * it->second) reg.get_or_emplace<ShadowKillArrayReady>(attacker);
            break;
          }
        }
      }
    }
  }
};
REGISTER_SKILL_BEHAVIOR(FlowingThrust)
void RegisterFlowingThrust() {}
} // namespace NoMoreDay::skills
