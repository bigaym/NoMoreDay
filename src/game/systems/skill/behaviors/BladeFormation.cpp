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
#include "game/foundation/SharedContext.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "raymath.h"
#include <algorithm>
#include <array>
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

  // 节点点亮的判定：生产路径必定已 Bake（RebakeSkillProfiles 预热，或 DoCast
  // 就地烘焙），故直接读烘焙产物的 feature_flags（生产单一来源）。
  // profile 为空时回退 SkillExecution.active_nodes，仅用于兼容绕过 TryCast 的
  // 直接行为调用（行为层/集成测试依赖此路径，见 SkillSystemTests
  // "Blade Ascendant key branches run"），生产不可达。
  [[nodiscard]] static bool HasNodeFlag(const BakedSkillProfile *profile,
                                        uint32_t flag, const SkillExecution &exec,
                                        uint32_t node_id) {
    return profile != nullptr ? (profile->delivery.feature_flags & flag) != 0
                              : exec.active_nodes.test(node_id % 100);
  }

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec) {
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
    int pts315 = 0, pts331 = 0, pts332 = 0, pts333 = 0, pts334 = 0;
    int pts350 = 0, pts352 = 0, pts371 = 0, pts373 = 0, pts374 = 0, pts375 = 0;
    if (registry.all_of<ActiveSkillsComponent>(owner)) {
      for (const auto &s : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
        if (s.skill_id == kSkillId) {
          pts300 = ReadPoints(s, BladeFormationNodes::SwordPool);
          pts301 = ReadPoints(s, BladeFormationNodes::SwiftIntent);
          pts302 = ReadPoints(s, BladeFormationNodes::EdgedSpirit);
          pts303 = ReadPoints(s, BladeFormationNodes::ElementalCore);
          pts310 = ReadPoints(s, BladeFormationNodes::SearchRadius);
          pts312 = ReadPoints(s, BladeFormationNodes::Network);
          pts315 = ReadPoints(s, BladeFormationNodes::SwordStepResonance);
          pts331 = ReadPoints(s, BladeFormationNodes::WeakPointCrit);
          pts332 = ReadPoints(s, BladeFormationNodes::DeadlyEdge);
          pts333 = ReadPoints(s, BladeFormationNodes::SwordPressure);
          pts334 = ReadPoints(s, BladeFormationNodes::Crush);
          pts350 = ReadPoints(s, BladeFormationNodes::Ward);
          pts352 = ReadPoints(s, BladeFormationNodes::RetaliationWeb);
          pts371 = ReadPoints(s, BladeFormationNodes::BlazingDance);
          pts373 = ReadPoints(s, BladeFormationNodes::ArcChain);
          pts374 = ReadPoints(s, BladeFormationNodes::SpiritCorrosion);
          pts375 = ReadPoints(s, BladeFormationNodes::Charge);
          break;
        }
      }
    }

    const auto &mech = data::SkillMechanicsRegistry::Get();

    // 标志位与天赋状态装配
    formation.has_giant_sword = HasNodeFlag(profile, 2, exec, BladeFormationNodes::GiantSword);
    formation.melee_orbit = HasNodeFlag(profile, 4, exec, BladeFormationNodes::BladeOrbit);
    formation.has_immortality = HasNodeFlag(profile, 8, exec, BladeFormationNodes::Immortality);
    if (formation.has_immortality && formation.immortality_cooldown <= 0.0f) {
      formation.immortality_ready = true;
    }
    formation.has_godspeed = HasNodeFlag(profile, 16, exec, BladeFormationNodes::Godspeed);
    formation.has_concentrate = HasNodeFlag(profile, 32, exec, BladeFormationNodes::Concentrate);
    formation.has_spell_echo = HasNodeFlag(profile, 32768, exec, BladeFormationNodes::SpellEcho);

    formation.pts303 = pts303;
    formation.pts333 = pts333;
    formation.pts334 = pts334;
    formation.has_array_resonance = HasNodeFlag(profile, 65536, exec, BladeFormationNodes::ArrayResonance);
    formation.has_fire = profile != nullptr
        ? HasTag(profile->effective_tags, Tag::Fire)
        : exec.active_nodes.test(BladeFormationNodes::ElementFire % 100);
    formation.pts371 = pts371;
    formation.has_lightning = profile != nullptr
        ? HasTag(profile->effective_tags, Tag::Lightning)
        : exec.active_nodes.test(BladeFormationNodes::ElementLightning % 100);
    formation.pts373 = pts373;
    formation.pts374 = pts374;
    formation.pts375 = pts375;

    // 索敌半径 (Node 310)：统一 200 基准，与无 profile 路径一致 (Baker 同公式 200*(1+0.2p))
    formation.search_radius = (profile && profile->delivery.range > 0.0f)
        ? profile->delivery.range
        : (200.0f * (1.0f + 0.20f * static_cast<float>(pts310)));

    // 灵力网络 (Node 312)
    formation.mana_regen_per_sword = HasNodeFlag(profile, 512, exec, BladeFormationNodes::Network)
        ? mech.GetFloat(kSkillId, BladeFormationNodes::Network, "mana_regen_per_sword_per_point", 3.0f) * static_cast<float>(pts312)
        : 0.0f;

    // 御剑共振 (Node 315)
    const bool hasSwordStepResonance =
        HasNodeFlag(profile, 64, exec, BladeFormationNodes::SwordStepResonance);
    formation.sword_step_haste = hasSwordStepResonance
        ? (mech.GetFloat(kSkillId, BladeFormationNodes::SwordStepResonance, "resonance_freq_pct_per_point", 20.0f) / 100.0f) * static_cast<float>(pts315)
        : 0.0f;
    formation.sword_step_intent_chance = hasSwordStepResonance
        ? (mech.GetFloat(kSkillId, BladeFormationNodes::SwordStepResonance, "intent_gain_chance_pct_per_point", 10.0f) / 100.0f) * static_cast<float>(pts315)
        : 0.0f;

    // 灵剑护体减伤比例 (Node 350)
    formation.ward_dr_per_sword = HasNodeFlag(profile, 8192, exec, BladeFormationNodes::Ward)
        ? (mech.GetFloat(kSkillId, BladeFormationNodes::Ward, "ward_dr_pct_per_sword_per_point", 1.0f) / 100.0f) * static_cast<float>(pts350)
        : 0.0f;

    // 反击剑网格挡率 (Node 352)
    formation.block_chance_per_sword = HasNodeFlag(profile, 16384, exec, BladeFormationNodes::RetaliationWeb)
        ? (mech.GetFloat(kSkillId, BladeFormationNodes::RetaliationWeb, "block_chance_per_sword_per_point", 2.0f) / 100.0f) * static_cast<float>(pts352)
        : 0.0f;

    // 灵剑数量上限（巨剑强制为 1；无尽剑匣翻倍）
    if (formation.has_giant_sword) {
      formation.max_swords = 1;
    } else if (profile) {
      formation.max_swords = (profile->projectile_count > 0) ? profile->projectile_count : 3;
    } else {
      // 未 Bake 兼容回退：无专精槽时 pts300 必为 0，无尽剑匣位仅可经 active_nodes 置位
      int count = 3 + pts300;
      if (exec.active_nodes.test(BladeFormationNodes::InfiniteSheath % 100)) {
        count *= 2;
      }
      formation.max_swords = count;
    }
    formation.current_swords = formation.max_swords;

    // 伤害与单发倍率 (Node 330 巨剑: 1.25f 即基底 0.5f 的 2.5 倍 / +150% 伤害; Node 311: 0.6f 衰减; Node 302: 锋灵 moreMult)
    const float baseDamageScale = formation.has_giant_sword ? 1.25f : 0.5f;
    const bool hasInfiniteSheath =
        HasNodeFlag(profile, 1, exec, BladeFormationNodes::InfiniteSheath);
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

  // 373 雷弧连锁候选枚举：优先复用 gameplay 注入的空间网格 (SharedContext::spatialGrid)
  // 缩小搜索范围；单测等无 SharedContext 的场景回退为旧的线性遍历，保证命中集合一致。
  // 网格只做粗筛（不保证精确半径），由本 helper 基于网格帧初缓存位置做精确距离过滤
  // （帧内快速位移有固有偏差，线性回退用活体位置）。
  // onCandidate 返回 false 表示停止枚举；网格回调不支持中断，此时仅忽略其结果。
  template <typename Func>
  static void ForEachChainCandidate(entt::registry &reg, entt::entity victim,
                                    const Position &center, float radiusSqr,
                                    Func &&onCandidate) {
    const SharedContext *shared = GetSharedContext(reg);
    const systems::SpatialHashGrid *grid = shared ? shared->spatialGrid : nullptr;
    if (grid != nullptr) {
      grid->query(center, std::sqrt(radiusSqr), [&](entt::entity e, const Position &pos) {
        // 网格在帧初重建，可能残留本帧已销毁或已标记死亡的实体，需逐项复核
        if (!reg.valid(e) || e == victim) return;
        if (!reg.all_of<EnemyTag>(e) || reg.any_of<KilledTag>(e)) return;
        if (Vector2DistanceSqr({center.x, center.y}, {pos.x, pos.y}) <= radiusSqr) {
          (void)onCandidate(e);
        }
      });
      return;
    }
    auto enemyView = reg.view<EnemyTag, Position>();
    for (auto e : enemyView) {
      // 与网格路径口径一致：SpatialGrid 帧初 rebuild 已排除 DormantTag，回退路径同样跳过休眠敌人
      if (e == victim || reg.any_of<KilledTag>(e) || reg.any_of<DormantTag>(e)) continue;
      const auto &pos = enemyView.get<Position>(e);
      if (Vector2DistanceSqr({center.x, center.y}, {pos.x, pos.y}) <= radiusSqr) {
        if (!onCandidate(e)) break;
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
            // 托管感电由 AilmentEngine 写入 type=Shock (id 为大写 "Shock")，无需子串匹配
            if (b.remaining > 0.0f && b.type == BuffType::Shock) {
              victimShocked = true;
              break;
            }
          }
        }
        if (victimShocked) {
          const auto &vPos = reg.get<Position>(victim);
          // 候选数超过链上限时，实际命中子集取决于遍历序（网格路径为桶哈希序、
          // 线性回退为 EnTT view 序），如需确定性可按距离排序后截取
          int chainsLeft = std::min(3, formation->pts373);
          const float chainRadiusSqr = formation->chain_radius * formation->chain_radius;
          // 空间网格的哈希桶存在跨单元格碰撞，同一实体可能被重复返回，
          // 用小数组去重以保持旧线性遍历“每个目标至多命中一次”的语义
          std::array<entt::entity, 3> hitTargets{};
          int hitCount = 0;
          ForEachChainCandidate(reg, victim, vPos, chainRadiusSqr, [&](entt::entity eEnt) {
            if (chainsLeft <= 0) return false;
            for (int i = 0; i < hitCount; ++i) {
              if (hitTargets[static_cast<size_t>(i)] == eEnt) return chainsLeft > 0;
            }
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
            hitTargets[static_cast<size_t>(hitCount++)] = eEnt;
            chainsLeft--;
            return chainsLeft > 0;
          });
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
            if (formation->has_fire && (b.type == BuffType::Burn || b.kind == BuffKind::Ignite)) {
              victimHasAilment = true;
              shredElement = Tag::Fire;
              break;
            }
            if (formation->has_lightning && b.type == BuffType::Shock) {
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
        const BuffId debuffId = (shredElement == Tag::Fire)
            ? BuffId::SpiritCorrosionFire
            : BuffId::SpiritCorrosionLightning;
        auto &fx = reg.get_or_emplace<ActiveEffectsComponent>(victim);
        // 走零分配 string_view 重载，避免构造临时 std::string（code_standard §2.1/§7.2）
        BuffEffect *existing = fx.Get(BuffIdToString(debuffId));
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
            .id = std::string(BuffIdToString(debuffId)),
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
