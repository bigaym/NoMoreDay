/**
 * @file BladeBoomerang.cpp
 * @brief 御剑回旋 (ID 8) - 模块化回旋镖交付实现
 */
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"
#include "raymath.h"

namespace NoMoreDay::skills {

namespace BladeBoomerangNodes {
constexpr uint32_t BreakAir = 812;
constexpr uint32_t PhantomSpin = 813;
constexpr uint32_t MagnetField = 830;
constexpr uint32_t CatchBlade = 831;
constexpr uint32_t GravityField = 832;
constexpr uint32_t BlackHole = 833;
constexpr uint32_t HoverCut = 850;
constexpr uint32_t Bleed = 851;
constexpr uint32_t Tear = 852;
constexpr uint32_t PathResidue = 870;
constexpr uint32_t GuardQi = 871;
} // namespace BladeBoomerangNodes

struct BladeBoomerang : SkillBehaviorBase<BladeBoomerang> {
  static constexpr uint32_t kSkillId = 8;
  static void DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
    auto *pos = registry.try_get<Position>(owner); auto *stats = registry.try_get<CombatStats>(owner); if (!pos || !stats) return;
    const auto link = seven_star_shared::ConsumeLinkBuffs(registry, owner, kSkillId, false, exec.cast_id);
    const auto *sd = SkillRegistry::Get().GetSkill(kSkillId);
    float speed = sd ? sd->GetParam("speed", 400.0f) : 400.0f;
    const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId);
    BakedSkillProfile localProfile;
    if (!profile && registry.all_of<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
        if (spec.skill_id == kSkillId) { SkillSpecializationBaker::Bake(registry, owner, kSkillId, &spec, localProfile, nullptr); profile = &localProfile; break; }
      }
    }
    float moreDmg = (profile ? profile->more_damage_mult : 1.0f) * link.damage_multiplier;
    int extra = (profile ? ((profile->delivery.feature_flags & 2) != 0) : exec.active_nodes.test(BladeBoomerangNodes::PhantomSpin % 100)) ? 2 : 0;
    bool hasCatch = profile ? ((profile->delivery.feature_flags & 8) != 0) : exec.active_nodes.test(BladeBoomerangNodes::CatchBlade % 100);
    bool hasHover = profile ? ((profile->delivery.feature_flags & 64) != 0) : exec.active_nodes.test(BladeBoomerangNodes::HoverCut % 100);
    float pullStr = 0.0f;
    if (profile ? ((profile->delivery.feature_flags & 4) != 0) : exec.active_nodes.test(BladeBoomerangNodes::MagnetField % 100)) pullStr += 300.0f;
    if (profile ? ((profile->delivery.feature_flags & 16) != 0) : exec.active_nodes.test(BladeBoomerangNodes::GravityField % 100)) pullStr += 500.0f;
    if (profile ? ((profile->delivery.feature_flags & 32) != 0) : exec.active_nodes.test(BladeBoomerangNodes::BlackHole % 100)) pullStr *= 2.0f;
    Tag convTag = profile ? (profile->effective_tags & ~Tag::Physical) : Tag::None;
    Color col = ORANGE;
    if (convTag == Tag::Lightning || (profile ? ((profile->delivery.feature_flags & 1024) != 0) : false)) {
      convTag = Tag::Lightning; col = PURPLE;
      BuffEffect g{.id = std::string(BuffIdToString(BuffId::BladeBoomerangGuardQi)), .name = "Guard Qi", .type = BuffType::Shield, .duration = 2.0f, .remaining = 2.0f};
      g.modifiers.push_back({.value = 10.0f, .type = StatType::ResistAll, .mode = ModifierMode::Flat}); registry.get_or_emplace<ActiveEffectsComponent>(owner).AddOrRefresh(g);
    } else if (convTag == Tag::Fire || (profile ? ((profile->delivery.feature_flags & 512) != 0) : false)) { convTag = Tag::Fire; col = ORANGE; }

    Vector2 dir = Vector2Normalize(Vector2Subtract(exec.target_pos, {pos->x, pos->y}));
    auto spawnProj = [&](Vector2 p_dir, float p_scale) {
      auto e = registry.create(); registry.emplace<LocalLevelTag>(e); registry.emplace<Position>(e, pos->x, pos->y);
      registry.emplace<Velocity>(e, p_dir.x * speed, p_dir.y * speed); registry.emplace<ColorComponent>(e, col);
      auto &p = registry.emplace<Projectile>(e);
      p.owner = owner; p.cast_id = exec.cast_id; p.speed = speed; p.lifeTime = 3.0f; p.radius = (sd ? sd->GetParam("radius", 40.0f) : 40.0f) * p_scale;
      p.pierce = true; p.pierceCount = 99; p.max_pierce = Projectile::kUnlimitedPiercing; p.snapshot = *stats; p.hasPull = pullStr > 0.0f; p.pullStrength = pullStr * p_scale;
      p.payload_context = {.base_damage_min = stats->min_weapon_damage, .base_damage_max = stats->max_weapon_damage, .crit_chance = stats->crit_chance, .crit_multiplier = stats->crit_damage, .more_damage = moreDmg * p_scale * (exec.is_empowered ? 1.5f : 1.0f), .effective_tags = Tag::Physical | convTag, .source_skill_id = exec.skill_id};
      registry.emplace<CombatStats>(e, p.snapshot); registry.emplace<SkillComponent>(e, exec.skill_id, owner);
      if (convTag != Tag::None) registry.emplace<SkillModifierComponent>(e).damage_modifiers.push_back({Tag::Physical, convTag, 1.0f, ModifierType::Convert});
      auto &bc = registry.emplace<BoomerangComponent>(e);
      bc.owner = owner; bc.cast_id = exec.cast_id; bc.skill_id = kSkillId; bc.phase = BoomerangPhase::Outward; bc.returnSpeed = hasCatch ? speed * 1.8f : speed * 1.5f;
      bc.returnTimer = hasCatch ? 0.38f : 0.45f; bc.hover_duration = hasHover ? 1.0f : 0.45f;
      bc.pull_radius = (pullStr > 0.0f) ? (p.radius * (exec.is_empowered ? 1.5f : 1.0f)) : 0.0f; bc.pull_strength = (pullStr > 0.0f) ? p.pullStrength : 0.0f; bc.catch_by_owner = true;
    };
    spawnProj(dir, 1.0f); if (extra > 0) { spawnProj(Vector2Rotate(dir, 0.25f), 0.6f); spawnProj(Vector2Rotate(dir, -0.25f), 0.6f); }
  }
  static void DoHit(entt::registry &registry, entt::entity attacker, entt::entity target, Tag, bool) {
    const auto *p = SkillSystem::GetBakedSkillProfile(registry, attacker, kSkillId);
    bool bleed = p ? ((p->delivery.feature_flags & 128) != 0) : false, tear = p ? ((p->delivery.feature_flags & 256) != 0) : false;
    if (!p && registry.all_of<ActiveSkillsComponent>(attacker)) {
      for (const auto &s : registry.get<ActiveSkillsComponent>(attacker).specialized_slots) {
        if (s.skill_id == kSkillId) { bleed = s.allocated_points.contains(BladeBoomerangNodes::Bleed); tear = s.allocated_points.contains(BladeBoomerangNodes::Tear); break; }
      }
    }
    if (bleed) {
      BuffEffect b{.id = std::string(BuffIdToString(BuffId::BladeBoomerangBleed)), .name = "Bleed", .type = BuffType::Bleed, .duration = 3.0f, .remaining = 3.0f, .is_debuff = true, .source = attacker};
      b.modifiers.push_back({.value = -10.0f, .type = StatType::MoveSpeed, .mode = ModifierMode::PercentAdd}); registry.get_or_emplace<ActiveEffectsComponent>(target).AddOrRefresh(b);
    }
    if (tear) { if (auto *fx = registry.try_get<ActiveEffectsComponent>(target); fx && fx->Get(BuffId::BladeBoomerangBleed)) SkillSystem::GainSwordIntent(registry, attacker, 1, kSkillId); }
  }
};
REGISTER_SKILL_BEHAVIOR(BladeBoomerang)
void RegisterBladeBoomerang() {}
} // namespace NoMoreDay::skills
