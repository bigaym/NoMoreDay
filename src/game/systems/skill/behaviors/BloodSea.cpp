#include "game/systems/skill/behaviors/BloodSea.hpp"

#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include "core/logging/Logger.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EffectComponent.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/SkillPointAccess.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/contracts/CombatEvents.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/systems/skill/BladeResourceService.hpp"

#include <algorithm>
#include <string_view>

namespace NoMoreDay::skills {

namespace {

constexpr std::string_view kBloodSeaActiveBuffId = "blood_sea_active";

// 近身判定半径比例：几何常量，不参与节点调平。
constexpr float kCloseRadiusRatio = 0.45f;
// 1211 开场脉冲半径比例：几何常量，不参与节点调平。
constexpr float kBurstRadiusRatio = 0.7f;

bool IsBloodSeaLinkableSkill(const uint32_t skill_id) {
  return skill_id == 1 || skill_id == 4 || skill_id == 7 || skill_id == 8 ||
         skill_id == 9;
}

bool IsInsideBloodSeaField(const Position &target_pos, const Position &field_pos,
                           const float radius) {
  const float dx = target_pos.x - field_pos.x;
  const float dy = target_pos.y - field_pos.y;
  return (dx * dx + dy * dy) <= (radius * radius);
}

std::string ResolveBloodSeaActiveDescription(const BloodSeaFieldComponent &field) {
  std::string description;
  if (field.torrent_form) {
    description = "Torrent";
  } else if (field.ring_form) {
    description = "Ring";
  } else {
    description = "Crimson Field";
  }

  if (field.has_void_keystone) {
    description += " | Void Miasma";
  } else if (field.header.has_linked_synergy) {
    description += " | Linked Pulse";
  } else if (field.has_trigger_burst) {
    description += " | Opening Burst";
  } else if (field.has_recovery_keystone) {
    description += " | Blood Return";
  }

  return description;
}

void SyncBloodSeaActiveBuff(entt::registry &registry, const entt::entity owner,
                            const BloodSeaFieldComponent &field) {
  if (!registry.valid(owner)) {
    return;
  }

  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
  if (auto *existing = effects.Get(kBloodSeaActiveBuffId)) {
    const bool isFreshCast = field.header.duration > (existing->remaining + 0.05f);
    existing->name = "Blood Sea";
    existing->description = ResolveBloodSeaActiveDescription(field);
    existing->type = BuffType::BloodSea;
    existing->remaining = field.header.duration;
    if (isFreshCast) {
      existing->duration = field.header.duration;
    }
    existing->max_stacks = 1;
    existing->stacks = 1;
    existing->is_debuff = false;
    existing->source = owner;
    return;
  }

  BuffEffect buff;
  buff.id = std::string(kBloodSeaActiveBuffId);
  buff.name = "Blood Sea";
  buff.description = ResolveBloodSeaActiveDescription(field);
  buff.type = BuffType::BloodSea;
  buff.duration = field.header.duration;
  buff.remaining = field.header.duration;
  buff.max_stacks = 1;
  buff.is_debuff = false;
  buff.source = owner;
  effects.AddOrRefresh(buff);
}

void ClearBloodSeaActiveBuff(entt::registry &registry, const entt::entity owner) {
  if (!registry.valid(owner)) {
    return;
  }
  if (auto *effects = registry.try_get<ActiveEffectsComponent>(owner)) {
    effects->Remove(kBloodSeaActiveBuffId);
  }
}

DamagePool BuildBloodSeaDamagePool(const BloodSeaFieldComponent &field,
                                   const float total_damage) {
  DamagePool pool;
  float physical_ratio =
      GetMech(kBloodSeaSkillId, 0u, "physical_ratio_base", 0.6f);
  if (field.has_void_keystone) {
    physical_ratio = GetMech(kBloodSeaSkillId, BloodSeaNodes::VoidErosionMiasma,
                             "physical_ratio", 0.45f);
  }
  if (field.ring_form) {
    physical_ratio -= GetMech(kBloodSeaSkillId, BloodSeaNodes::BloodRingDevour,
                              "physical_ratio_delta", 0.05f);
  }
  physical_ratio = std::clamp(
      physical_ratio, GetMech(kBloodSeaSkillId, 0u, "physical_ratio_min", 0.25f),
      GetMech(kBloodSeaSkillId, 0u, "physical_ratio_max", 0.8f));
  pool.Add(Tag::Physical, total_damage * physical_ratio);
  pool.Add(Tag::Void, total_damage * (1.0f - physical_ratio));
  return pool;
}

Tag BuildBloodSeaDamageTags() {
  return Tag::Spell | Tag::Area | Tag::DamageOverTime | Tag::SwordSkill |
         Tag::Physical | Tag::Void;
}

std::vector<entt::entity> CollectBloodSeaTargets(entt::registry &registry,
                                                 const Position &field_pos,
                                                 const float radius,
                                                 const entt::entity owner,
                                                 const entt::entity field_entity) {
  std::vector<entt::entity> targets;
  auto view = registry.view<EnemyTag, Position>();
  for (const entt::entity target : view) {
    if (target == owner || target == field_entity ||
        registry.any_of<KilledTag>(target)) {
      continue;
    }
    const auto &target_pos = view.get<Position>(target);
    if (IsInsideBloodSeaField(target_pos, field_pos, radius)) {
      targets.push_back(target);
    }
  }
  return targets;
}

void ApplyResistShred(entt::registry &registry, const entt::entity target,
                      const BloodSeaFieldComponent &field) {
  if (field.resist_shred <= 0.0f) {
    return;
  }

  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(target);
  BuffEffect debuff;
  debuff.id = std::string(BuffIdToString(BuffId::BloodSeaMiasma));
  debuff.name = "Blood Sea Miasma";
  debuff.type = BuffType::DefenseDown;
  debuff.duration = GetMech(kBloodSeaSkillId, 0u, "miasma_base_duration", 1.0f) +
                    field.miasma_duration_bonus;
  debuff.remaining = debuff.duration;
  debuff.is_debuff = true;

  debuff.modifiers.push_back({.value = -field.resist_shred,
                              .type = StatType::ResistPhysical,
                              .mode = ModifierMode::Flat});
  debuff.modifiers.push_back({.value = -field.resist_shred,
                              .type = StatType::ResistShadow,
                              .mode = ModifierMode::Flat});
  effects.AddOrRefresh(debuff);
}

float ApplyHealing(entt::registry &registry, const entt::entity owner,
                   const float attempted_heal) {
  auto *stats = registry.try_get<CombatStats>(owner);
  if (stats == nullptr || attempted_heal <= 0.0f) {
    return 0.0f;
  }

  // 981 逆脉: 锁血禁疗期间禁止一切治疗 (含血海吸血/回复)
  // 禁疗×增疗交互：逆脉/免死窗口内禁疗，故默认 3s 窗口下 1217 的 +20% 治疗增益不体现
  // 实际治疗量；仅当窗口在 2s 增疗窗内结束时（逆脉剩余 < 增疗时长）才部分生效。
  if (const auto *pt = registry.try_get<PhantomTranceComponent>(owner)) {
    if (IsDeathSealActive(*pt)) {
      return 0.0f;
    }
  }

  const float previous = stats->health;
  stats->health = std::min(stats->max_health, stats->health + attempted_heal);
  const float actual = stats->health - previous;
  if (actual > 0.0f) {
    if (auto *hp = registry.try_get<HealthComponent>(owner)) {
      hp->current = stats->health;
    }
    registry.emplace_or_replace<StatsDirty>(owner);
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateOnHeal(owner, owner, actual));
  }
  return actual;
}

float DealPulse(entt::registry &registry, const entt::entity field_entity,
                BloodSeaFieldComponent &field,
                const std::vector<entt::entity> &targets,
                const float base_damage) {
  float total_applied_damage = 0.0f;
  const auto *field_pos = registry.try_get<Position>(field_entity);
  float nearest_distance_sq = std::numeric_limits<float>::max();
  if (field_pos != nullptr) {
    for (const entt::entity target : targets) {
      if (const auto *target_pos = registry.try_get<Position>(target)) {
        const float dx = target_pos->x - field_pos->x;
        const float dy = target_pos->y - field_pos->y;
        nearest_distance_sq = std::min(nearest_distance_sq, dx * dx + dy * dy);
      }
    }
  }

  for (const entt::entity target : targets) {
    float target_damage = base_damage * field.bonus_damage_mult;
    if (field_pos != nullptr) {
      if (const auto *target_pos = registry.try_get<Position>(target)) {
        const float dx = target_pos->x - field_pos->x;
        const float dy = target_pos->y - field_pos->y;
        const float distance_sq = dx * dx + dy * dy;
        if (distance_sq <= nearest_distance_sq + 1.0f) {
          target_damage *= 1.0f + field.pursuit_bonus_mult;
        }
        const float close_radius = field.header.radius * kCloseRadiusRatio;
        if (distance_sq <= close_radius * close_radius) {
          target_damage *= 1.0f + field.close_pressure_bonus_mult;
        }
      }
    }

    // 1217 绝影共噬窗口：前 N 秒内血海脉冲伤害提高（数据驱动倍率）。
    if (field.shared_devour_timer > 0.0f) {
      target_damage *= 1.0f + field.shared_devour_damage_mult;
    }

    DamageRequest request;
    request.origin = DamageOrigin::HazardEnvironment;
    request.attacker = field.header.owner;
    request.defender = target;
    request.skill_id = BloodSea::kSkillId;
    request.base_pool = BuildBloodSeaDamagePool(field, target_damage);
    request.additional_tags = BuildBloodSeaDamageTags();
    request.source_entity = field_entity;
    const auto result = ResolveDamage(registry, request, field.header.owner);
    total_applied_damage += result.damage.total_damage;

    if (field.aftershock_bonus_mult > 0.0f) {
      DamageRequest aftershock = request;
      aftershock.base_pool =
          BuildBloodSeaDamagePool(field, target_damage * field.aftershock_bonus_mult);
      const auto aftershock_result =
          ResolveDamage(registry, aftershock, field.header.owner);
      total_applied_damage += aftershock_result.damage.total_damage;
    }

    ApplyResistShred(registry, target, field);
  }

  float attempted_heal = total_applied_damage * field.leech_ratio;
  // 1217 绝影共噬窗口：同期治疗（吸血）效率同步提高；逆脉禁疗仍由 ApplyHealing 优先拦截。
  if (field.shared_devour_timer > 0.0f) {
    attempted_heal *= 1.0f + field.shared_devour_heal_mult;
  }
  const float actual_heal = ApplyHealing(registry, field.header.owner, attempted_heal);
  if (field.has_recovery_keystone) {
    const bool gained = systems::BladeResourceService::TryGainBloodthirstFromOverflowHeal(
        registry, field.header.owner, attempted_heal, actual_heal, BloodSea::kSkillId);
    if (gained && field.return_empower_bonus_mult > 0.0f) {
      field.return_empower_timer = std::max(
          field.return_empower_timer,
          GetMech(kBloodSeaSkillId, BloodSeaNodes::LifeHuntReturn,
                  "return_empower_duration", 1.5f));
    }
  }
  return total_applied_damage;
}

} // namespace

