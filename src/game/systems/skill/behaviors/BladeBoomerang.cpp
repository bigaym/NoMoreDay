/**
 * @file BladeBoomerang.cpp
 * @brief 御剑·回旋 (ID 8) - 模块化回旋镖交付实现
 *
 * 设计语义见 设计文档/职业设计草案_剑修.md §3.8。
 * 数值唯一事实源: 飞行速度/最远距离/半径/牵引力度取 skills.json 的 params；
 * 专精数值由 SkillSpecializationBaker case8 烘焙进 BakedDeliveryParams；
 * 行为内部调参（间隔/时长/比例）统一由 skill_mechanics.json 的 "8" 块提供。
 */
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/BuffIds.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/StunApplication.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"
#include "raylib.h"
#include "raymath.h"

namespace NoMoreDay::skills {

namespace BladeBoomerangNodes {
// 基础核心
constexpr uint32_t Lightweight = 800;
constexpr uint32_t VelocityNode = 801;
constexpr uint32_t Sharpness = 802;
constexpr uint32_t Feedback = 803;
// 分支 A: 滞空切割与流血
constexpr uint32_t Hovering = 810;
constexpr uint32_t Bleed = 811;
constexpr uint32_t CritMulti = 812;
constexpr uint32_t SonicBoom = 813;
constexpr uint32_t BloodRip = 814;
constexpr uint32_t EyeOfStorm = 815;
// 分支 B: 多重投掷与接剑
constexpr uint32_t TriBlade = 830;
constexpr uint32_t BladeDance = 831;
constexpr uint32_t Catch = 832;
constexpr uint32_t Combo = 833;
constexpr uint32_t SwordStepCatch = 834;
constexpr uint32_t ReturnDance = 835;
// 分支 C: 磁力与巨阙
constexpr uint32_t Magnet = 850;
constexpr uint32_t Area = 851;
constexpr uint32_t IntentGain = 852;
constexpr uint32_t MindIntent = 853;
constexpr uint32_t Giant = 854;
constexpr uint32_t ColossusEcho = 855;
// 分支 D: 灵根路径与异象
constexpr uint32_t AshPath = 870;
constexpr uint32_t Wildfire = 871;
constexpr uint32_t Electro = 872;
constexpr uint32_t HighVoltage = 873;
constexpr uint32_t ElementalWake = 874;
constexpr uint32_t PathPen = 875;
constexpr uint32_t ElementShield = 876;
} // namespace BladeBoomerangNodes

// feature_flags 位定义（与 SkillSpecializationBaker case8 写入值保持一致）
namespace BladeBoomerangFlags {
constexpr uint32_t Hovering = 1u << 4; // 810 滞空切割
constexpr uint32_t Magnet = 1u << 16;  // 850 磁力场（折返牵引）
constexpr uint32_t Giant = 1u << 20;   // 854 巨阙（禁用侧刃 + 命中硬直）
} // namespace BladeBoomerangFlags

// 节点常量在本文件内大量作为 GetFloat 的节点键使用，统一引入以保持可读性
using namespace BladeBoomerangNodes;

struct BladeBoomerang : SkillBehaviorBase<BladeBoomerang> {
  static constexpr uint32_t kSkillId = 8;

  // 兼容旧键名，避免其他翻译单元/工具按旧标识引用时编译失败
  static constexpr uint32_t kHoverCutNode = BladeBoomerangNodes::Hovering;

  static void DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
    auto *pos = registry.try_get<Position>(owner);
    auto *stats = registry.try_get<CombatStats>(owner);
    if (!pos || !stats) {
      return;
    }

    const auto link = seven_star_shared::ConsumeLinkBuffs(registry, owner, kSkillId, false, exec.cast_id);
    const auto &mech = data::SkillMechanicsRegistry::Get();
    const auto *sd = SkillRegistry::Get().GetSkill(kSkillId);
    const float baseRadius = sd ? sd->GetParam("radius", 40.0f) : 40.0f;
    const float basePullStrength = sd ? sd->GetParam("pull_strength", 300.0f) : 300.0f;

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
    BakedDeliveryParams defaultDelivery;
    defaultDelivery.speed = sd ? sd->GetParam("speed", 400.0f) : 400.0f;
    defaultDelivery.range = sd ? sd->GetParam("max_distance", 300.0f) : 300.0f;
    const BakedDeliveryParams &del = profile ? profile->delivery : defaultDelivery;

    const Tag effectiveTags = profile ? profile->effective_tags : Tag::Physical;
    const Tag convTag = effectiveTags & ~Tag::Physical;
    Color col = WHITE;
    if (convTag == Tag::Fire) {
      col = ORANGE;
    } else if (convTag == Tag::Lightning) {
      col = PURPLE;
    }

