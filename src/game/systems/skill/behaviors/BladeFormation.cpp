/**
 * @file BladeFormation.cpp
 * @brief 灵剑决 (ID 3) - 模块化环绕守护与灵剑召唤实现
 */
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/data/BuffIds.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>

namespace NoMoreDay::skills {

namespace BladeFormationNodes {
constexpr uint32_t SwordPool = 300;
constexpr uint32_t SwiftIntent = 301;
constexpr uint32_t EdgedSpirit = 302;
constexpr uint32_t ElementalCore = 303;
constexpr uint32_t SearchRadius = 310;
constexpr uint32_t InfiniteSheath = 311;
constexpr uint32_t Network = 312;
constexpr uint32_t Godspeed = 313;
constexpr uint32_t Concentrate = 314;
constexpr uint32_t SwordStepResonance = 315;
constexpr uint32_t GiantSword = 330;
constexpr uint32_t WeakPointCrit = 331;
constexpr uint32_t DeadlyEdge = 332;
constexpr uint32_t SwordPressure = 333;
constexpr uint32_t Crush = 334;
constexpr uint32_t ColossusRend = 335;
constexpr uint32_t Ward = 350;
constexpr uint32_t BladeOrbit = 351;
constexpr uint32_t RetaliationWeb = 352;
constexpr uint32_t Immortality = 353;
constexpr uint32_t SpellEcho = 354;
constexpr uint32_t ArrayResonance = 355;
constexpr uint32_t ElementFire = 370;
constexpr uint32_t BlazingDance = 371;
constexpr uint32_t ElementLightning = 372;
constexpr uint32_t ArcChain = 373;
constexpr uint32_t SpiritCorrosion = 374;
constexpr uint32_t Charge = 375;
} // namespace BladeFormationNodes

struct BladeFormation : SkillBehaviorBase<BladeFormation> {
  static constexpr uint32_t kSkillId = 3;