void BloodSea::DoCast(entt::registry &registry, entt::entity owner,
                      SkillExecution &exec) {
  const BloodSeaCastSpec spec = ResolveBloodSeaCastSpec(registry, owner);
  const int consumed = systems::BladeResourceService::ConsumeAll(registry, owner, kSkillId);
  const int effective_consumed = std::max(1, consumed);
  const auto *skill = SkillRegistry::Get().GetSkill(kSkillId);

  const entt::entity field_entity = registry.create();
  registry.emplace<LocalLevelTag>(field_entity);
  if (const auto *owner_pos = registry.try_get<Position>(owner)) {
    registry.emplace<Position>(field_entity, owner_pos->x, owner_pos->y);
  } else {
    registry.emplace<Position>(field_entity, exec.target_pos.x, exec.target_pos.y);
  }
  registry.emplace<ColorComponent>(field_entity, Color{150, 24, 32, 220});
  registry.emplace<SkillComponent>(field_entity, kSkillId, owner);

  auto &field = registry.emplace<BloodSeaFieldComponent>(field_entity);
  registry.emplace<PersistentFieldTag>(field_entity); // 持久场原型标记：交付系统据此跳过自管理脉冲
  field.header.owner = owner;
  field.consumed_bloodthirst = effective_consumed;
  field.header.duration =
      (skill ? skill->GetParam("field_duration", spec.fieldDurationDefault)
             : spec.fieldDurationDefault) +
      spec.fieldDurationPerBloodthirst * static_cast<float>(effective_consumed);
  field.header.radius =
      (skill ? skill->GetParam("field_radius", spec.fieldRadiusDefault)
             : spec.fieldRadiusDefault) +
      static_cast<float>(effective_consumed) * spec.fieldRadiusPerBloodthirst;
  field.header.tick_interval =
      skill ? skill->GetParam("field_tick", spec.fieldTickDefault)
            : spec.fieldTickDefault;
  field.bonus_damage_mult =
      1.0f + static_cast<float>(effective_consumed) *
                 (skill ? skill->GetParam("bloodthirst_damage_bonus",
                                          spec.bloodthirstDamageBonusDefault)
                        : spec.bloodthirstDamageBonusDefault);
  field.leech_ratio =
      skill ? skill->GetParam("leech_ratio", spec.leechRatioDefault)
            : spec.leechRatioDefault;
  field.resist_shred =
      spec.resistShredPerPoint * static_cast<float>(spec.miasmaShredPoints);
  field.has_trigger_burst = spec.triggerBurst;
  // 联动脉冲门控裁决：设计 §5.3:1236 把「血海内近战/御剑命中追加小额血爆」定义为节点
  // 1207 无间血狱，旧实现误挂在 1217 绝影共噬上。此处改挂 1207，与 1217 解耦；
  // 既不删除既有联动脉冲行为，也不无条件放大它。header 字段仍沿用通用「联动能力」语义。
  field.header.has_linked_synergy = spec.bottomlessPurgatory;
  field.has_recovery_keystone = spec.recoveryKeystone;
  field.has_void_keystone = spec.voidKeystone;
  field.torrent_form = spec.torrentForm;
  field.ring_form = spec.ringForm;

  // 1217 绝影共噬（设计 §5.3:1252）：仅当已点 1217 且施放瞬间技能9 逆脉/免死窗口处于
  // 激活态时，才把「前 N 秒增伤/增疗」窗口写入新场。窗口存在性统一走 IsDeathSealActive，
  // 与 ApplyHealing 的禁疗口径一致，避免两处窗口判定漂移。
  //
  // 禁疗×增疗交互：逆脉/免死窗口内禁疗，故默认 3s 窗口下 +20% 治疗增益不体现实际治疗量；
  // 仅当窗口在 2s 增疗窗内结束时（逆脉剩余 < 增疗时长）才部分生效。
  if (spec.sharedDevouring) {
    if (const auto *trance = registry.try_get<PhantomTranceComponent>(owner);
        trance != nullptr && IsDeathSealActive(*trance)) {
      field.shared_devour_timer = spec.empowerDuration;
      field.shared_devour_damage_mult = spec.empowerDamageMult;
      field.shared_devour_heal_mult = spec.empowerHealMult;
    }
  }

  float health_ratio = 1.0f;
  if (const auto *stats = registry.try_get<CombatStats>(owner)) {
    if (stats->max_health > 0.0f) {
      health_ratio = stats->health / stats->max_health;
    }
  }
  const bool is_low_life = health_ratio <= spec.lowLifeThreshold;

  field.header.radius += static_cast<float>(spec.bloodCurtainOpeningPoints) *
                         spec.radiusPerPoint;
  field.header.duration += static_cast<float>(spec.lingeringBloodMistPoints) *
                           spec.durationPerPoint;
  field.bonus_damage_mult *=
      1.0f + static_cast<float>(spec.pressureTideRisePoints) * spec.damagePerPoint;
  field.bonus_damage_mult += static_cast<float>(effective_consumed) *
                             static_cast<float>(spec.bloodthirstEdgePoints) *
                             spec.damagePerPointPerBloodthirst;
  field.move_follow_speed +=
      static_cast<float>(spec.bloodMistPursuitPoints) * spec.moveSpeedPerPoint;
  field.leech_ratio += static_cast<float>(effective_consumed) *
                       static_cast<float>(spec.bloodDrinkingTidePoints) *
                       spec.leechPerPointPerBloodthirst;
  field.leech_ratio += static_cast<float>(spec.bloodWaveRedrinkPoints) *
                       spec.leechPerPoint;
  field.pursuit_bonus_mult = static_cast<float>(spec.huntingBloodTrailPoints) *
                             spec.pursuitDamagePerPoint;
  field.aftershock_bonus_mult =
      static_cast<float>(spec.severedVeinAftershockPoints) *
      spec.aftershockDamagePerPoint;
  field.return_empower_bonus_mult = static_cast<float>(spec.lifeHuntReturnPoints) *
                                    spec.empowerBonusPerPoint;
  field.close_pressure_bonus_mult = static_cast<float>(spec.huntingMiasmaPoints) *
                                    spec.closePressurePerPoint;
  field.linked_pressure_bonus_mult =
      static_cast<float>(spec.huntingBloodPressurePoints) *
      spec.linkedPressurePerPoint;
  field.miasma_duration_bonus = static_cast<float>(spec.boneGnawingEmberPoints) *
                                spec.miasmaDurationPerPoint;
  field.void_damage_bonus_mult = static_cast<float>(spec.boneGnawingEmberPoints) *
                                 spec.voidDamagePerPoint;

  if (is_low_life) {
    field.bonus_damage_mult *=
        1.0f + static_cast<float>(spec.oppressiveEndPoints) *
                   spec.lowLifeDamagePerPoint;
    const float low_life_pressure = std::clamp(
        (spec.lowLifeThreshold - health_ratio) / spec.lowLifeThreshold, 0.0f, 1.0f);
    field.bonus_damage_mult *=
        1.0f + static_cast<float>(spec.dyingEdgePoints) *
                   spec.lowLifePressureDamagePerPoint * low_life_pressure;
    field.leech_ratio += static_cast<float>(spec.desperateReclaimPoints) *
                         spec.lowLifeLeechPerPoint;
  }

  if (field.has_recovery_keystone) {
    field.leech_ratio += spec.recoveryLeechBonus;
  }
  if (field.has_void_keystone) {
    field.bonus_damage_mult *= spec.voidDamageMult;
    field.resist_shred += spec.voidResistShredBonus;
  }
  if (field.torrent_form) {
    field.move_follow_speed = spec.torrentMoveSpeed;
    field.header.radius *= spec.torrentRadiusMult;
    field.header.tick_interval *= spec.torrentTickIntervalMult;
  }
  if (field.ring_form) {
    field.header.radius *= spec.ringRadiusMult;
    field.leech_ratio += spec.ringLeechBonus;
    field.bonus_damage_mult *= spec.ringDamageMult;
  }
  if (spec.bottomlessPurgatory) {
    field.bonus_damage_mult *= spec.bottomlessDamageMult;
  }
  field.header.tick_interval *=
      std::max(spec.tickIntervalFloor,
               1.0f - spec.tickIntervalReductionPerPoint *
                          static_cast<float>(spec.bladeMistResonancePoints));

  if (field.has_trigger_burst) {
    // Node 1211 (FreshBloodReturn / 鲜血回灌): Gain 2 Bloodthirst and heal for 10% of missing health.
    systems::BladeResourceService::Gain(
        registry, owner, static_cast<int>(spec.burstBloodthirstGain), kSkillId);
    if (auto* stats = registry.try_get<CombatStats>(owner)) {
      const float missing_health = std::max(0.0f, stats->max_health - stats->health);
      // 与 DealPulse 吸血口径一致：1211 血爆回复同样仅在绝影共噬窗口内吃增疗乘数，
      // 门控同一状态源 shared_devour_timer；施放瞬间若处于逆脉禁疗窗口，实际治疗
      // 仍会被 ApplyHealing 归零。
      float burst_heal = missing_health * spec.missingHealthHealRatio;
      if (field.shared_devour_timer > 0.0f) {
        burst_heal *= 1.0f + field.shared_devour_heal_mult;
      }
      ApplyHealing(registry, owner, burst_heal);
    }

    std::vector<entt::entity> burst_targets;
    auto view = registry.view<EnemyTag, Position>();

    const auto &field_pos = registry.get<Position>(field_entity);
    const float burst_radius = field.header.radius * kBurstRadiusRatio;
    for (const entt::entity target : view) {
      if (registry.any_of<KilledTag>(target)) {
        continue;
      }
      const auto &target_pos = view.get<Position>(target);
      if (IsInsideBloodSeaField(target_pos, field_pos, burst_radius)) {
        burst_targets.push_back(target);
      }
    }
    if (!burst_targets.empty()) {
      ++field.pulses_triggered;
      (void)DealPulse(registry, field_entity, field, burst_targets,
                      spec.burstBaseDamage +
                          spec.burstDamagePerBloodthirst *
                              static_cast<float>(effective_consumed));
    }
  }

  SyncBloodSeaActiveBuff(registry, owner, field);

  auto &area_field = registry.emplace<AreaFieldComponent>(field_entity);
  area_field.owner = owner;
  area_field.cast_id = exec.cast_id;
  area_field.source_skill_id = kSkillId;
  area_field.remaining_duration = field.header.duration;
  area_field.pulse_interval = field.header.tick_interval;
  area_field.timer = 0.0f;
  area_field.radius = field.header.radius;
  area_field.shape_type = field.ring_form ? 1 : 0;

  PayloadDefinition pdef{};
  pdef.type = PayloadType::Damage;
  pdef.value_mult = field.bonus_damage_mult;
  pdef.damage_tags = Tag::Physical | Tag::Area | (field.has_void_keystone ? Tag::Void : Tag::None);
  area_field.payloads[0] = pdef;
  area_field.payload_count = 1;

  LOG_INFO("Blood Sea cast: consumed={} radius={:.1f}", effective_consumed,
           field.header.radius);
}

