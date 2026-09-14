#include "game/systems/skill/AreaFieldDeliverySystem.hpp"
#include "game/contracts/CombatEvents.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/contracts/DamagePipelineTypes.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/data/BuffIds.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/SkillVfxEvent.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "raymath.h"
#include <vector>

namespace NoMoreDay {

namespace {
constexpr float kSkyfallImpactEffectiveness = 1.5f;
constexpr float kSkyfallResidualFieldDuration = 5.0f;
constexpr float kSkyfallResidualFieldPulseInterval = 0.3f;
constexpr float kSkyfallResidualFieldDamageMult = 0.5f;
constexpr float kDefaultAilmentDuration = 3.0f;
constexpr float kDefaultSkillVfxIntensity = 1.0f;
} // namespace

void AreaFieldDeliverySystem::Update(entt::registry &registry,
                                     systems::SpatialHashGrid &grid,
                                     float dt) {
  static thread_local std::vector<entt::entity> s_to_destroy;
  s_to_destroy.clear();

  auto view = registry.view<AreaFieldComponent, Position>();

  for (auto entity : view) {
    // 持久场原型（带 PersistentFieldTag）自管理脉冲伤害与清理回调。
    // 此处按标记跳过，避免与通用 AreaField 交付重复脉冲及销毁竞态。
    if (registry.any_of<PersistentFieldTag>(entity)) {
      continue;
    }

    auto &field = view.get<AreaFieldComponent>(entity);
    const auto &pos = view.get<Position>(entity);

    field.remaining_duration -= dt;
    if (auto *array = registry.try_get<SwordArrayComponent>(entity)) {
      array->duration = field.remaining_duration;
    }
    if (field.remaining_duration <= 0.0f) {
      s_to_destroy.push_back(entity);
      continue;
    }

    // 615 御剑阵威: 处于御剑步状态站在阵内，判定频率增加 20%..60%
    float timerAdv = dt;
    auto *array = registry.try_get<SwordArrayComponent>(entity);
    if (array && array->sword_step_frequency_bonus > 0.0f && registry.valid(field.owner)) {
      const auto *oPos = registry.try_get<Position>(field.owner);
      if (oPos) {
        const float d2 = Vector2DistanceSqr({oPos->x, oPos->y}, {pos.x, pos.y});
        if (d2 <= field.radius * field.radius) {
          bool isSwordStep = false;
          if (const auto *effects = registry.try_get<ActiveEffectsComponent>(field.owner)) {
            if (effects->Get(BuffId::SwordStep) != nullptr) isSwordStep = true;
          }
          if (!isSwordStep) {
            if (const auto *stance = registry.try_get<::MovementStanceComponent>(field.owner)) {
              if (stance->stance == ::MovementStance::SwordRiding) isSwordStep = true;
            }
          }
          if (isSwordStep) {
            timerAdv *= (1.0f + array->sword_step_frequency_bonus);
          }
        }
      }
    }

    field.timer += timerAdv;
    if (field.timer >= field.pulse_interval) {
      field.timer = 0.0f;

      if (field.payload_count == 0) {
        continue;
      }

      // 提交领域脉冲视觉表现
      if (field.source_skill_id != 0) {
        SkillVfxEvent vfx{};
        vfx.skillId = field.source_skill_id;
        vfx.castId = field.cast_id;
        vfx.type = SkillVfxEventType::CastImpact;
        vfx.origin = {pos.x, pos.y};
        vfx.target = {pos.x, pos.y};
        vfx.intensity = kDefaultSkillVfxIntensity;
        systems::GPUSkillEffectSystem::Get().SubmitSkillEvent(vfx);
      }

      const bool ownerIsEnemy = registry.valid(field.owner) && registry.any_of<EnemyTag>(field.owner);

      static thread_local std::vector<entt::entity> candidates;
      candidates.clear();
      grid.query({pos.x, pos.y}, field.radius, [&](entt::entity target, const Position &tPos) {
        if (!registry.valid(target) || target == field.owner || registry.any_of<KilledTag>(target)) {
          return;
        }

        const bool targetIsEnemy = registry.any_of<EnemyTag>(target);
        if (ownerIsEnemy == targetIsEnemy) {
          return;
        }

        const float dx = tPos.x - pos.x;
        const float dy = tPos.y - pos.y;
        if (dx * dx + dy * dy <= field.radius * field.radius) {
          candidates.push_back(target);
        }
      });

      // 672 九幽雷池: 不再是全域均摊，而是每隔 1s 向随机 2-3 名敌人劈下极高额单体瞬爆落雷 (673 增加目标)
      static thread_local std::vector<entt::entity> struckTargets;
      struckTargets.clear();
      if (array && array->is_lightning_pool) {
        if (!candidates.empty()) {
          const size_t numToPick = std::min(candidates.size(), static_cast<size_t>(std::max(1, array->lightning_targets)));
          for (size_t pick = 0; pick < numToPick; ++pick) {
            const size_t idx = static_cast<size_t>(utils::ThreadSafeRandom::GetInt(0, static_cast<int>(candidates.size() - 1)));
            struckTargets.push_back(candidates[idx]);
            // 随机抽中者与尾部交换后弹出，避免整池拷贝
            candidates[idx] = candidates.back();
            candidates.pop_back();
          }
        }
      } else {
        struckTargets = candidates;
      }

      // 672 落雷伤害倍率: 由机制表 (skill_mechanics.json 技能6/节点672) 驱动，缺失时回退 2.5
      const float strikeMult =
          (array && array->is_lightning_pool)
              ? data::SkillMechanicsRegistry::Get().GetFloat(6, 672, "strike_damage_mult", 2.5f)
              : 1.0f;

      for (auto target : struckTargets) {
        if (!registry.valid(target) || registry.any_of<KilledTag>(target)) continue;

        // 612 剑气共鸣: 当敌人同时处于多个剑阵重叠区域时，受到的伤害总增 (More) 20%..60%
        int overlapCount = 0;
        if (array && array->resonance_more_mult > 1.0f && registry.valid(field.owner)) {
          if (const auto *tPos = registry.try_get<Position>(target)) {
            for (auto otherArrEnt : registry.view<SwordArrayComponent, Position>()) {
              const auto &otherArr = registry.get<SwordArrayComponent>(otherArrEnt);
              if (otherArr.owner == field.owner) {
                const auto &oPos = registry.get<Position>(otherArrEnt);
                if (Vector2DistanceSqr({tPos->x, tPos->y}, {oPos.x, oPos.y}) <= otherArr.radius * otherArr.radius) {
                  overlapCount++;
                }
              }
            }
          }
        }

        const bool isBoss = registry.any_of<BossBattleComponent>(target);

        for (uint8_t i = 0; i < field.payload_count; ++i) {
          const auto &payload = field.payloads[i];
          if (payload.type == PayloadType::Damage) {
            DamageRequest req;
            req.attacker = field.owner;
            req.defender = target;
            req.skill_id = field.source_skill_id;
            req.source_entity = entity;
            req.added_effectiveness = payload.value_mult;
            req.trigger_effectiveness = payload.value_mult;

            // 672 雷池高额单体落雷
            if (array && array->is_lightning_pool) {
              req.added_effectiveness *= strikeMult;
              req.trigger_effectiveness *= strikeMult;
            }
            // 612 重叠增伤
            if (overlapCount > 1 && array && array->resonance_more_mult > 1.0f) {
              req.added_effectiveness *= array->resonance_more_mult;
              req.trigger_effectiveness *= array->resonance_more_mult;
            }
            // 633 对 Boss More 20%
            if (array && isBoss && array->boss_more_damage > 1.0f) {
              req.added_effectiveness *= array->boss_more_damage;
              req.trigger_effectiveness *= array->boss_more_damage;
            }

            req.additional_tags = payload.damage_tags;

            // 633 绝命法场: 斩杀生命值低于 12% 的非 Boss 敌人 (若血量已达线则直接处决，若伤害后达线亦处决)
            // 处决走统一死亡链 CombatSystem::KillEnemy (KilledTag/词缀死亡效果/击杀统计)，与正常致死路径语义一致
            auto tryExecute = [&]() -> bool {
              if (!array || !array->has_execute || !registry.valid(target) ||
                  registry.any_of<KilledTag>(target) || isBoss) {
                return false;
              }
              auto *hp = registry.try_get<HealthComponent>(target);
              if (!hp || hp->current <= 0.0f ||
                  hp->current > hp->max * array->execute_health_threshold_ratio) {
                return false;
              }
              hp->current = 0.0f;
              registry.emplace_or_replace<ExecutedTag>(target);
              CombatSystem::KillEnemy(registry, target, field.owner);
              CombatEvent execHit = CombatEventFactory::CreateSkillHit(
                  field.owner, target, field.source_skill_id, array->effective_tag);
              CombatEventDispatcher::Dispatch(registry, execHit);
              // 事件派发为同步调用，635 阵斩回响在同调用栈内完成判定；
              // 派发后立即清除 ExecutedTag，闭合标记生命周期，避免跨帧残留
              if (registry.all_of<ExecutedTag>(target)) {
                registry.remove<ExecutedTag>(target);
              }
              return true;
            };

            if (!tryExecute()) {
              auto result = ResolveDamage(registry, req, target);
              if (!result.target_killed) {
                (void)tryExecute();
              }
            }

            if (array) {
              // 670 焚天烈焰阵 & 671 炼狱余火: 熔岩命中必然附加多层烧灼，火阵内点燃伤害提升且离阵燃烧 3 秒
              if (array->is_fire_field && registry.valid(target) && !registry.any_of<KilledTag>(target)) {
                // 671 设计为「敌人离开火阵后继续燃烧 3 秒」，当前以「命中时延长点燃时长」近似实现：
                // 点燃本身不会因离阵消失，阵内命中续期≈持续燃烧；若未来引入 debuff linger 机制可迁移
                const float lingerBurn =
                    data::SkillMechanicsRegistry::Get().GetFloat(6, 671, "linger_burn_duration", 3.0f);
                systems::AilmentApplyRequest applyReq;
                applyReq.ailment = AilmentType::Ignite;
                applyReq.source = field.owner;
                applyReq.duration = array->burn_duration + (array->ignite_more_damage > 0.0f ? lingerBurn : 0.0f);
                applyReq.stacks = static_cast<uint16_t>(array->burn_stacks);
                applyReq.magnitude = array->base_ignite_magnitude * (1.0f + array->ignite_more_damage);
                (void)systems::AilmentApplier::Apply(registry, target, applyReq);
              }

              // 630 迟缓剑压: 减速 10%..40%, 持续 2.0s (BuffId::SwordArraySlow)
              if (array->has_slow && registry.valid(target) && !registry.any_of<KilledTag>(target)) {
                auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(target);
                BuffEffect slow;
                slow.id = std::string(BuffIdToString(BuffId::SwordArraySlow));
                slow.name = "Sword Array Slow";
                slow.type = BuffType::SpeedDown;
                slow.kind = BuffKind::Slow;
                slow.duration = (array->slow_duration > 0.0f) ? array->slow_duration : 2.0f;
                slow.remaining = slow.duration;
                slow.is_debuff = true;
                StatModifier mod;
                mod.type = StatType::MoveSpeed;
                mod.mode = ModifierMode::PercentAdd;
                mod.value = -array->slow_magnitude * 100.0f;
                mod.source = ModifierSource::Skill;
                slow.modifiers.push_back(mod);
                effects.AddOrRefresh(slow);
                (void)registry.get_or_emplace<StatsDirty>(target);
              }

              // 631 破甲剑意: 50%..200% 几率施加护甲击碎 (Armor Shred) 并正确按层数叠乘
              if (array->has_armor_shred && registry.valid(target) && !registry.any_of<KilledTag>(target)) {
                float chance = array->armor_shred_chance;
                int added_stacks = 0;
                while (chance >= 1.0f) {
                  ++added_stacks;
                  chance -= 1.0f;
                }
                if (chance > 0.0f && utils::ThreadSafeRandom::GetFloat01() <= chance) {
                  ++added_stacks;
                }
                if (added_stacks > 0) {
                  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(target);
                  std::string shredId = std::string(BuffIdToString(BuffId::SwordArrayArmorShred));
                  BuffEffect *existing = effects.Get(shredId);
                  const int maxSt = array->shred_max_stacks > 0 ? array->shred_max_stacks : 10;
                  const float shredVal = array->shred_armor_per_stack > 0.0f ? array->shred_armor_per_stack : 10.0f;
                  const float dur = array->shred_duration > 0.0f ? array->shred_duration : 4.0f;
                  if (existing) {
                    existing->duration = dur;
                    existing->remaining = dur;
                    existing->stacks = std::min(maxSt, existing->stacks + added_stacks);
                    existing->modifiers.clear();
                    StatModifier mod;
                    mod.type = StatType::Armor;
                    mod.mode = ModifierMode::Flat;
                    mod.value = -shredVal * static_cast<float>(existing->stacks);
                    mod.source = ModifierSource::Skill;
                    existing->modifiers.push_back(mod);
                  } else {
                    BuffEffect shred;
                    shred.id = shredId;
                    shred.name = "Sword Array Armor Shred";
                    shred.type = BuffType::DefenseDown;
                    shred.duration = dur;
                    shred.remaining = dur;
                    shred.stacks = std::min(maxSt, added_stacks);
                    shred.max_stacks = maxSt;
                    shred.is_debuff = true;
                    StatModifier mod;
                    mod.type = StatType::Armor;
                    mod.mode = ModifierMode::Flat;
                    mod.value = -shredVal * static_cast<float>(shred.stacks);
                    mod.source = ModifierSource::Skill;
                    shred.modifiers.push_back(mod);
                    effects.AddOrRefresh(shred);
                  }
                  (void)registry.get_or_emplace<StatsDirty>(target);
                }
              }

              // 632 虚弱领域: 敌人造成的全伤害 Less 6%..18% (PercentMult 百分比点数修饰)
              if (array->weaken_less_damage > 0.0f && registry.valid(target) && !registry.any_of<KilledTag>(target)) {
                auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(target);
                BuffEffect weaken;
                weaken.id = std::string(BuffIdToString(BuffId::SwordArrayWeaken));
                weaken.name = "Sword Array Weaken";
                weaken.type = BuffType::AttackDown;
                weaken.duration = 2.0f;
                weaken.remaining = 2.0f;
                weaken.is_debuff = true;
                for (auto st : {StatType::PhysicalDamage, StatType::FireDamage, StatType::ColdDamage, StatType::LightningDamage, StatType::PoisonDamage, StatType::ShadowDamage}) {
                  StatModifier mod;
                  mod.type = st;
                  mod.mode = ModifierMode::PercentMult;
                  mod.value = -array->weaken_less_damage * 100.0f;
                  mod.source = ModifierSource::Skill;
                  weaken.modifiers.push_back(mod);
                }
                effects.AddOrRefresh(weaken);
                (void)registry.get_or_emplace<StatsDirty>(target);
              }

              // 674 法阵侵蚀: 每秒施加 1..4 层降抗 (TypeB), 最多 10 层, 持续 3s (SkillOnly 挂载)
              if (array->corrosion_stacks_per_sec > 0.0f && registry.valid(target) && !registry.any_of<KilledTag>(target)) {
                auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(target);
                int addStacks = std::max(1, static_cast<int>(array->corrosion_stacks_per_sec));
                BuffEffect *existing = effects.Get(BuffId::SwordArrayCorrosion);
                const StatType resType = array->is_lightning_pool ? StatType::ResistLightning : (array->is_fire_field ? StatType::ResistFire : StatType::ResistPhysical);
                const int maxSt = array->corrosion_max_stacks > 0 ? array->corrosion_max_stacks : 10;
                const float shredRes = array->corrosion_resist_per_stack > 0.0f ? array->corrosion_resist_per_stack : 2.0f;
                const float dur = array->corrosion_linger_duration > 0.0f ? array->corrosion_linger_duration : 3.0f;
                if (existing) {
                  existing->duration = dur;
                  existing->remaining = dur;
                  existing->source_skill_id = static_cast<int>(field.source_skill_id);
                  existing->stacks = std::min(maxSt, existing->stacks + addStacks);
                  existing->modifiers.clear();
                  StatModifier mod;
                  mod.type = resType;
                  mod.mode = ModifierMode::Flat;
                  mod.value = -shredRes * static_cast<float>(existing->stacks);
                  mod.source = ModifierSource::Skill;
                  existing->modifiers.push_back(mod);
                } else {
                  BuffEffect corrosion;
                  corrosion.id = std::string(BuffIdToString(BuffId::SwordArrayCorrosion));
                  corrosion.name = "Sword Array Corrosion";
                  corrosion.type = BuffType::DefenseDown;
                  corrosion.duration = dur;
                  corrosion.remaining = dur;
                  corrosion.source_skill_id = static_cast<int>(field.source_skill_id);
                  corrosion.stacks = std::min(maxSt, addStacks);
                  corrosion.max_stacks = maxSt;
                  corrosion.is_debuff = true;
                  StatModifier mod;
                  mod.type = resType;
                  mod.mode = ModifierMode::Flat;
                  mod.value = -shredRes * static_cast<float>(corrosion.stacks);
                  mod.source = ModifierSource::Skill;
                  corrosion.modifiers.push_back(mod);
                  effects.AddOrRefresh(corrosion);
                }
                (void)registry.get_or_emplace<StatsDirty>(target);
              }
            }
          } else if (payload.type == PayloadType::Ailment) {
            if (payload.ailment_id != 0) {
              systems::AilmentApplyRequest applyReq;
              applyReq.ailment = static_cast<AilmentType>(payload.ailment_id);
              applyReq.source = field.owner;
              applyReq.magnitude = payload.value_mult;
              applyReq.duration = (payload.duration > 0.0f) ? payload.duration : kDefaultAilmentDuration;
              (void)systems::AilmentApplier::Apply(registry, target, applyReq);
            }
          }
        }
      }

      // 673 连珠落雷: 仅在点出 673 时，被雷击的敌人之间产生电弧造成额外闪电伤害
      if (array && array->is_lightning_pool && array->has_chain_lightning && struckTargets.size() > 1) {
        const float arcDmg = array->chain_lightning_damage_pct > 0.0f ? array->chain_lightning_damage_pct : 0.50f;
        for (size_t t1 = 0; t1 < struckTargets.size(); ++t1) {
          for (size_t t2 = t1 + 1; t2 < struckTargets.size(); ++t2) {
            auto entA = struckTargets[t1];
            auto entB = struckTargets[t2];
            if (!registry.valid(entA) || !registry.valid(entB)) continue;
            // A 和 B 互相受到连锁电弧伤害
            for (auto arcTarget : {entA, entB}) {
              DamageRequest req;
              req.attacker = field.owner;
              req.defender = arcTarget;
              req.skill_id = field.source_skill_id;
              req.source_entity = entity;
              req.added_effectiveness = arcDmg;
              req.trigger_effectiveness = arcDmg;
              req.additional_tags = Tag::Lightning;
              (void)ResolveDamage(registry, req, arcTarget);
            }
          }
        }
      }
    }
  }

  // 处理天降打击与流星轰击 (SkyfallImpactComponent - Task 2.6)
  auto skyfall_view = registry.view<SkyfallImpactComponent, Position>();
  for (auto entity : skyfall_view) {
    auto &skyfall = skyfall_view.get<SkyfallImpactComponent>(entity);
    auto &pos = skyfall_view.get<Position>(entity);
    skyfall.timer += dt;

    if (skyfall.waves_spawned < skyfall.wave_count) {
      const float nextWaveTime = skyfall.delay_before_impact + static_cast<float>(skyfall.waves_spawned) * skyfall.wave_interval;
      if (skyfall.timer >= nextWaveTime) {
        skyfall.waves_spawned++;

        grid.query({pos.x, pos.y}, skyfall.impact_radius, [&](entt::entity target, const Position &tPos) {
          if (!registry.valid(target) || target == skyfall.owner || registry.any_of<KilledTag>(target)) {
            return;
          }
          const bool ownerIsEnemy = registry.valid(skyfall.owner) && registry.any_of<EnemyTag>(skyfall.owner);
          const bool targetIsEnemy = registry.any_of<EnemyTag>(target);
          if (ownerIsEnemy != targetIsEnemy) {
            const auto *p = SkillSystem::GetBakedSkillProfile(registry, skyfall.owner, skyfall.skill_id);
            const float eff = (p && p->more_damage_mult > 0.0f)
                                  ? (kSkyfallImpactEffectiveness * p->more_damage_mult)
                                  : kSkyfallImpactEffectiveness;
            DamageRequest req;
            req.attacker = skyfall.owner;
            req.defender = target;
            req.skill_id = skyfall.skill_id;
            req.source_entity = entity;
            req.added_effectiveness = eff;
            if (p) {
              req.additional_tags = p->effective_tags;
            }
            (void)ResolveDamage(registry, req, target);
          }
        });
      }
    }

    if (skyfall.waves_spawned >= skyfall.wave_count) {
      if (skyfall.leave_field_skill_id != 0) {
        const auto *p = SkillSystem::GetBakedSkillProfile(registry, skyfall.owner, skyfall.leave_field_skill_id);
        const float resDuration = (p && p->delivery.duration > 0.0f)
                                      ? p->delivery.duration
                                      : kSkyfallResidualFieldDuration;
        const float resInterval = (p && p->delivery.sub_interval > 0.0f)
                                      ? p->delivery.sub_interval
                                      : kSkyfallResidualFieldPulseInterval;
        const float resDmgMult = (p && p->more_damage_mult > 0.0f)
                                     ? (kSkyfallResidualFieldDamageMult * p->more_damage_mult)
                                     : kSkyfallResidualFieldDamageMult;
        const float resRadius = (p && p->area_radius > 1.0f) ? p->area_radius : skyfall.impact_radius;
        AreaFieldComponent field{};
        field.owner = skyfall.owner;
        field.source_skill_id = skyfall.leave_field_skill_id;
        field.cast_id = skyfall.cast_id;
        field.remaining_duration = resDuration;
        field.pulse_interval = resInterval;
        field.radius = resRadius;
        field.payload_count = 1;
        field.payloads[0].type = PayloadType::Damage;
        field.payloads[0].value_mult = resDmgMult;
        if (p) {
          field.payloads[0].damage_tags = p->effective_tags;
        }
        registry.emplace_or_replace<AreaFieldComponent>(entity, field);
        registry.remove<SkyfallImpactComponent>(entity);
        continue;
      }
      s_to_destroy.push_back(entity);
    }
  }

  for (auto entity : s_to_destroy) {
    if (registry.valid(entity)) {
      registry.destroy(entity);
    }
  }
}

} // namespace NoMoreDay