  static void DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
    auto &formation = registry.get_or_emplace<BladeFormationComponent>(owner);
    const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId);
    BakedSkillProfile localProfile;
    if (!profile && registry.all_of<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
        if (spec.skill_id == kSkillId) {
          SkillSpecializationBaker::Bake(registry, owner, kSkillId, &spec, localProfile, nullptr);
          profile = &localProfile;
          break;
        }
      }
    }

    // 提取分配点数
    int pts300 = 0, pts301 = 0, pts302 = 0, pts303 = 0, pts310 = 0, pts312 = 0;
    int pts314 = 0, pts315 = 0, pts331 = 0, pts332 = 0, pts333 = 0, pts334 = 0;
    int pts350 = 0, pts352 = 0, pts371 = 0, pts373 = 0, pts374 = 0, pts375 = 0;
    if (registry.all_of<ActiveSkillsComponent>(owner)) {
      for (const auto &s : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
        if (s.skill_id == kSkillId) {
          if (auto it = s.allocated_points.find(BladeFormationNodes::SwordPool); it != s.allocated_points.end()) pts300 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::SwiftIntent); it != s.allocated_points.end()) pts301 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::EdgedSpirit); it != s.allocated_points.end()) pts302 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::ElementalCore); it != s.allocated_points.end()) pts303 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::SearchRadius); it != s.allocated_points.end()) pts310 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::Network); it != s.allocated_points.end()) pts312 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::Concentrate); it != s.allocated_points.end()) pts314 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::SwordStepResonance); it != s.allocated_points.end()) pts315 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::WeakPointCrit); it != s.allocated_points.end()) pts331 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::DeadlyEdge); it != s.allocated_points.end()) pts332 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::SwordPressure); it != s.allocated_points.end()) pts333 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::Crush); it != s.allocated_points.end()) pts334 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::Ward); it != s.allocated_points.end()) pts350 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::RetaliationWeb); it != s.allocated_points.end()) pts352 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::BlazingDance); it != s.allocated_points.end()) pts371 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::ArcChain); it != s.allocated_points.end()) pts373 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::SpiritCorrosion); it != s.allocated_points.end()) pts374 = it->second;
          if (auto it = s.allocated_points.find(BladeFormationNodes::Charge); it != s.allocated_points.end()) pts375 = it->second;
          break;
        }
      }
    }

    const auto &mech = data::SkillMechanicsRegistry::Get();

    // 标志位与天赋状态装配
    formation.has_giant_sword = profile ? ((profile->delivery.feature_flags & 2) != 0) : exec.active_nodes.test(BladeFormationNodes::GiantSword % 100);
    formation.melee_orbit = profile ? ((profile->delivery.feature_flags & 4) != 0) : exec.active_nodes.test(BladeFormationNodes::BladeOrbit % 100);
    formation.has_immortality = profile ? ((profile->delivery.feature_flags & 8) != 0) : exec.active_nodes.test(BladeFormationNodes::Immortality % 100);
    if (formation.has_immortality && formation.immortality_cooldown <= 0.0f) {
      formation.immortality_ready = true;
    }
    formation.has_godspeed = profile ? ((profile->delivery.feature_flags & 16) != 0) : exec.active_nodes.test(BladeFormationNodes::Godspeed % 100);
    formation.has_concentrate = profile ? ((profile->delivery.feature_flags & 32) != 0) : (pts314 > 0 || exec.active_nodes.test(BladeFormationNodes::Concentrate % 100));
    formation.has_spell_echo = profile ? ((profile->delivery.feature_flags & 32768) != 0) : exec.active_nodes.test(BladeFormationNodes::SpellEcho % 100);

    formation.pts303 = pts303;
    formation.pts333 = pts333;
    formation.pts334 = pts334;
    formation.has_array_resonance = profile ? ((profile->delivery.feature_flags & 65536) != 0) : exec.active_nodes.test(BladeFormationNodes::ArrayResonance % 100);
    formation.has_fire = (profile && HasTag(profile->effective_tags, Tag::Fire)) || exec.active_nodes.test(BladeFormationNodes::ElementFire % 100);
    formation.pts371 = pts371;
    formation.has_lightning = (profile && HasTag(profile->effective_tags, Tag::Lightning)) || exec.active_nodes.test(BladeFormationNodes::ElementLightning % 100);
    formation.pts373 = pts373;
    formation.pts374 = pts374;
    formation.pts375 = pts375;

    // 索敌半径 (Node 310)：统一 200 基准，与无 profile 路径一致 (Baker 同公式 200*(1+0.2p))
    formation.search_radius = (profile && profile->delivery.range > 0.0f)
        ? profile->delivery.range
        : (200.0f * (1.0f + 0.20f * static_cast<float>(pts310)));

    // 灵力网络 (Node 312)
    formation.mana_regen_per_sword = (pts312 > 0)
        ? mech.GetFloat(kSkillId, BladeFormationNodes::Network, "mana_regen_per_sword_per_point", 3.0f) * static_cast<float>(pts312)
        : ((profile && (profile->delivery.feature_flags & 512) != 0) ? 3.0f : 0.0f);

    // 御剑共振 (Node 315)
    formation.sword_step_haste = (pts315 > 0)
        ? (mech.GetFloat(kSkillId, BladeFormationNodes::SwordStepResonance, "resonance_freq_pct_per_point", 20.0f) / 100.0f) * static_cast<float>(pts315)
        : ((profile && (profile->delivery.feature_flags & 64) != 0) ? 0.20f : 0.0f);
    formation.sword_step_intent_chance = (pts315 > 0)
        ? (mech.GetFloat(kSkillId, BladeFormationNodes::SwordStepResonance, "intent_gain_chance_pct_per_point", 10.0f) / 100.0f) * static_cast<float>(pts315)
        : ((profile && (profile->delivery.feature_flags & 64) != 0) ? 0.10f : 0.0f);

    // 灵剑护体减伤比例 (Node 350)
    formation.ward_dr_per_sword = (pts350 > 0)
        ? (mech.GetFloat(kSkillId, BladeFormationNodes::Ward, "ward_dr_pct_per_sword_per_point", 1.0f) / 100.0f) * static_cast<float>(pts350)
        : ((profile && (profile->delivery.feature_flags & 8192) != 0) ? 0.01f : 0.0f);

    // 反击剑网格挡率 (Node 352)
    formation.block_chance_per_sword = (pts352 > 0)
        ? (mech.GetFloat(kSkillId, BladeFormationNodes::RetaliationWeb, "block_chance_per_sword_per_point", 2.0f) / 100.0f) * static_cast<float>(pts352)
        : ((profile && (profile->delivery.feature_flags & 16384) != 0) ? 0.02f : 0.0f);

    // 灵剑数量上限（巨剑强制为 1；无尽剑匣翻倍）
    if (formation.has_giant_sword) {
      formation.max_swords = 1;
    } else if (profile) {
      formation.max_swords = (profile->projectile_count > 0) ? profile->projectile_count : 3;
    } else {
      int count = 3 + pts300;
      if (exec.active_nodes.test(BladeFormationNodes::InfiniteSheath % 100)) {
        count *= 2;
      }
      formation.max_swords = count;
    }
    formation.current_swords = formation.max_swords;

    // 伤害与单发倍率 (Node 330 巨剑: 1.25f 即基底 0.5f 的 2.5 倍 / +150% 伤害; Node 311: 0.6f 衰减; Node 302: 锋灵 moreMult)
    const float baseDamageScale = formation.has_giant_sword ? 1.25f : 0.5f;
    const bool hasInfiniteSheath = (profile && (profile->delivery.feature_flags & 1) != 0) ||
                                   (!profile && exec.active_nodes.test(BladeFormationNodes::InfiniteSheath % 100));
    formation.damage_penalty = hasInfiniteSheath ? 0.6f : 1.0f;
    const float moreMult = profile ? profile->more_damage_mult : (1.0f + 0.10f * static_cast<float>(pts302));
    const float finalDamageScale = baseDamageScale * formation.damage_penalty * moreMult;

    // DoHit 热路径预烘焙缓存：DoCast 一次性读取 mechanics，命中时零 GetFloat、零 std::string 构造
    formation.final_damage_scale = finalDamageScale;
    formation.taken_phys_pct = (pts333 > 0)
        ? mech.GetFloat(kSkillId, BladeFormationNodes::SwordPressure, "taken_phys_pct_per_point", 5.0f) * static_cast<float>(pts333)
        : 0.0f;
    formation.crush_stun_chance = (pts334 >= 3) ? 1.0f : (0.3333f * static_cast<float>(pts334));
    formation.crush_stun_duration = mech.GetFloat(kSkillId, BladeFormationNodes::Crush, "stun_duration", 0.5f);
    formation.ignite_base_duration = mech.GetFloat(kSkillId, BladeFormationNodes::ElementFire, "ignite_duration", 3.0f);
    formation.ignite_duration_mult = 1.0f + (mech.GetFloat(kSkillId, BladeFormationNodes::BlazingDance, "ignite_duration_pct_per_point", 20.0f) / 100.0f) * static_cast<float>(pts371);
    // 每剑点燃持续伤害加成：由 371 启用，固定每剑 10.0（与历史语义一致，不随点数缩放）
    formation.ignite_dot_per_sword = (pts371 > 0)
        ? mech.GetFloat(kSkillId, BladeFormationNodes::BlazingDance, "dot_per_sword", 10.0f)
        : 0.0f;
    formation.ignite_base_magnitude = 15.0f;
    formation.chain_damage_pct = (mech.GetFloat(kSkillId, BladeFormationNodes::ArcChain, "chain_damage_pct_per_point", 15.0f) / 100.0f) * static_cast<float>(pts373);
    formation.chain_radius = mech.GetFloat(kSkillId, BladeFormationNodes::ArcChain, "chain_radius", 200.0f);
    formation.shred_per_stack = (pts374 > 0)
        ? mech.GetFloat(kSkillId, BladeFormationNodes::SpiritCorrosion, "shred_per_stack_per_point", 2.0f) * static_cast<float>(pts374)
        : 0.0f;
    formation.shred_max_stacks = mech.GetFloat(kSkillId, BladeFormationNodes::SpiritCorrosion, "max_stacks", 8.0f);
    formation.shred_duration = mech.GetFloat(kSkillId, BladeFormationNodes::SpiritCorrosion, "duration", 4.0f);
    formation.burst_mult = (pts375 > 0) ? mech.GetFloat(kSkillId, BladeFormationNodes::Charge, "elemental_damage_mult", 2.0f) : 0.0f;

    // 攻击频率计算（疾风意 301 提升频率；巨剑 330 频率减半；神速 313 受攻速加成）
    const float freqInc = profile ? profile->delivery.sub_interval : (0.08f * static_cast<float>(pts301));
    float attackInterval = (1.0f / (1.0f + freqInc)) * (formation.has_giant_sword ? 2.0f : 1.0f);
    if (formation.has_godspeed) {
      if (const auto *stats = registry.try_get<CombatStats>(owner)) {
        const float asBonus = (std::max)(0.0f, (stats->attack_speed - 100.0f) * 0.75f / 100.0f);
        attackInterval /= (1.0f + asBonus);
      }
    }
    formation.attack_interval = attackInterval;

    // 环绕哨兵组件装配
    auto &sentinel = registry.emplace_or_replace<OrbitingSentinelComponent>(owner);
    sentinel.anchor_entity = owner;
    sentinel.skill_id = kSkillId;
    sentinel.count = formation.max_swords;
    sentinel.angular_velocity = formation.melee_orbit ? 360.0f : 180.0f;
    sentinel.orbit_radius = formation.melee_orbit ? 50.0f : 60.0f;
    sentinel.attack_interval = formation.attack_interval;

    // 元素附魔与转质确定
    Tag element = Tag::None;
    float convRatio = 0.5f;
    if (formation.has_fire) {
      element = Tag::Fire;
      convRatio = 1.0f;
    } else if (formation.has_lightning) {
      element = Tag::Lightning;
      convRatio = 1.0f;
    } else {
      element = systems::BladeResourceService::GetHeavenlyAttunementElementTag(registry, owner);
      const float convBonus = (pts303 > 0)
          ? (mech.GetFloat(kSkillId, BladeFormationNodes::ElementalCore, "conv_efficiency_pct_per_point", 10.0f) / 100.0f) * static_cast<float>(pts303)
          : ((profile && (profile->delivery.feature_flags & 256) != 0) ? 0.10f : 0.0f);
      convRatio = std::min(1.0f, 0.5f + convBonus);
    }

    // 灵剑实体生命周期与组件装配
    std::vector<entt::entity> existing;
    auto view = registry.view<SpiritSwordTag, SummonComponent>();
    for (auto e : view) {
      if (view.get<SummonComponent>(e).owner == owner) {
        existing.push_back(e);
      }
    }

    const float bonusCrit = profile ? profile->delivery.bonus_crit : (0.05f * static_cast<float>(pts331));
    const float bonusCritDamage = profile ? profile->delivery.bonus_crit_damage : (0.25f * static_cast<float>(pts332));

    for (auto e : existing) {
      auto &cb = registry.get_or_emplace<SummonCombatProfile>(e);
      cb.damage_scale = finalDamageScale;
      cb.bonus_crit = bonusCrit;
      cb.bonus_crit_damage = bonusCritDamage;
      auto &ai = registry.get_or_emplace<SpiritSwordAI>(e);
      ai.attack_interval = formation.attack_interval;
      ai.state = formation.melee_orbit ? SpiritSwordAI::State::MeleeOrbit : SpiritSwordAI::State::Idle;

      auto &aiProf = registry.get_or_emplace<SummonAIProfile>(e);
      aiProf.role = formation.melee_orbit ? SummonRole::Melee : SummonRole::Orbit;
      aiProf.command_mode = formation.mode == SpiritSwordMode::Elite ? SummonCommandMode::Aggressive : SummonCommandMode::Assist;
      aiProf.leash_radius = formation.search_radius;

      auto &rt = registry.get_or_emplace<SummonRuntimeState>(e);
      if (rt.proc_budget < cb.proc_budget_cap) {
        rt.proc_budget = cb.proc_budget_cap;
      }

      if (element != Tag::None) {
        auto &m = registry.emplace_or_replace<SkillModifierComponent>(e);
        m.damage_modifiers.clear();
        m.damage_modifiers.push_back({Tag::Physical, element, convRatio, ModifierType::Convert});
      }
    }

    const int cur = static_cast<int>(existing.size());
    if (cur > formation.max_swords) {
      for (int i = formation.max_swords; i < cur; ++i) {
        registry.destroy(existing[i]);
      }
    } else if (cur < formation.max_swords) {
      auto *pos = registry.try_get<Position>(owner);
      for (int i = cur; i < formation.max_swords; ++i) {
        auto sword = registry.create();
        registry.emplace<LocalLevelTag>(sword);
        registry.emplace<SpiritSwordTag>(sword);
        registry.emplace<Position>(sword, pos ? pos->x : 0.0f, pos ? pos->y : 0.0f);
        registry.emplace<Velocity>(sword, 0.0f, 0.0f);

        auto &sm = registry.emplace<SummonComponent>(sword);
        sm.owner = owner;
        sm.skill_id = kSkillId;
        sm.archetype_id = SummonArchetype::SpiritSword;
        sm.lifetime = sm.max_lifetime = -1.0f; // 永久维持

        auto &aiProf = registry.emplace<SummonAIProfile>(sword);
        aiProf.role = formation.melee_orbit ? SummonRole::Melee : SummonRole::Orbit;
        aiProf.command_mode = formation.mode == SpiritSwordMode::Elite ? SummonCommandMode::Aggressive : SummonCommandMode::Assist;
        aiProf.leash_radius = formation.search_radius;
        aiProf.retarget_interval = 0.2f;

        auto &rt = registry.emplace<SummonRuntimeState>(sword);
        rt.proc_budget = 6.0f;

        auto &ai = registry.emplace<SpiritSwordAI>(sword);
        ai.orbit_angle = (360.0f / static_cast<float>(formation.max_swords)) * static_cast<float>(i);
        ai.attack_interval = formation.attack_interval;
        ai.state = formation.melee_orbit ? SpiritSwordAI::State::MeleeOrbit : SpiritSwordAI::State::Idle;

        auto &cb = registry.emplace<SummonCombatProfile>(sword);
        cb.damage_scale = finalDamageScale;
        cb.bonus_crit = bonusCrit;
        cb.bonus_crit_damage = bonusCritDamage;
        cb.melee_orbit_hit_radius = 30.0f;
        cb.melee_orbit_base_damage = 25.0f;

        if (element != Tag::None) {
          auto &m = registry.emplace_or_replace<SkillModifierComponent>(sword);
          m.damage_modifiers.clear();
          m.damage_modifiers.push_back({Tag::Physical, element, convRatio, ModifierType::Convert});
        }
      }
    }
  }

  static void DoHit(entt::registry &reg, entt::entity attacker, entt::entity victim, Tag, bool) {
    if (!reg.valid(attacker)) return;
    auto *formation = reg.try_get<BladeFormationComponent>(attacker);
    if (!formation) return;

    // 命中回蓝 (Legacy Talent 321 / 兼容验证)
    if (formation->mana_on_hit) {
      if (auto *stats = reg.try_get<CombatStats>(attacker)) {
        stats->mana = std::min(stats->max_mana, stats->mana + 2.0f);
        reg.get_or_emplace<StatsDirty>(attacker);
      }
    }

    // 御剑共振 (Node 315)：御剑步期间灵剑命中有 10%...30% 几率回复 1 层剑意
    if (formation->sword_step_intent_chance > 0.0f) {
      if (auto *effects = reg.try_get<ActiveEffectsComponent>(attacker)) {
        if (effects->Get(BuffId::SwordStep) != nullptr) {
          const float roll = utils::ThreadSafeRandom::GetFloat01();
          if (roll <= formation->sword_step_intent_chance) {
            systems::BladeResourceService::Gain(reg, attacker, 1, kSkillId);
          }
        }
      }
    }

    if (!reg.valid(victim)) return;

    // 373/375 伤害基数：灵剑单发攻击等效伤害（武器均值 × 最终伤害缩放）
    float baseHit = 0.0f;
    if (const auto *stats = reg.try_get<CombatStats>(attacker)) {
      baseHit = ((stats->min_weapon_damage + stats->max_weapon_damage) * 0.5f) * formation->final_damage_scale;
    }

    // 剑压 (Node 333)：巨剑命中时使敌人受到的所有物理伤害增加 (Taken) 5%...15%
    if (formation->has_giant_sword && formation->taken_phys_pct > 0.0f) {
      const float takenPct = formation->taken_phys_pct;
      BuffEffect debuff{
        .id = "SwordPressure",
        .name = "Sword Pressure",
        .type = BuffType::DefenseDown,
        .duration = 4.0f,
        .remaining = 4.0f,
        .is_debuff = true
      };
      debuff.modifiers.push_back({
        .value = -takenPct,
        .type = StatType::ResistPhysical,
        .mode = ModifierMode::Flat
      });
      reg.get_or_emplace<ActiveEffectsComponent>(victim).AddOrRefresh(debuff);
      reg.get_or_emplace<StatsDirty>(victim);
    }

    // 碎岩 (Node 334)：巨剑命中时有 33%...100% 几率击晕普通敌人 0.5s，并施加 1 层护甲击碎
    if (formation->has_giant_sword && formation->crush_stun_chance > 0.0f) {
      const float stunChance = formation->crush_stun_chance;
      if (utils::ThreadSafeRandom::GetFloat01() <= stunChance) {
        systems::AilmentApplyRequest stunReq{
          .ailment = AilmentType::Stun,
          .source = attacker,
          .duration = formation->crush_stun_duration
        };
        (void)systems::AilmentApplier::Apply(reg, victim, stunReq);
      }
      BuffEffect shred{
        .id = "ArmorShred",
        .name = "Armor Shred",
        .type = BuffType::DefenseDown,
        .duration = 4.0f,
        .remaining = 4.0f,
        .stacks = 1,
        .is_debuff = true
      };
      shred.modifiers.push_back({
        .value = -10.0f,
        .type = StatType::Armor,
        .mode = ModifierMode::Flat
      });
      reg.get_or_emplace<ActiveEffectsComponent>(victim).AddOrRefresh(shred);
      reg.get_or_emplace<StatsDirty>(victim);
    }

    // 地火明夷 (Node 370) & 灼魂剑舞 (Node 371)：命中必点燃，点燃持续与伤害按活跃灵剑缩放
    if (formation->has_fire) {
      const float durMult = formation->ignite_duration_mult;
      const float baseDur = formation->ignite_base_duration;
      const float dotExtra = (formation->ignite_dot_per_sword > 0.0f)
          ? formation->ignite_dot_per_sword * static_cast<float>(formation->current_swords)
          : 0.0f;
      systems::AilmentApplyRequest fireReq{
        .ailment = AilmentType::Ignite,
        .source = attacker,
        .magnitude = formation->ignite_base_magnitude + dotExtra,
        .duration = baseDur * durMult,
        .stacks = 1
      };
      (void)systems::AilmentApplier::Apply(reg, victim, fireReq);
    }

    // 紫电紫雷 (Node 372) & 雷弧连锁 (Node 373)
    if (formation->has_lightning) {
      systems::AilmentApplyRequest shockReq{
        .ailment = AilmentType::Shock,
        .source = attacker,
        .magnitude = 1.0f,
        .duration = 4.0f,
        .stacks = 1
      };
      (void)systems::AilmentApplier::Apply(reg, victim, shockReq);

      // 雷弧连锁 (Node 373)：若敌人已感电，产生闪电弧向 1...3 名附近敌人连锁
      if (formation->pts373 > 0 && reg.all_of<Position>(victim)) {
        bool victimShocked = false;
        if (const auto *fx = reg.try_get<ActiveEffectsComponent>(victim)) {
          for (const auto &b : fx->effects) {
            if (b.remaining > 0.0f && (b.type == BuffType::Shock || b.id.find("shock") != std::string::npos)) {
              victimShocked = true;
              break;
            }
          }
        }
        if (victimShocked) {
          const auto &vPos = reg.get<Position>(victim);
          int chainsLeft = std::min(3, formation->pts373);
          const float chainRadiusSqr = formation->chain_radius * formation->chain_radius;
          auto enemyView = reg.view<EnemyTag, Position>();
          for (auto eEnt : enemyView) {
            // DoHit 回调无 grid 上下文，O(N)+早退为当前架构约束，完整网格化待回调签名扩展
            if (chainsLeft <= 0) break;
            if (eEnt == victim || reg.any_of<KilledTag>(eEnt)) continue;
            const auto &ePos = enemyView.get<Position>(eEnt);
            if (Vector2DistanceSqr({vPos.x, vPos.y}, {ePos.x, ePos.y}) <= chainRadiusSqr) {
              DamagePool pool;
              pool.Add(Tag::Lightning, baseHit * formation->chain_damage_pct);
              DamageRequest req;
              req.origin = DamageOrigin::SecondaryProc;
              req.attacker = attacker;
              req.defender = eEnt;
              req.skill_id = kSkillId;
              req.base_pool = pool;
              req.additional_tags = Tag::Lightning | Tag::SecondaryHit;
              (void)ResolveDamage(reg, req, attacker);
              chainsLeft--;
            }
          }
        }
      }
    }

    // 灵剑蚀甲 (Node 374)：攻击带有对应元素异常（点燃/感电）的敌人，降低对应元素抗性 2...8 点（最多 8 层），持续 4 秒
    if (formation->shred_per_stack > 0.0f) {
      bool victimHasAilment = false;
      Tag shredElement = Tag::None;
      if (const auto *fx = reg.try_get<ActiveEffectsComponent>(victim)) {
        for (const auto &b : fx->effects) {
          if (b.remaining > 0.0f) {
            if (formation->has_fire && (b.type == BuffType::Burn || b.id.find("ignite") != std::string::npos)) {
              victimHasAilment = true;
              shredElement = Tag::Fire;
              break;
            }
            if (formation->has_lightning && (b.type == BuffType::Shock || b.id.find("shock") != std::string::npos)) {
              victimHasAilment = true;
              shredElement = Tag::Lightning;
              break;
            }
          }
        }
      }
      if (victimHasAilment && shredElement != Tag::None) {
        const float shredPerStack = formation->shred_per_stack;
        const float maxStacks = formation->shred_max_stacks;
        const float dur = formation->shred_duration;
        std::string debuffId = (shredElement == Tag::Fire) ? "SpiritCorrosion_Fire" : "SpiritCorrosion_Lightning";
        auto &fx = reg.get_or_emplace<ActiveEffectsComponent>(victim);
        BuffEffect *existing = fx.Get(debuffId);
        if (existing) {
          existing->duration = dur;
          existing->remaining = dur;
          if (existing->stacks < static_cast<int>(maxStacks)) {
            existing->stacks++;
          }
          existing->max_stacks = static_cast<int>(maxStacks);
          existing->modifiers.clear();
          existing->modifiers.push_back({
            .value = -shredPerStack * static_cast<float>(existing->stacks),
            .type = (shredElement == Tag::Fire) ? StatType::ResistFire : StatType::ResistLightning,
            .mode = ModifierMode::Flat
          });
        } else {
          BuffEffect debuff{
            .id = debuffId,
            .name = "Spirit Corrosion",
            .type = BuffType::DefenseDown,
            .duration = dur,
            .remaining = dur,
            .stacks = 1,
            .max_stacks = static_cast<int>(maxStacks),
            .is_debuff = true
          };
          debuff.modifiers.push_back({
            .value = -shredPerStack,
            .type = (shredElement == Tag::Fire) ? StatType::ResistFire : StatType::ResistLightning,
            .mode = ModifierMode::Flat
          });
          fx.AddOrRefresh(debuff);
        }
        reg.get_or_emplace<StatsDirty>(victim);
      }
    }

    // 灵剑充能 (Node 375)：每攻击 4/3/2 次，下一次造成双倍元素伤害并引爆该元素异常
    if (formation->pts375 > 0 && (formation->has_fire || formation->has_lightning)) {
      const int hitsRequired = std::max(2, 5 - formation->pts375);
      formation->charge_attack_counter++;
      if (formation->charge_attack_counter >= hitsRequired) {
        formation->charge_attack_counter = 0;
        Tag elemTag = formation->has_fire ? Tag::Fire : Tag::Lightning;
        DamagePool pool;
        pool.Add(elemTag, baseHit * formation->burst_mult);
        DamageRequest detReq;
        detReq.origin = DamageOrigin::SecondaryProc;
        detReq.attacker = attacker;
        detReq.defender = victim;
        detReq.skill_id = kSkillId;
        detReq.base_pool = pool;
        detReq.additional_tags = elemTag | Tag::SecondaryHit;
        (void)ResolveDamage(reg, detReq, attacker);
      }
    }
  }
};

REGISTER_SKILL_BEHAVIOR(BladeFormation)
void RegisterBladeFormation() {}
} // namespace NoMoreDay::skills