    // 命中盒半径: 851 重力网按比例放大判定体积
    float radius = baseRadius * del.pull_radius_mult;
    float speed = del.speed;

    // 853 心剑合一: 每层剑意按比例提升飞行/折返速度与命中盒
    if (del.intent_scaling > 0.0f) {
      if (const auto *intent = registry.try_get<SwordIntentComponent>(owner)) {
        const float intentMult =
            1.0f + del.intent_scaling * static_cast<float>(intent->stacks);
        radius *= intentMult;
        speed *= intentMult;
      }
    }

    // 802 锋锐的暴击率走技能域属性修饰符，弹体快照必须显式读取技能域暴击
    // GetStatWithTags(CritChance) 与 CombatStats/payload 统一为分数制 [0,1]
    float critChance = StatsSystem::GetStatWithTags(registry, owner, StatType::CritChance,
                                                    effectiveTags, kSkillId, owner);
    if (critChance <= 0.0f) {
      // CombatStats::crit_chance 本身即归一化小数，仅在技能域统计不可用时回退
      critChance = stats->crit_chance;
    }

    // 802 锋锐: 引擎无“附加物理点伤”的 StatType（PhysicalDamage 处于百分比乘区），
    // 其暴击率部分由 skills.json 的 stat_modifiers(CritChance Flat) 经 GetStatWithTags 生效，
    // 附加点伤部分在交付层按专精点数直接累加到武器基础伤害。
    int points802 = 0;
    if (auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
      for (const auto &s : active->specialized_slots) {
        if (s.skill_id == kSkillId) {
          points802 = ReadPoints(s, Sharpness);
          break;
        }
      }
    }

    // 854 巨阙: 附加总护甲 5% 的基础物理伤害
    float baseMin = stats->min_weapon_damage;
    float baseMax = stats->max_weapon_damage;
    if (points802 > 0) {
      const float flat =
          mech.GetFloat(kSkillId, Sharpness, "added_physical_per_point", 5.0f) *
          static_cast<float>(points802);
      baseMin += flat;
      baseMax += flat;
    }
    if (del.giant_armor_scale > 0.0f) {
      const float armorBonus = stats->armor * del.giant_armor_scale;
      baseMin += armorBonus;
      baseMax += armorBonus;
    }

    const float moreDmg = (profile ? profile->more_damage_mult : 1.0f) * link.damage_multiplier;

    Vector2 dir = Vector2Subtract(exec.target_pos, {pos->x, pos->y});
    if (Vector2LengthSqr(dir) < 1e-6f) {
      dir = {1.0f, 0.0f};
    } else {
      dir = Vector2Normalize(dir);
    }

    const bool hasMagnet = (del.feature_flags & BladeBoomerangFlags::Magnet) != 0;
    const bool isGiant = (del.feature_flags & BladeBoomerangFlags::Giant) != 0;
    const float giantSizeMult = isGiant ? mech.GetFloat(kSkillId, Giant, "giant_size_mult", 2.0f) : 1.0f;