void BloodSea::UpdateField(entt::registry &registry, entt::entity entity,
                           BloodSeaFieldComponent &field, float dt,
                           const systems::SpatialHashGrid &grid) {
  auto *field_pos = registry.try_get<Position>(entity);
  auto *owner_pos = registry.try_get<Position>(field.header.owner);
  if (field_pos == nullptr || owner_pos == nullptr || !registry.valid(field.header.owner)) {
    ClearBloodSeaActiveBuff(registry, field.header.owner);
    if (registry.valid(entity)) {
      registry.destroy(entity);
    }
    return;
  }

  field.header.duration -= dt;
  if (auto *af = registry.try_get<AreaFieldComponent>(entity)) {
    af->remaining_duration = field.header.duration;
  }
  field.header.tick_timer -= dt;
  field.linked_pulse_cooldown = std::max(0.0f, field.linked_pulse_cooldown - dt);
  field.return_empower_timer = std::max(0.0f, field.return_empower_timer - dt);
  // 1217 绝影共噬窗口倒计时：2 秒后自动归零，增益随之失效。
  field.shared_devour_timer = std::max(0.0f, field.shared_devour_timer - dt);
  if (field.header.duration <= 0.0f) {
    ClearBloodSeaActiveBuff(registry, field.header.owner);
    registry.destroy(entity);
    return;
  }

  SyncBloodSeaActiveBuff(registry, field.header.owner, field);

  field_pos->x = Lerp(field_pos->x, owner_pos->x,
                      std::clamp(dt * field.move_follow_speed, 0.0f, 1.0f));
  field_pos->y = Lerp(field_pos->y, owner_pos->y,
                      std::clamp(dt * field.move_follow_speed, 0.0f, 1.0f));

  if (field.header.tick_timer > 0.0f) {
    return;
  }
  field.header.tick_timer = field.header.tick_interval;

  std::vector<entt::entity> targets;
  grid.query(*field_pos, field.header.radius, [&](entt::entity target, const Position &) {
    if (target == field.header.owner || target == entity || registry.any_of<KilledTag>(target) ||
        !registry.any_of<EnemyTag>(target)) {
      return;
    }
    targets.push_back(target);
  });
  if (targets.empty()) {
    targets = CollectBloodSeaTargets(registry, *field_pos, field.header.radius,
                                     field.header.owner, entity);
  }
  if (targets.empty()) {
    return;
  }

  ++field.pulses_triggered;
  float base_damage =
      GetMech(kBloodSeaSkillId, 0u, "pulse_base_damage", 14.0f) +
      GetMech(kBloodSeaSkillId, 0u, "pulse_damage_per_bloodthirst", 4.0f) *
          static_cast<float>(field.consumed_bloodthirst);
  if (field.has_void_keystone) {
    base_damage *= GetMech(kBloodSeaSkillId, BloodSeaNodes::VoidErosionMiasma,
                           "pulse_damage_mult", 1.08f);
    base_damage *= 1.0f + field.void_damage_bonus_mult;
  }
  if (field.ring_form) {
    base_damage *= GetMech(kBloodSeaSkillId, BloodSeaNodes::BloodRingDevour,
                           "pulse_damage_mult", 1.1f);
  }
  (void)DealPulse(registry, entity, field, targets, base_damage);
  SyncBloodSeaActiveBuff(registry, field.header.owner, field);
}

