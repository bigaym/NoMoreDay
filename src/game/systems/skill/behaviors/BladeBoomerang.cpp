/**
 * @file BladeBoomerang.cpp
 * @brief 御剑回旋 (ID 8) - 回旋镖技能行为实现
 *
 * 天赋分支:
 * - 812 破空: 速度转增伤
 * - 813 幻影回旋: 额外飞剑
 * - 830-833 牵引机制: 磁力/重力/黑洞
 * - 850 滞空切割: 折返点常驻
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/Projectile.hpp"
#include "game/data/SkillRegistry.hpp"


#include "core/logging/Logger.hpp"
#include "game/components/SkillDefs.hpp" // For SwordIntent
#include "raymath.h"


namespace NoMoreDay::skills {

struct BladeBoomerang : SkillBehaviorBase<BladeBoomerang> {
  static constexpr uint32_t kSkillId = 8;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec) {
    auto *pos = registry.try_get<Position>(owner);
    auto *stats = registry.try_get<CombatStats>(owner);
    if (!pos || !stats)
      return;

    const auto *skillData = SkillRegistry::Get().GetSkill(8);
    float speed = skillData ? skillData->GetParam("speed", 400.0f) : 400.0f;
    float returnTimer =
        skillData ? skillData->GetParam("return_timer", 0.45f) : 0.45f;
    float radius = skillData ? skillData->GetParam("radius", 40.0f) : 40.0f;
    float basePull =
        skillData ? skillData->GetParam("pull_strength", 300.0f) : 300.0f;
    float gravityPull =
        skillData ? skillData->GetParam("gravity_strength", 500.0f) : 500.0f;

    Vector2 dir =
        Vector2Normalize(Vector2Subtract(exec.target_pos, {pos->x, pos->y}));

    bool hasPull = false;
    float pullStrength = 0.0f;
    int extraProjectiles = 0;
    float moreDamageFromSpeed = 1.0f;

    bool hasZhiKong = false;
    if (auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id == kSkillId) {
          // Talent: Po Kong (破空) - ID 812
          if (spec.allocated_points.contains(812)) {
            float bonus =
                (speed / 100.0f) * 0.1f * spec.allocated_points.at(812);
            moreDamageFromSpeed += bonus;
          }

          // Talent: Huan Ying Hui Xuan (幻影回旋) - ID 813
          if (spec.allocated_points.contains(813) &&
              spec.allocated_points.at(813) > 0) {
            extraProjectiles = 2;
          }

          // Talent: Ci Li Chang (磁力场) - ID 830
          if (spec.allocated_points.contains(830) &&
              spec.allocated_points.at(830) > 0) {
            hasPull = true;
            pullStrength = basePull;
          }

          // Talent: Zhong Li Chang (重力场) - ID 832
          if (spec.allocated_points.contains(832)) {
            pullStrength += gravityPull * spec.allocated_points.at(832);
          }

          // Talent: Jian Qi Hei Dong (剑气黑洞) - ID 833
          if (spec.allocated_points.contains(833) &&
              spec.allocated_points.at(833) > 0) {
            pullStrength *= 2.0f; // Black hole effect
            radius *= 1.5f;
          }

          // Talent: Zhi Kong Qie Ge (滞空切割) - ID 850
          if (spec.allocated_points.contains(850)) {
            hasZhiKong = true;
          }
          break;
        }
      }
    }

    auto &particleSys = systems::GPUParticleSystem::Get();
    auto splash = systems::InkEffectHelper::CreateInkSplash({pos->x, pos->y},
                                                            10, 10.0f, 100.0f);
    for (auto &p : splash) {
      p.velocity = Vector2Add(p.velocity, Vector2Scale(dir, 200.0f));
      particleSys.Emit(p);
    }

    auto spawnProj = [&](Vector2 p_dir, float p_scale) {
      auto proj_ent = registry.create();
      registry.emplace<LocalLevelTag>(proj_ent);
      registry.emplace<Position>(proj_ent, pos->x, pos->y);
      registry.emplace<Velocity>(proj_ent, p_dir.x * speed, p_dir.y * speed);
      registry.emplace<ColorComponent>(proj_ent, ORANGE);

      auto &proj = registry.emplace<Projectile>(proj_ent);
      proj.owner = owner;
      proj.cast_id = exec.cast_id;
      proj.speed = speed;
      proj.lifeTime = 3.0f;
      proj.radius = radius * p_scale;
      proj.pierce = true;
      proj.pierceCount = 99;
      proj.snapshot = *stats;
      proj.hasPull = hasPull;
      proj.pullStrength = pullStrength * p_scale;

      for (auto &mult : proj.snapshot.damage_multipliers) {
        mult *= moreDamageFromSpeed * p_scale;
        if (exec.is_empowered)
          mult *= 1.5f;
      }

      if (exec.is_empowered) {
        proj.radius *= 1.5f;
        proj.pullStrength += 300.0f;
        proj.hasPull = true;
      }

      registry.emplace<CombatStats>(proj_ent, proj.snapshot);
      registry.emplace<SkillComponent>(proj_ent, exec.skill_id, owner);

      auto &bc = registry.emplace<BoomerangComponent>(proj_ent);
      bc.owner = owner;
      bc.returnTimer = returnTimer;
      bc.phase = BoomerangComponent::Outward;
      bc.returnSpeed = speed * 1.5f;

      if (hasZhiKong) {
        // Talent 850 Logic handled in BoomerangSystem,
        // but we could set a flag here if needed.
      }
    };

    spawnProj(dir, 1.0f);
    if (extraProjectiles > 0) {
      spawnProj(Vector2Rotate(dir, 0.25f), 0.6f);
      spawnProj(Vector2Rotate(dir, -0.25f), 0.6f);
    }
    LOG_INFO("Blade Boomerang fired by entity {}", (uint32_t)owner);
  }

  static void DoHit(entt::registry &registry, entt::entity attacker,
                    entt::entity target, Tag hit_tags, bool is_crit) {
    if (auto *active = registry.try_get<ActiveSkillsComponent>(attacker)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id == kSkillId) {
          // Talent: Fang Xue (放血) - ID 851
          if (spec.allocated_points.contains(851) &&
              spec.allocated_points.at(851) > 0) {
            auto &effects =
                registry.get_or_emplace<ActiveEffectsComponent>(target);
            BuffEffect bleed;
            bleed.id = "blade_boomerang_bleed";
            bleed.name = "Bleed";
            bleed.type = BuffType::Bleed;
            bleed.duration = 3.0f;
            bleed.remaining = 3.0f;
            bleed.is_debuff = true;
            bleed.source = attacker;

            // Slow effect (SpeedDown)
            float slowAmount = 10.0f * spec.allocated_points.at(851);
            bleed.modifiers.push_back({.value = -slowAmount,
                                       .type = StatType::MoveSpeed,
                                       .mode = ModifierMode::PercentAdd});

            effects.AddOrRefresh(bleed);
          }
          break;
        }
      }
    }
  }
};

REGISTER_SKILL_BEHAVIOR(BladeBoomerang)

void RegisterBladeBoomerang() {}

} // namespace NoMoreDay::skills