    auto spawnProj = [&](Vector2 p_dir, bool p_isSide) {
      auto e = registry.create();
      registry.emplace<LocalLevelTag>(e);
      registry.emplace<Position>(e, pos->x, pos->y);
      registry.emplace<Velocity>(e, p_dir.x * speed, p_dir.y * speed);
      registry.emplace<ColorComponent>(e, col);

      // 830 幻影回旋: 侧翼虚影剑基础伤害 -30%
      float bladeMin = baseMin;
      float bladeMax = baseMax;
      float bladeMore = moreDmg;
      if (p_isSide) {
        const float sidePct = mech.GetFloat(kSkillId, TriBlade, "side_damage_pct", 0.70f);
        bladeMin *= sidePct;
        bladeMax *= sidePct;
      }
      if (exec.is_empowered) {
        bladeMore *= 1.5f;
      }

      auto &p = registry.emplace<Projectile>(e);
      p.owner = owner;
      p.cast_id = exec.cast_id;
      p.speed = speed;
      p.lifeTime = 3.0f;
      p.radius = radius * (isGiant ? giantSizeMult : 1.0f);
      p.pierce = true;
      p.pierceCount = 99;
      p.max_pierce = Projectile::kUnlimitedPiercing;
      p.snapshot = *stats;
      p.hasPull = hasMagnet;
      p.pullStrength = hasMagnet ? basePullStrength : 0.0f;
      p.payload_context = {.base_damage_min = bladeMin,
                           .base_damage_max = bladeMax,
                           .crit_chance = critChance,
                           .crit_multiplier = stats->crit_damage,
                           .more_damage = bladeMore,
                           .effective_tags = Tag::Physical | convTag,
                           .source_skill_id = exec.skill_id};

      registry.emplace<CombatStats>(e, p.snapshot);
      registry.emplace<SkillComponent>(e, exec.skill_id, owner);
      if (convTag != Tag::None) {
        registry.emplace<SkillModifierComponent>(e).damage_modifiers.push_back(
            {Tag::Physical, convTag, 1.0f, ModifierType::Convert});
      }

      auto &bc = registry.emplace<BoomerangComponent>(e);
      bc.owner = owner;
      bc.cast_id = exec.cast_id;
      bc.skill_id = kSkillId;
      bc.phase = BoomerangPhase::Outward;
      bc.max_distance = del.range;
      bc.hover_duration = del.duration;
      bc.returnSpeed = speed * 1.5f;
      bc.returnTimer = 0.45f;
      bc.returning_damage_mult = del.return_damage_mult;
      bc.bleed_damage_pool = 0.0f;
      bc.returning_kill_occurred = false;
      bc.path_spawn_timer = 0.0f;
      bc.element_path_tag = convTag;
      bc.path_width_mult = del.path_width_mult;
      bc.path_duration_mult = del.path_duration_mult;
      bc.path_amp = del.path_amp;
      bc.path_pen = del.path_pen;
      bc.arc_freq_mult = del.arc_freq_mult;
      bc.catch_mana = del.catch_mana;
      bc.combo_attack_speed = del.combo_attack_speed;
      bc.step_extend_sec = del.step_extend_sec;
      bc.heal_bleed_pct = del.heal_bleed_pct;
      bc.pull_radius_mult = del.pull_radius_mult;
      bc.side_angle_mult = del.side_angle_mult;
      bc.catch_by_owner = true;
      // 850 磁力场: 折返牵引的基础半径/力度取技能 params；
      // 851 的放大由交付层统一乘以 pull_radius_mult，此处不得重复折算
      bc.pull_radius = hasMagnet ? baseRadius : 0.0f;
      bc.pull_strength = hasMagnet ? basePullStrength : 0.0f;
      // 854 巨阙命中硬直由 DoHit 施加，不使用顶点收尾击晕
      bc.stun_on_apex_end = false;
    };

    spawnProj(dir, false);
    // 854 巨阙与 830 幻影回旋互斥：即使 unordered_map 迭代顺序导致 sub_count 被后写，也以巨阙为准
    if (del.sub_count >= 2 && del.giant_armor_scale <= 0.0f) {
      // 830 侧刃呈扇形散开，角度受 831 无尽刃舞缩放
      const float angleRad =
          mech.GetFloat(kSkillId, TriBlade, "side_angle_deg", 14.0f) * del.side_angle_mult * DEG2RAD;
      spawnProj(Vector2Rotate(dir, angleRad), true);
      spawnProj(Vector2Rotate(dir, -angleRad), true);
    }
  }