void BloodSea::HandleLinkedHit(entt::registry &registry, const CombatEvent &evt) {
  if (!IsBloodSeaLinkableSkill(evt.skill_id) || !registry.valid(evt.source) ||
      !registry.valid(evt.target)) {
    return;
  }

  const auto *target_pos = registry.try_get<Position>(evt.target);
  if (target_pos == nullptr) {
    return;
  }

  auto view = registry.view<BloodSeaFieldComponent, Position>();
  for (const entt::entity field_entity : view) {
    auto &field = view.get<BloodSeaFieldComponent>(field_entity);
    const auto &field_pos = view.get<Position>(field_entity);
    if (field.header.owner != evt.source ||
        !IsInsideBloodSeaField(*target_pos, field_pos, field.header.radius)) {
      continue;
    }

    ++field.header.linked_hit_count;
    // 门控来源：has_linked_synergy 现由节点 1207 无间血狱置位（设计 §5.3:1236），
    // 与 1217 绝影共噬的窗口增伤完全解耦；冷却仍由节点数据键驱动。
    if (!field.header.has_linked_synergy || field.linked_pulse_cooldown > 0.0f) {
      continue;
    }

    field.linked_pulse_cooldown =
        field.torrent_form
            ? GetMech(kBloodSeaSkillId, BloodSeaNodes::CrimsonTorrent,
                      "linked_pulse_cooldown", 0.12f)
            : GetMech(kBloodSeaSkillId, 0u, "linked_pulse_cooldown", 0.2f);
    std::vector<entt::entity> targets = {evt.target};
    ++field.pulses_triggered;
    float linked_damage =
        GetMech(kBloodSeaSkillId, 0u, "linked_pulse_base_damage", 12.0f) +
        static_cast<float>(field.consumed_bloodthirst) *
            GetMech(kBloodSeaSkillId, 0u, "linked_pulse_damage_per_bloodthirst",
                    2.0f);
    linked_damage *= 1.0f + field.linked_pressure_bonus_mult;
    if (field.return_empower_timer > 0.0f) {
      linked_damage *= 1.0f + field.return_empower_bonus_mult;
    }
    (void)DealPulse(registry, field_entity, field, targets,
                    linked_damage);
  }
}

REGISTER_SKILL_BEHAVIOR(BloodSea)

void RegisterBloodSea() {}

} // namespace NoMoreDay::skills