  static void DoHit(entt::registry &registry, entt::entity attacker, entt::entity target, Tag, bool) {
    if (!registry.valid(target)) {
      return;
    }

    // 命中回调可能传入飞剑实体（携带 BoomerangComponent）而非玩家，统一解析施法者
    entt::entity caster = attacker;
    if (!registry.all_of<ActiveSkillsComponent>(caster)) {
      if (const auto *bc = registry.try_get<BoomerangComponent>(attacker)) {
        caster = bc->owner;
      }
    }

    const auto &mech = data::SkillMechanicsRegistry::Get();
    const auto *profile = SkillSystem::GetBakedSkillProfile(registry, caster, kSkillId);
    BakedDeliveryParams defaultDelivery;
    const BakedDeliveryParams &del = profile ? profile->delivery : defaultDelivery;

    auto specPoints = [&](uint32_t node) -> int {
      if (auto *active = registry.try_get<ActiveSkillsComponent>(caster)) {
        for (const auto &s : active->specialized_slots) {
          if (s.skill_id == kSkillId) {
            return ReadPoints(s, node);
          }
        }
      }
      return 0;
    };
    auto findBoomerang = [&]() -> entt::entity {
      if (registry.all_of<BoomerangComponent>(attacker)) {
        return attacker;
      }
      for (auto e : registry.view<BoomerangComponent>()) {
        if (registry.get<BoomerangComponent>(e).owner == caster) {
          return e;
        }
      }
      return entt::null;
    };

    // 811 放血: 概率施加一层流血，并按该层总伤折算累计到本次飞行的流血池
    const float bleedChance = del.bleed_chance > 0.0f ? del.bleed_chance
                                                      : 0.25f * static_cast<float>(specPoints(Bleed));
    if (bleedChance > 0.0f &&
        GetRandomValue(1, 100) <= static_cast<int>(bleedChance * 100.0f + 0.5f)) {
      const float mag = mech.GetFloat(kSkillId, Bleed, "magnitude",
                                      mech.GetFloat(kSkillId, 0, "bleed_magnitude", 10.0f));
      const float dur = mech.GetFloat(kSkillId, Bleed, "duration",
                                      mech.GetFloat(kSkillId, 0, "bleed_duration", 4.0f));
      systems::AilmentApplyRequest req{
          .ailment = AilmentType::Bleed, .source = caster, .magnitude = mag, .duration = dur, .stacks = 1};
      if (systems::AilmentApplier::Apply(registry, target, req)) {
        const entt::entity boomerang = findBoomerang();
        if (registry.valid(boomerang)) {
          registry.get<BoomerangComponent>(boomerang).bleed_damage_pool += mag * dur;
        }
      }
    }

    // 813 剑鸣: 仅 810 悬停切割期间生效——概率施加护甲击碎，命中已有击碎则延长 1s
    const int pts813 = specPoints(SonicBoom);
    if (pts813 > 0) {
      const entt::entity boomerang = findBoomerang();
      bool inHoverCut = false;
      if (registry.valid(boomerang)) {
        const auto &bc = registry.get<BoomerangComponent>(boomerang);
        const auto *tpos = registry.try_get<Position>(target);
        if (bc.phase == BoomerangPhase::HoverApex && tpos != nullptr) {
          const auto *proj = registry.try_get<Projectile>(boomerang);
          const float cutRadius =
              (proj != nullptr && proj->radius > 0.0f) ? proj->radius : 40.0f;
          inHoverCut =
              Vector2Distance(bc.apex_position, {tpos->x, tpos->y}) <= cutRadius;
        }
      }
      const float shredChance =
          mech.GetFloat(kSkillId, SonicBoom, "shred_chance_pct_per_point", 0.30f) *
          static_cast<float>(pts813);
      if (inHoverCut &&
          GetRandomValue(1, 100) <= static_cast<int>(shredChance * 100.0f + 0.5f)) {
        const float extend = mech.GetFloat(kSkillId, SonicBoom, "shred_extend_sec", 1.0f);
        const float baseDur = mech.GetFloat(kSkillId, 0, "shred_duration", 4.0f);
        float duration = baseDur;
        if (auto *fx = registry.try_get<ActiveEffectsComponent>(target)) {
          for (const auto &eff : fx->effects) {
            if (eff.id == BuffIdToString(BuffId::ArmorShred)) {
              duration = eff.duration + extend;
              break;
            }
          }
        }
        BuffEffect shred{.id = std::string(BuffIdToString(BuffId::ArmorShred)),
                         .name = "Armor Shred",
                         .type = BuffType::DefenseDown,
                         .duration = duration,
                         .remaining = duration,
                         .stacks = 1,
                         .max_stacks = 1,
                         .is_debuff = true};
        shred.modifiers.push_back({.value = -10.0f, .type = StatType::Armor, .mode = ModifierMode::Flat});
        registry.get_or_emplace<ActiveEffectsComponent>(target).AddOrRefresh(shred);
        registry.get_or_emplace<StatsDirty>(target);
      }
    }

    // 854 巨阙: 命中必定打断动作并造成硬直（击晕）；未烘焙时按专精点数回退判定
    const bool isGiant = del.giant_armor_scale > 0.0f || specPoints(Giant) > 0;
    if (isGiant) {
      const float stunDur = mech.GetFloat(kSkillId, Giant, "stun_duration", 1.5f);
      (void)ApplyStun(registry, target, caster, stunDur,
                      "BladeBoomerangGiantStun", kSkillId);
    }

    // 852 意随剑舞: 折返命中概率 +1 剑意
    const float intentChance =
        del.intent_gain_chance > 0.0f ? del.intent_gain_chance
                                      : 0.15f * static_cast<float>(specPoints(IntentGain));
    if (intentChance > 0.0f) {
      const entt::entity boomerang = findBoomerang();
      const bool isReturning =
          registry.valid(boomerang) &&
          registry.get<BoomerangComponent>(boomerang).phase ==
              BoomerangPhase::Returning;
      if (isReturning && GetRandomValue(1, 100) <= static_cast<int>(intentChance * 100.0f + 0.5f)) {
        SkillSystem::GainSwordIntent(registry, caster, 1, kSkillId);
      }
    }
  }
};
REGISTER_SKILL_BEHAVIOR(BladeBoomerang)
void RegisterBladeBoomerang() {}
} // namespace NoMoreDay::skills
