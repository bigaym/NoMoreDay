#include "game/systems/skill/BeamChannelDeliverySystem.hpp"
#include "core/logging/Logger.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "game/contracts/CombatEvents.hpp"
#include "game/contracts/DamagePipelineTypes.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/SkillPointAccess.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/TagRegistry.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/SkillVfxEvent.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace NoMoreDay {

namespace {
constexpr uint32_t kMindBladeSkillId = 7u;
constexpr float kMindBladeBaseDamage = 30.0f; // skills.json 技能7 base_damage

// 查询实体在某技能专精树上某节点的已分配点数；未分配返回 0
int GetSkillPoint(entt::registry &registry, entt::entity entity, uint32_t skill_id,
                  uint32_t node_id) {
  if (const auto *active = registry.try_get<ActiveSkillsComponent>(entity)) {
    for (const auto &spec : active->specialized_slots) {
      if (spec.skill_id == skill_id) {
        return skills::ReadPoints(spec, node_id);
      }
    }
  }
  return 0;
}

int GetSkill5Point(entt::registry &registry, entt::entity entity, uint32_t node_id) {
  return GetSkillPoint(registry, entity, 5u, node_id);
}

int GetSkill7Point(entt::registry &registry, entt::entity entity, uint32_t node_id) {
  return GetSkillPoint(registry, entity, 7u, node_id);
}

// 轻/中型 vs 重型：Boss 与 TANK 视为重型（牵引/孤立判定跳过）
bool IsHeavyEnemy(entt::registry &registry, entt::entity e) {
  if (registry.any_of<BossBattleComponent>(e)) {
    return true;
  }
  if (const auto *state = registry.try_get<EnemyStateComponent>(e)) {
    return state->archetypeType == EnemyArchetype::TANK;
  }
  return false;
}

// 技能7 当前元素：优先取 Baker 输出的 effective_tags，回退到引导组件的转换标签
Tag ResolveSkill7Element(entt::registry &registry, entt::entity caster) {
  if (const auto *profile = SkillSystem::GetBakedSkillProfile(registry, caster, 7u)) {
    if (HasTag(profile->effective_tags, Tag::Cold)) return Tag::Cold;
    if (HasTag(profile->effective_tags, Tag::Lightning)) return Tag::Lightning;
  }
  if (const auto *beam = registry.try_get<BeamChannelComponent>(caster)) {
    if (beam->conversion_tag == Tag::Cold) return Tag::Cold;
    if (beam->conversion_tag == Tag::Lightning) return Tag::Lightning;
  }
  return Tag::Physical;
}

bool HasAilment(entt::registry &registry, entt::entity e, AilmentType type) {
  const auto *fx = registry.try_get<ActiveEffectsComponent>(e);
  if (!fx) return false;
  for (const auto &eff : fx->effects) {
    AilmentType parsed = AilmentType::None;
    if (systems::AilmentAdapter::IsManagedAilmentId(eff.id, &parsed) && parsed == type) {
      return true;
    }
  }
  return false;
}

void ApplyAilmentTo(entt::registry &registry, entt::entity victim,
                    entt::entity source, AilmentType type, float magnitude,
                    float duration, uint8_t stacks) {
  systems::AilmentApplyRequest req{};
  req.ailment = type;
  req.source = source;
  req.magnitude = magnitude;
  req.duration = duration;
  req.stacks = stacks;
  (void)systems::AilmentApplier::Apply(registry, victim, req);
}

// 751 空间粉碎：叠加护甲击碎；达到高层阈值后附加「护甲效率降低」深层减益
void ApplyArmorShred(entt::registry &registry, entt::entity victim, int stacks,
                     int highStackThreshold, float highStackReductionPct) {
  const int cappedStacks = std::max(1, std::min(stacks, highStackThreshold));
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(victim);
  BuffEffect shred{
      .id = "Skill7ArmorShred",
      .name = "Armor Shred",
      .type = BuffType::DefenseDown,
      .duration = 5.0f,
      .remaining = 5.0f,
      .stacks = static_cast<uint8_t>(std::min(cappedStacks, 255)),
      .max_stacks = static_cast<uint8_t>(std::min(highStackThreshold, 255)),
      .is_debuff = true};
  shred.source_skill_id = 7u;
  shred.modifiers.push_back({.value = -10.0f * static_cast<float>(cappedStacks),
                             .type = StatType::Armor,
                             .mode = ModifierMode::Flat});
  effects.AddOrRefresh(shred);
  (void)registry.get_or_emplace<StatsDirty>(victim);

  const BuffEffect *existing = effects.Get("Skill7ArmorShred");
  if (existing && existing->stacks >= highStackThreshold) {
    BuffEffect deep{.id = "Skill7ArmorShredDeep",
                    .name = "Armor Shred (Deep)",
                    .type = BuffType::DefenseDown,
                    .duration = 5.0f,
                    .remaining = 5.0f,
                    .stacks = 1,
                    .max_stacks = 1,
                    .is_debuff = true};
    deep.source_skill_id = 7u;
    deep.modifiers.push_back({.value = -highStackReductionPct * 100.0f,
                              .type = StatType::Armor,
                              .mode = ModifierMode::PercentAdd});
    effects.AddOrRefresh(deep);
    (void)registry.get_or_emplace<StatsDirty>(victim);
  }
}

// 技能7 心剑·无影：Part1 现代 BeamChannelComponent 单管线单帧更新。
// 返回 true 表示引导结束，需由调用方移除 BeamChannelComponent。
bool UpdateMindBladeBeam(entt::registry &registry, systems::SpatialHashGrid &grid,
                         entt::entity caster, BeamChannelComponent &beam,
                         const Position &pos, float dt) {
  const auto &mech = data::SkillMechanicsRegistry::Get();
  const auto *profile = SkillSystem::GetBakedSkillProfile(registry, caster, 7u);
  auto *stats = registry.try_get<CombatStats>(caster);

  const Tag element = ResolveSkill7Element(registry, caster);
  const bool isCold = element == Tag::Cold;
  const bool isLightning = element == Tag::Lightning;
  const Tag effectiveTags =
      profile ? profile->effective_tags
              : (element == Tag::Physical ? Tag::Physical : element);

  // DoD#3 节点判定单源：技能7 的节点点亮一律以运行时专精点数为准。
  // Baker 的 feature_flags 只是离线烘焙快照，profile 缺失（技能7 未重烘）或洗点后
  // 未重烘时会滞后于实际点数，故不再保留 flags/点数双通道判定。
  auto HasNode7 = [&](uint32_t node) {
    return GetSkill7Point(registry, caster, node) > 0;
  };

  const int pts710 = GetSkill7Point(registry, caster, 710);
  const int pts712 = GetSkill7Point(registry, caster, 712);
  const int pts713 = GetSkill7Point(registry, caster, 713);
  const int pts731 = GetSkill7Point(registry, caster, 731);
  const int pts733 = GetSkill7Point(registry, caster, 733);
  const int pts735 = GetSkill7Point(registry, caster, 735);
  const int pts750 = GetSkill7Point(registry, caster, 750);
  const int pts751 = GetSkill7Point(registry, caster, 751);
  const int pts752 = GetSkill7Point(registry, caster, 752);
  const int pts755 = GetSkill7Point(registry, caster, 755);
  const int pts771 = GetSkill7Point(registry, caster, 771);
  const int pts773 = GetSkill7Point(registry, caster, 773);
  const int pts774 = GetSkill7Point(registry, caster, 774);
  const int pts775 = GetSkill7Point(registry, caster, 775);

  const bool has711 = HasNode7(711);
  const bool has712 = HasNode7(712);
  const bool has713 = HasNode7(713);
  const bool has715 = HasNode7(715);
  const bool has730 = HasNode7(730);
  const bool has731 = HasNode7(731);
  const bool has733 = HasNode7(733);
  const bool has735 = HasNode7(735);
  const bool has750 = HasNode7(750);
  const bool has751 = HasNode7(751);
  const bool has752 = HasNode7(752);
  const bool has753 = HasNode7(753);
  const bool has754 = HasNode7(754);
  const bool has770 = HasNode7(770);
  const bool has771 = HasNode7(771);
  const bool has772 = HasNode7(772);
  const bool has773 = HasNode7(773);
  const bool has774 = HasNode7(774);
  const bool has775 = HasNode7(775);
  const bool has734 = HasNode7(734);

  const float maxStacks = mech.GetFloat(7u, 710u, "max_stacks", 4.0f);
  const float baseDamage = kMindBladeBaseDamage;

  // 光标位置裁剪到最大施放距离；基准射程与 Baker 共用机制表键 base_range
  float maxRange = mech.GetFloat(7u, 0u, "base_range", 350.0f);
  if (profile && profile->delivery.range > 0.0f) {
    maxRange = profile->delivery.range;
  }
  Vector2 cutPos = beam.target_pos;
  {
    Vector2 diff = Vector2Subtract(beam.target_pos, {pos.x, pos.y});
    const float dist = Vector2Length(diff);
    if (dist > maxRange && dist > 0.001f) {
      const Vector2 dir = Vector2Scale(diff, 1.0f / dist);
      cutPos = {pos.x + dir.x * maxRange, pos.y + dir.y * maxRange};
    }
  }

  const float radius = (profile && profile->area_radius > 1.0f)
                           ? profile->area_radius
                           : mech.GetFloat(7u, 0u, "base_radius", 60.0f);
  const float centerRadius = mech.GetFloat(7u, 715u, "center_radius", 80.0f);
  const float isolationRadius = mech.GetFloat(7u, 735u, "isolation_radius", 120.0f);

  // 735 精准切割：以指定中心统计孤立半径内敌人数，仅当恰好 1 个敌人时视为孤立目标
  auto isIsolatedAt = [&](Vector2 center) -> bool {
    if (!has735) return false;
    int count = 0;
    grid.query({center.x, center.y}, isolationRadius,
               [&](entt::entity e, const Position &) {
                 if (registry.any_of<EnemyTag>(e) && !registry.any_of<KilledTag>(e)) {
                   count++;
                 }
               });
    return count == 1;
  };

  // ---- 命中修正计算 ----
  auto computeMore = [&](entt::entity victim, bool isolated) -> float {
    float more = profile ? profile->more_damage_mult : 1.0f;
    if (HasNode7(710)) {
      more *= (1.0f + mech.GetFloat(7u, 710u, "more_per_stack", 0.05f) *
                           static_cast<float>(beam.charge_stacks));
    }
    if (has753) {
      more *= (1.0f + mech.GetFloat(7u, 753u, "more_per_intent", 0.20f) *
                           static_cast<float>(beam.intent_stacks));
    }
    if (has735 && isolated) {
      more *= (1.0f + mech.GetFloat(7u, 735u, "isolated_more_pct_per_point", 0.15f) *
                           static_cast<float>(pts735));
    }
    if (has774) {
      const bool hasChill = HasAilment(registry, victim, AilmentType::Chill) ||
                            HasAilment(registry, victim, AilmentType::Freeze);
      const bool hasShock = HasAilment(registry, victim, AilmentType::Shock);
      if (isCold && hasChill) {
        more *= (1.0f + mech.GetFloat(7u, 774u, "ailment_more_pct_per_point", 0.10f) *
                             static_cast<float>(pts774));
      } else if (isLightning && hasShock) {
        more *= (1.0f + mech.GetFloat(7u, 774u, "ailment_more_pct_per_point", 0.10f) *
                             static_cast<float>(pts774));
      }
    }
    // 771 极寒碎骨：命中冻结目标时击碎增伤
    if (has771 && HasAilment(registry, victim, AilmentType::Freeze)) {
      more *= (1.0f + mech.GetFloat(7u, 771u, "shatter_damage_pct_per_point", 0.20f) *
                           static_cast<float>(pts771));
    }
    return more;
  };

  auto critChanceFor = [&](entt::entity victim) -> float {
    float c = stats ? stats->crit_chance : 0.0f;
    if (profile) c += profile->delivery.bonus_crit;
    if (has773 && isLightning && HasAilment(registry, victim, AilmentType::Shock)) {
      // 统一分数制：mech 值本身即每点小数暴击率
      c += mech.GetFloat(7u, 773u, "shock_crit_pct_per_point", 0.05f) *
           static_cast<float>(pts773);
    }
    return c;
  };

  auto critMultFor = [&](entt::entity, bool inCenter) -> float {
    float m = stats ? stats->crit_damage : 1.5f;
    if (profile) m += profile->delivery.bonus_crit_damage;
    if (has715 && inCenter) {
      m *= (1.0f + mech.GetFloat(7u, 715u, "crit_dmg_pct_per_point", 0.20f) *
                       static_cast<float>(GetSkill7Point(registry, caster, 715)));
    }
    return m;
  };

  // 755 心念反哺：击杀回蓝；精英以上必回剑意
  auto onKill = [&](entt::entity victim) {
    if (pts755 <= 0) return;
    if (stats) {
      const float restore = mech.GetFloat(7u, 755u, "mana_restore_per_kill_per_point", 3.0f) *
                            static_cast<float>(pts755);
      const float maxMana = stats->max_mana > 0.0f ? stats->max_mana : stats->mana;
      stats->mana = std::min(maxMana, stats->mana + restore);
      (void)registry.get_or_emplace<StatsDirty>(caster);
    }
    if (IsHeavyEnemy(registry, victim)) {
      (void)SkillSystem::GainSwordIntent(
          registry, caster,
          static_cast<int>(mech.GetFloat(7u, 755u, "elite_intent_restore", 1.0f)), 7u);
    }
  };

  // 命中节点效果：护甲击碎 / 流血 / 抗性上限压制 / 寒冷
  auto onHitNodesFn = [&](entt::entity victim, bool inCenter) {
    if (has751) {
      const float chance = mech.GetFloat(7u, 751u, "shred_chance_pct_per_point", 0.60f) *
                           static_cast<float>(pts751);
      if (utils::ThreadSafeRandom::GetFloat01() < chance) {
        const int shredStacks = 1 + (chance > 1.0f ? 1 : 0);
        ApplyArmorShred(registry, victim, shredStacks,
                        static_cast<int>(mech.GetFloat(7u, 751u, "high_stack_threshold", 10.0f)),
                        mech.GetFloat(7u, 751u, "high_stack_armor_reduction_pct", 0.15f));
      }
    }
    if (has752) {
      const int stacks = static_cast<int>(
                             mech.GetFloat(7u, 752u, "bleed_stacks_per_point", 1.0f)) *
                         pts752;
      if (stacks > 0) {
        float bleedMagnitude = baseDamage * 0.1f;
        // 中心范围内流血伤害加速爆发：以强度提升近似「更快爆发」语义；
        // 复用 applyHit 按本次命中所属撕裂中心算出的 centerHit，次级撕裂随 secPos 生效
        if (inCenter) {
          bleedMagnitude *=
              (1.0f + mech.GetFloat(7u, 752u, "bleed_burst_speedup_pct", 0.20f));
        }
        ApplyAilmentTo(registry, victim, caster, AilmentType::Bleed, bleedMagnitude,
                       3.0f, static_cast<uint8_t>(std::min(stacks, 255)));
      }
    }
    if (has775 && inCenter && (isCold || isLightning)) {
      auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(victim);
      BuffEffect suppress{.id = "Skill7MentalSuppression",
                          .name = "Mental Suppression",
                          .type = BuffType::DefenseDown,
                          .duration = 1.0f,
                          .remaining = 1.0f,
                          .stacks = 1,
                          .max_stacks = 1,
                          .is_debuff = true};
      suppress.source_skill_id = 7u;
      suppress.resist_cap_suppression =
          mech.GetFloat(7u, 775u, "cap_suppression_pct_per_point", 0.03f) *
          static_cast<float>(pts775);
      suppress.resist_cap_element = isCold ? Tag::Cold : Tag::Lightning;
      effects.AddOrRefresh(suppress);
    }
    if (has770 && isCold) {
      ApplyAilmentTo(registry, victim, caster, AilmentType::Chill, baseDamage * 0.1f, 2.0f, 1);
    }
  };

  // 统一命中结算：payload_context 携带基准伤害/暴击/more/元素，经 ResolveDamage 走 DamagePipeline
  auto applyHit = [&](entt::entity victim, const Position &vp, float hitBaseDamage,
                      bool isolated, const Vector2 &hitCenter, bool applyOnHitNodes) {
    // 中心判定以本次命中所属的撕裂中心为准：主撕裂传 cutPos，730 次级撕裂传 secPos
    const float distFromCenter =
        Vector2Distance({vp.x, vp.y}, {hitCenter.x, hitCenter.y});
    const bool centerHit = distFromCenter <= centerRadius;
    DamagePayloadContext ctx{};
    ctx.base_damage_min = hitBaseDamage;
    ctx.base_damage_max = hitBaseDamage;
    ctx.crit_chance = critChanceFor(victim); // 统一分数制 [0,1]
    ctx.crit_multiplier = critMultFor(victim, centerHit);
    ctx.increased_damage = 0.0f;
    ctx.more_damage = computeMore(victim, isolated);
    ctx.effective_tags = effectiveTags;
    ctx.source_skill_id = 7u;

    DamageRequest req{};
    req.attacker = caster;
    req.defender = victim;
    req.skill_id = 7u;
    req.source_entity = caster;
    req.additional_tags = effectiveTags | Tag::Area;
    req.payload_context = ctx;
    const auto result = ResolveDamage(registry, req, caster);
    if (result.target_killed) {
      onKill(victim);
    }
    if (applyOnHitNodes && registry.valid(victim) && !registry.any_of<KilledTag>(victim)) {
      onHitNodesFn(victim, centerHit);
    }
  };

  // 754 剑意化无：消耗剑意的同一时刻，令活跃飞剑瞬移至撕裂中心高频穿刺一次
  auto formlessIntent = [&]() {
    if (!has754) return;
    bool anySword = false;
    auto swordView = registry.view<SpiritSwordTag, SummonComponent, Position>();
    for (auto sw : swordView) {
      const auto &sc = swordView.get<SummonComponent>(sw);
      if (sc.owner != caster) continue;
      auto &sp = swordView.get<Position>(sw);
      sp.x = cutPos.x;
      sp.y = cutPos.y;
      anySword = true;
    }
    if (!anySword) return;
    const float eff = mech.GetFloat(7u, 754u, "pierce_effectiveness", 0.30f);
    const bool pierceIsolated = isIsolatedAt(cutPos);
    grid.query({cutPos.x, cutPos.y}, radius * 0.5f,
               [&](entt::entity e, const Position &ep) {
                 if (!registry.any_of<EnemyTag>(e) || registry.any_of<KilledTag>(e)) return;
                 applyHit(e, ep, baseDamage * eff, pierceIsolated, cutPos, true);
               });
  };

  // 结束收尾：711 碎空爆引爆 → OnChannelEnd 派发
  auto finish = [&]() {
    if (has711) {
      const float dRadius =
          mech.GetFloat(7u, 711u, "detonation_radius", 120.0f) *
          (1.0f + mech.GetFloat(7u, 711u, "detonation_radius_per_stack", 0.15f) *
                       static_cast<float>(beam.charge_stacks));
      const float dMult = mech.GetFloat(7u, 711u, "detonation_damage_mult", 6.0f);
      auto &particleSys = systems::GPUParticleSystem::Get();
      for (int i = 0; i < 24; ++i) {
        const float a = utils::ThreadSafeRandom::GetFloat(0.0f, 2.0f * PI);
        const float r = utils::ThreadSafeRandom::GetFloat(0.0f, dRadius);
        components::GPUParticle p{};
        p.position = {cutPos.x + std::cos(a) * r, cutPos.y + std::sin(a) * r};
        p.velocity = {std::cos(a) * 120.0f, std::sin(a) * 120.0f};
        p.color = isLightning ? Color{214, 188, 255, 220}
                  : (isCold ? Color{210, 245, 255, 218} : Color{150, 190, 255, 220});
        p.scale = 6.0f;
        p.lifetime = 0.4f;
        p.maxLifetime = 0.4f;
        p.flags = 2;
        p.growthRate = -6.0f;
        particleSys.Emit(p);
      }
      RenderSystem::AddDistortionSource(cutPos.x, cutPos.y, dRadius * 0.6f, 0.35f);
      const bool detonationIsolated = isIsolatedAt(cutPos);
      grid.query({cutPos.x, cutPos.y}, dRadius,
                 [&](entt::entity e, const Position &ep) {
                   if (!registry.any_of<EnemyTag>(e) || registry.any_of<KilledTag>(e)) return;
                   applyHit(e, ep, baseDamage * dMult, detonationIsolated, cutPos, true);
                 });
    }
    const CombatEvent evt = CombatEventFactory::CreateChannelEnd(
        caster, 7u, effectiveTags, beam.current_channel_time);
    CombatEventDispatcher::Dispatch(registry, evt);
  };

  // ---- 生命周期 ----
  // 734 神游脱战：输入层登记打断请求 → 扣蓝、瞬移、加速、收尾
  if (beam.interrupt_requested) {
    beam.interrupt_requested = false;
    if (has734) {
      const float blinkCost = mech.GetFloat(7u, 734u, "blink_mana_cost", 30.0f);
      if (!stats || stats->mana >= blinkCost) {
        if (stats) {
          stats->mana -= blinkCost;
          (void)registry.get_or_emplace<StatsDirty>(caster);
        }
        Vector2 dir = Vector2Subtract(beam.interrupt_target, {pos.x, pos.y});
        const float len = Vector2Length(dir);
        if (len > 0.001f) {
          dir = Vector2Scale(dir, 1.0f / len);
          if (auto *p = registry.try_get<Position>(caster)) {
            p->x += dir.x * mech.GetFloat(7u, 734u, "blink_distance", 250.0f);
            p->y += dir.y * mech.GetFloat(7u, 734u, "blink_distance", 250.0f);
          }
        }
        const float speedDur = mech.GetFloat(7u, 734u, "speed_bonus_duration", 2.0f);
        auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(caster);
        BuffEffect haste{.id = "Skill7AstralShift",
                         .name = "Astral Shift",
                         .type = BuffType::SpeedUp,
                         .duration = speedDur,
                         .remaining = speedDur,
                         .stacks = 1,
                         .max_stacks = 1,
                         .is_debuff = false};
        haste.modifiers.push_back(
            {.value = mech.GetFloat(7u, 734u, "speed_bonus_pct", 0.15f) * 100.0f,
             .type = StatType::MoveSpeed,
             .mode = ModifierMode::PercentAdd});
        effects.AddOrRefresh(haste);
        (void)registry.get_or_emplace<StatsDirty>(caster);
      }
    }
    finish();
    return true;
  }

  beam.current_channel_time += dt;
  if (beam.current_channel_time >= beam.max_channel_time) {
    finish();
    return true;
  }
  // 松开按键（输入保活窗口计时耗尽）
  beam.channel_timer -= dt;
  if (beam.channel_timer <= 0.0f) {
    finish();
    return true;
  }

  // ---- 每帧视觉：空间撕裂环 / 畸变 / 链粒子 / 电弧 ----
  {
    const bool isEmpowered = beam.is_empowered;
    const bool hasVoidRift = HasTag(effectiveTags, Tag::Void) ||
                             beam.conversion_tag == Tag::Void;
    const uint8_t elementType = SkillSystem::EncodeSkillVfxElementType(effectiveTags);
    Vector2 dir = Vector2Subtract(beam.target_pos, {pos.x, pos.y});
    const float dlen = Vector2Length(dir);
    dir = (dlen > 0.001f) ? Vector2Scale(dir, 1.0f / dlen) : Vector2{1.0f, 0.0f};
    if (dlen <= 0.001f) cutPos = {pos.x + 50.0f, pos.y};

    components::GPUSkillEffect riftRing = {};
    riftRing.position = cutPos;
    riftRing.velocity = Vector2Scale(dir, 40.0f);
    riftRing.coreColor = isEmpowered ? Vector4{0.22f, 0.30f, 0.48f, 0.98f}
                          : (hasVoidRift ? Vector4{0.08f, 0.07f, 0.14f, 0.95f}
                                         : Vector4{0.18f, 0.24f, 0.38f, 0.90f});
    riftRing.glowColor = isLightning ? Vector4{0.72f, 0.56f, 1.00f, 0.90f}
                         : (isCold ? Vector4{0.76f, 0.92f, 1.00f, 0.88f}
                                   : Vector4{0.46f, 0.74f, 1.00f, 0.84f});
    riftRing.radius = 24.0f;
    riftRing.sectorAngle = 360.0f;
    riftRing.type = isEmpowered   ? 7.0f
                    : hasVoidRift ? 3.0f
                    : isLightning ? 6.0f
                    : isCold      ? 5.0f
                                  : 4.0f;
    riftRing.flags = NoMoreDay::render::skillfx::PackSkillEffectFlags(elementType, 7u);
    systems::GPUSkillEffectSystem::Get().Submit(riftRing);

    const float distortionRadius = isEmpowered ? 34.0f : (hasVoidRift ? 32.0f : 28.0f);
    const float distortionStrength = isEmpowered ? 0.30f : (hasVoidRift ? 0.26f : 0.22f);
    RenderSystem::AddDistortionSource(cutPos.x, cutPos.y, distortionRadius,
                                      distortionStrength);

    auto &particleSys = systems::GPUParticleSystem::Get();
    if (utils::ThreadSafeRandom::GetFloat(0.0f, 1.0f) < 700.0f * dt) {
      constexpr int kSamples = 6;
      for (int i = 1; i <= kSamples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSamples + 1);
        const Vector2 samplePos = Vector2Lerp({pos.x, pos.y}, cutPos, t);
        components::GPUParticle link = {};
        link.position = samplePos;
        link.velocity = {0.0f, 0.0f};
        link.acceleration = {0.0f, 0.0f};
        link.color = isEmpowered ? Color{236, 246, 255, 96} : Color{185, 225, 240, 48};
        link.scale = isEmpowered ? 3.2f : 2.6f;
        link.lifetime = 0.13f;
        link.maxLifetime = 0.13f;
        link.flags = 1;
        link.growthRate = -4.0f;
        particleSys.Emit(link);
      }
    }
    if (isLightning && utils::ThreadSafeRandom::GetFloat(0.0f, 1.0f) < 800.0f * dt) {
      for (int i = 0; i < 2; ++i) {
        components::GPUParticle arc = {};
        arc.position = {cutPos.x + utils::ThreadSafeRandom::GetFloat(-18.0f, 18.0f),
                        cutPos.y + utils::ThreadSafeRandom::GetFloat(-18.0f, 18.0f)};
        arc.velocity = {utils::ThreadSafeRandom::GetFloat(-40.0f, 40.0f),
                        utils::ThreadSafeRandom::GetFloat(-40.0f, 40.0f)};
        arc.acceleration = {0.0f, 0.0f};
        arc.color = Color{220, 188, 255, 215};
        arc.scale = 4.4f;
        arc.lifetime = 0.18f;
        arc.maxLifetime = 0.18f;
        arc.flags = 2;
        arc.growthRate = -9.0f;
        particleSys.Emit(arc);
      }
    }
  }

  // ---- 蓄积层数（710 心流叠加 / 711 碎空爆） ----
  if (HasNode7(710) || has711) {
    float interval = mech.GetFloat(7u, 710u, "stack_interval", 0.5f);
    if (has713) {
      interval /= (1.0f + mech.GetFloat(7u, 713u, "charge_speed_pct_per_point", 0.20f) *
                             static_cast<float>(pts713));
    }
    beam.charge_timer += dt;
    while (beam.charge_timer >= interval && interval > 0.0f) {
      beam.charge_timer -= interval;
      if (static_cast<float>(beam.charge_stacks) < maxStacks) {
        beam.charge_stacks++;
      }
    }
    // 713 满层后继续引导：每秒流失最大生命 5%（死亡安全下限 1 点）
    if (has713 && static_cast<float>(beam.charge_stacks) >= maxStacks) {
      if (auto *hp = registry.try_get<HealthComponent>(caster)) {
        hp->current -= hp->max * mech.GetFloat(7u, 713u, "self_damage_pct_per_sec", 0.05f) * dt;
        hp->current = std::max(1.0f, hp->current);
      }
    }
  }

  // ---- 753 意念风暴：每秒最多消耗 2 层剑意，每层 +20% more；754 剑意化无联动 ----
  if (has753) {
    beam.intent_timer += dt;
    const float perSec = std::max(0.5f, mech.GetFloat(7u, 753u, "max_intent_per_sec", 2.0f));
    const float consumeInterval = 1.0f / perSec;
    while (beam.intent_timer >= consumeInterval) {
      beam.intent_timer -= consumeInterval;
      if (!SkillSystem::ConsumeSwordIntent(registry, caster, 1, 7u)) {
        break;
      }
      beam.intent_stacks++;
      formlessIntent();
    }
  }

  // ---- 712 虚无牵引：711 蓄力期间持续向心拖拽轻/中型敌人 ----
  if (has712 && has711) {
    const float pullRadius =
        mech.GetFloat(7u, 712u, "pull_radius", 200.0f) +
        mech.GetFloat(7u, 712u, "pull_radius_per_point", 133.33f) * static_cast<float>(pts712);
    const float pullStrength = mech.GetFloat(7u, 712u, "pull_strength", 180.0f);
    grid.query({cutPos.x, cutPos.y}, pullRadius, [&](entt::entity e, const Position &ep) {
      if (!registry.any_of<EnemyTag>(e) || registry.any_of<KilledTag>(e)) return;
      if (IsHeavyEnemy(registry, e)) return;
      if (auto *vel = registry.try_get<Velocity>(e)) {
        Vector2 to = Vector2Subtract(cutPos, {ep.x, ep.y});
        const float len = Vector2Length(to);
        if (len > 0.001f) {
          to = Vector2Scale(to, 1.0f / len);
          vel->vx += to.x * pullStrength * dt;
          vel->vy += to.y * pullStrength * dt;
        }
      }
    });
  }

  // ---- 770 天外冰晶：周期生成向外飞散冰片投射物 ----
  if (has770 && isCold && !has711) {
    beam.shard_timer -= dt;
    if (beam.shard_timer <= 0.0f) {
      beam.shard_timer = std::max(0.1f, beam.tick_interval);
      const int count =
          static_cast<int>(mech.GetFloat(7u, 770u, "shard_count", 5.0f));
      const float shardPct = mech.GetFloat(7u, 770u, "shard_damage_pct", 0.30f);
      const float shardSpeed = mech.GetFloat(7u, 770u, "shard_speed", 400.0f);
      for (int i = 0; i < count; ++i) {
        const float a = (static_cast<float>(i) / static_cast<float>(std::max(1, count))) *
                            2.0f * PI +
                        utils::ThreadSafeRandom::GetFloat(-0.2f, 0.2f);
        const Vector2 dir = {std::cos(a), std::sin(a)};
        auto projEnt = registry.create();
        registry.emplace<LocalLevelTag>(projEnt);
        registry.emplace<Position>(projEnt, cutPos.x + dir.x * 10.0f,
                                   cutPos.y + dir.y * 10.0f);
        registry.emplace<Velocity>(projEnt, dir.x * shardSpeed, dir.y * shardSpeed);
        registry.emplace<ColorComponent>(projEnt, SKYBLUE);
        auto &proj = registry.emplace<Projectile>(projEnt);
        proj.owner = caster;
        proj.cast_id = beam.cast_id;
        proj.radius = 12.0f;
        proj.speed = shardSpeed;
        proj.lifeTime = 0.6f;
        proj.visualType = 2;
        proj.pierce = true;
        proj.pierceCount = 999;
        proj.max_pierce = Projectile::kUnlimitedPiercing;
        if (stats) {
          proj.snapshot = *stats;
          registry.emplace<CombatStats>(projEnt, proj.snapshot);
        }
        DamagePayloadContext ctx{};
        ctx.base_damage_min = baseDamage;
        ctx.base_damage_max = baseDamage;
        ctx.crit_chance = stats ? stats->crit_chance : 0.0f;
        ctx.crit_multiplier = stats ? stats->crit_damage : 1.5f;
        // 冰片按 shard_damage_pct 缩放；更多伤取技能总 more
        ctx.more_damage = shardPct * (profile ? profile->more_damage_mult : 1.0f);
        ctx.effective_tags = Tag::Cold;
        ctx.source_skill_id = 7u;
        proj.payload_context = ctx;
        auto &sc = registry.emplace<SkillComponent>(projEnt);
        sc.skill_id = 7u;
        auto &mods = registry.emplace<SkillModifierComponent>(projEnt);
        mods.damage_modifiers.push_back(
            DamageModifier{Tag::Physical, Tag::Cold, 1.0f, ModifierType::Convert});
      }
      // 771 极寒碎骨近似：冰刺碎裂时对中心附近敌人强制施加 1 层寒冷
      if (has771) {
        grid.query({cutPos.x, cutPos.y}, radius, [&](entt::entity e, const Position &) {
          if (!registry.any_of<EnemyTag>(e) || registry.any_of<KilledTag>(e)) return;
          ApplyAilmentTo(registry, e, caster, AilmentType::Chill, baseDamage * 0.1f, 2.0f, 1);
        });
      }
    }
  }

  // ---- 772 神雷天罡：每 0.5s 在光标处落雷并留下 2s 感电区域 ----
  if (has772 && isLightning && !has711) {
    beam.pillar_timer += dt;
    const float pillarInterval = mech.GetFloat(7u, 772u, "pillar_interval", 0.5f);
    if (beam.pillar_timer >= pillarInterval) {
      beam.pillar_timer -= pillarInterval;
      const float pillarMult = mech.GetFloat(7u, 772u, "pillar_damage_mult", 2.0f);
      // 主目标 = 距落点最近的敌人（773 对其额外 more）
      entt::entity mainTarget = entt::null;
      float bestDistSq = std::numeric_limits<float>::max();
      grid.query({cutPos.x, cutPos.y}, radius, [&](entt::entity e, const Position &ep) {
        if (!registry.any_of<EnemyTag>(e) || registry.any_of<KilledTag>(e)) return;
        const float dSq = Vector2DistanceSqr({cutPos.x, cutPos.y}, {ep.x, ep.y});
        if (dSq < bestDistSq) {
          bestDistSq = dSq;
          mainTarget = e;
        }
      });
      const bool pillarIsolated = isIsolatedAt(cutPos);
      grid.query({cutPos.x, cutPos.y}, radius, [&](entt::entity e, const Position &ep) {
        if (!registry.any_of<EnemyTag>(e) || registry.any_of<KilledTag>(e)) return;
        float pillarBase = baseDamage * pillarMult;
        if (has773 && e == mainTarget) {
          pillarBase *= (1.0f + mech.GetFloat(7u, 773u, "main_target_more_pct_per_point", 0.15f) *
                                 static_cast<float>(pts773));
        }
        applyHit(e, ep, pillarBase, pillarIsolated, cutPos, true);
      });
      // 落雷视觉
      auto &particleSys = systems::GPUParticleSystem::Get();
      for (int i = 0; i < 10; ++i) {
        components::GPUParticle p{};
        p.position = {cutPos.x + utils::ThreadSafeRandom::GetFloat(-14.0f, 14.0f),
                      cutPos.y + utils::ThreadSafeRandom::GetFloat(-14.0f, 14.0f)};
        p.velocity = {0.0f, -80.0f};
        p.color = Color{214, 188, 255, 230};
        p.scale = 5.0f;
        p.lifetime = 0.25f;
        p.maxLifetime = 0.25f;
        p.flags = 2;
        particleSys.Emit(p);
      }
      // 感电区域
      auto fieldEnt = registry.create();
      registry.emplace<LocalLevelTag>(fieldEnt);
      registry.emplace<Position>(fieldEnt, cutPos.x, cutPos.y);
      auto &field = registry.emplace<ShockFieldComponent>(fieldEnt);
      field.owner = caster;
      field.skill_id = 7u;
      field.center = cutPos;
      field.radius = radius;
      field.remaining = mech.GetFloat(7u, 772u, "shock_field_duration", 2.0f);
      field.tick_interval = 0.5f;
      field.tick_timer = 0.5f;
      field.damage = baseDamage * pillarMult;
    }
  }

  // ---- tick 结算 ----
  beam.tick_timer -= dt;
  if (beam.tick_timer > 0.0f) {
    return false;
  }
  beam.tick_timer = std::max(0.05f, beam.tick_interval);

  // 扣蓝（H9）：照抄技能5 模型，effective_mana_cost 已含 700 减免与 732 增耗
  {
    const float manaRate = profile ? profile->effective_mana_cost : 15.0f;
    const float manaCost = manaRate * beam.tick_interval;
    if (stats) {
      if (stats->mana < manaCost) {
        finish();
        return true;
      }
      stats->mana -= manaCost;
      (void)registry.get_or_emplace<StatsDirty>(caster);
    }
  }

  // 751/752/775 等命中节点在 711 蓄力期间不触发（无 tick 命中）
  if (has711) {
    return false;
  }

  // 735 孤立目标：以撕裂中心为基准判定是否仅命中一个敌人
  const bool isolated = isIsolatedAt(cutPos);

  // 主撕裂范围伤害
  grid.query({cutPos.x, cutPos.y}, radius, [&](entt::entity e, const Position &ep) {
    if (!registry.any_of<EnemyTag>(e) || registry.any_of<KilledTag>(e)) return;
    applyHit(e, ep, baseDamage, isolated, cutPos, true);
  });

  // 切割斩击 slashBody + 碎片粒子（迁移自旧 Part2 技能7 tick 命中）
  {
    Vector2 dirToCenter = Vector2Subtract(cutPos, {pos.x, pos.y});
    const float aimLen = Vector2Length(dirToCenter);
    dirToCenter = (aimLen > 0.001f) ? Vector2Scale(dirToCenter, 1.0f / aimLen)
                                    : Vector2{1.0f, 0.0f};
    const float halfLen = 60.0f;
    components::GPUSkillEffect slashBody = {};
    slashBody.position = {pos.x + dirToCenter.x * halfLen,
                          pos.y + dirToCenter.y * halfLen};
    slashBody.velocity = Vector2Scale(dirToCenter, 60.0f);
    slashBody.coreColor = isLightning ? Vector4{0.72f, 0.56f, 1.00f, 0.95f}
                          : (isCold ? Vector4{0.76f, 0.92f, 1.00f, 0.92f}
                                    : Vector4{0.55f, 0.80f, 1.00f, 0.90f});
    slashBody.glowColor = slashBody.coreColor;
    slashBody.radius = halfLen * 1.35f;
    slashBody.sectorAngle = 60.0f;
    slashBody.type = 2.0f; // Blade
    slashBody.flags = render::skillfx::PackSkillEffectFlags(
        SkillSystem::EncodeSkillVfxElementType(effectiveTags), 7u);
    systems::GPUSkillEffectSystem::Get().Submit(slashBody);

    auto &pSys = systems::GPUParticleSystem::Get();
    for (int i = 0; i < 6; ++i) {
      const float a = utils::ThreadSafeRandom::GetFloat(0.0f, 2.0f * PI);
      components::GPUParticle sp{};
      sp.position = cutPos;
      sp.velocity = {std::cos(a) * utils::ThreadSafeRandom::GetFloat(40.0f, 140.0f),
                     std::sin(a) * utils::ThreadSafeRandom::GetFloat(40.0f, 140.0f)};
      sp.color = Color{170, 210, 255, 200};
      sp.scale = 3.5f;
      sp.lifetime = 0.3f;
      sp.maxLifetime = 0.3f;
      sp.flags = 2;
      sp.growthRate = -5.0f;
      pSys.Emit(sp);
    }
  }

  // 750 引力坍缩：概率对范围内轻/中型敌人向心拉扯
  if (has750) {
    const float chance = mech.GetFloat(7u, 750u, "pull_chance_pct_per_point", 0.25f) *
                         static_cast<float>(pts750);
    if (utils::ThreadSafeRandom::GetFloat01() < chance) {
      const float pullRadius = mech.GetFloat(7u, 750u, "pull_radius", 200.0f);
      const float pullStrength = mech.GetFloat(7u, 750u, "pull_strength", 150.0f);
      grid.query({cutPos.x, cutPos.y}, pullRadius, [&](entt::entity e, const Position &ep) {
        if (!registry.any_of<EnemyTag>(e) || registry.any_of<KilledTag>(e)) return;
        if (IsHeavyEnemy(registry, e)) return;
        if (auto *vel = registry.try_get<Velocity>(e)) {
          Vector2 to = Vector2Subtract(cutPos, {ep.x, ep.y});
          const float len = Vector2Length(to);
          if (len > 0.001f) {
            to = Vector2Scale(to, 1.0f / len);
            vel->vx += to.x * pullStrength;
            vel->vy += to.y * pullStrength;
          }
        }
      });
    }
  }

  // 730/731 神识多开 / 千面阵：随机敌人脚下生成次级撕裂
  if (has730) {
    int extraCount = static_cast<int>(mech.GetFloat(7u, 730u, "secondary_count", 1.0f));
    if (has731) {
      extraCount += static_cast<int>(
                        mech.GetFloat(7u, 731u, "secondary_count_per_point", 1.0f)) *
                    pts731;
    }
    const float secondaryDamagePct =
        has733 ? mech.GetFloat(7u, 733u, "secondary_damage_pct", 1.0f)
               : mech.GetFloat(7u, 730u, "secondary_damage_pct", 0.50f);
    float secondaryRadius = radius;
    if (has733) {
      secondaryRadius *=
          (1.0f + mech.GetFloat(7u, 733u, "secondary_radius_pct_per_point", 0.20f) *
                      static_cast<float>(pts733));
    }
    std::vector<entt::entity> candidates;
    grid.query({cutPos.x, cutPos.y}, 700.0f, [&](entt::entity e, const Position &) {
      if (registry.any_of<EnemyTag>(e) && !registry.any_of<KilledTag>(e)) {
        candidates.push_back(e);
      }
    });
    for (int i = 0; i < extraCount && !candidates.empty(); ++i) {
      const int idx = utils::ThreadSafeRandom::GetInt(0, static_cast<int>(candidates.size()) - 1);
      const entt::entity secTarget = candidates[idx];
      if (!registry.valid(secTarget) || !registry.all_of<Position>(secTarget)) continue;
      const auto &sp = registry.get<Position>(secTarget);
      const Vector2 secPos = {sp.x, sp.y};
      const bool secondaryIsolated = isIsolatedAt(secPos);
      grid.query({secPos.x, secPos.y}, secondaryRadius, [&](entt::entity e, const Position &ep) {
        if (!registry.any_of<EnemyTag>(e) || registry.any_of<KilledTag>(e)) return;
        applyHit(e, ep, baseDamage * secondaryDamagePct, secondaryIsolated, secPos, true);
      });
    }
  }

  return false;
}
} // namespace

namespace skills {

// 552 随影落点采样：在 center 周围半径 radius 的圆环上按角度取点。
// 抽为具名纯函数，使交付层与回归测试共用同一「严格环形」语义，替代旧圆盘随机采样。
Vector2 SampleFollowingShadowRingPoint(Vector2 center, float radius, float angle_deg) {
  const float rad = angle_deg * DEG2RAD;
  return {center.x + std::cos(rad) * radius, center.y + std::sin(rad) * radius};
}

} // namespace skills

void BeamChannelDeliverySystem::Update(entt::registry &registry,
                                       systems::SpatialHashGrid &grid,
                                       float dt) {
  // 1. 纯参数驱动的现代 BeamChannelComponent (Task 2.4b, 2.4d)
  static thread_local std::vector<entt::entity> s_beam_to_remove;
  s_beam_to_remove.clear();

  auto beam_view = registry.view<BeamChannelComponent, Position>();
  auto triggerSwordGodFinisher = [&](entt::entity ent, const BeamChannelComponent &b) {
    // 534 天剑降世与 535 余波机制数值统一走 skill_mechanics.json（技能5/534、技能5/535）
    const float minChannelDuration =
        data::SkillMechanicsRegistry::Get().GetFloat(5u, 534, "min_channel_duration", 2.0f);
    const float giantSwordRadius =
        data::SkillMechanicsRegistry::Get().GetFloat(5u, 534, "giant_sword_radius", 120.0f);
    const float giantSwordDamageMult =
        data::SkillMechanicsRegistry::Get().GetFloat(5u, 534, "giant_sword_damage_mult", 8.0f);
    const float knockupForce =
        data::SkillMechanicsRegistry::Get().GetFloat(5u, 534, "knockup_force", 300.0f);
    const float shockwaveRadiusPerPoint =
        data::SkillMechanicsRegistry::Get().GetFloat(5u, 535, "radius_pct_per_point", 0.20f);
    const float shockwaveStunDuration =
        data::SkillMechanicsRegistry::Get().GetFloat(5u, 535, "stun_duration", 1.5f);

    if (b.skill_id != 5 || b.current_channel_time < minChannelDuration) return;
    const auto *prof = SkillSystem::GetBakedSkillProfile(registry, ent, 5u);
    if (!prof || (prof->delivery.feature_flags & 8192) == 0) return; // 534 天剑降世

    int pts_535 = GetSkill5Point(registry, ent, 535);
    float explosion_radius = giantSwordRadius * (1.0f + shockwaveRadiusPerPoint * static_cast<float>(pts_535));
    auto *stats = registry.try_get<CombatStats>(ent);
    float baseDmg = stats ? ((stats->min_weapon_damage + stats->max_weapon_damage) * 0.5f) : 50.0f;

    grid.query({b.target_pos.x, b.target_pos.y}, explosion_radius, [&](entt::entity e, const Position &ep) {
      if (registry.any_of<EnemyTag>(e) && !registry.any_of<KilledTag>(e)) {
        DamagePool pool;
        pool.Add(Tag::Physical, baseDmg * giantSwordDamageMult); // 800% 物理伤害
        DamageRequest req;
        req.origin = DamageOrigin::SecondaryProc;
        req.attacker = ent;
        req.defender = e;
        req.skill_id = 5u;
        req.base_pool = pool;
        req.additional_tags = Tag::Physical | Tag::Area;
        (void)ResolveDamage(registry, req, ent);

        // 击飞 (击飞力度)
        if (auto *vel = registry.try_get<Velocity>(e)) {
          Vector2 kb = Vector2Normalize(Vector2Subtract({ep.x, ep.y}, {b.target_pos.x, b.target_pos.y}));
          vel->vx += kb.x * knockupForce;
          vel->vy += kb.y * knockupForce;
        }

        // 535 余波: 仅击晕普通怪，Boss 免疫（设计明文「受波及的普通敌人」）
        if (pts_535 > 0 && !registry.any_of<BossBattleComponent>(e)) {
          auto &fx = registry.get_or_emplace<ActiveEffectsComponent>(e);
          BuffEffect stun{
            .id = "ShockwaveStun",
            .name = "Stun",
            .type = BuffType::Stun,
            .duration = shockwaveStunDuration,
            .remaining = shockwaveStunDuration,
            .stacks = 1,
            .max_stacks = 1,
            .is_debuff = true
          };
          fx.AddOrRefresh(stun);
        }
      }
    });
  };

  for (auto entity : beam_view) {
    auto &beam = beam_view.get<BeamChannelComponent>(entity);
    const auto &pos = beam_view.get<Position>(entity);

    // 技能7 已统一收拢到 Part1 单管线，独占更新并跳过技能5 逻辑
    if (beam.skill_id == kMindBladeSkillId) {
      if (UpdateMindBladeBeam(registry, grid, entity, beam, pos, dt)) {
        s_beam_to_remove.push_back(entity);
      }
      continue;
    }

    beam.current_channel_time += dt;
    if (beam.current_channel_time >= beam.max_channel_time) {
      triggerSwordGodFinisher(entity, beam);
      s_beam_to_remove.push_back(entity);
      continue;
    }

    beam.tick_timer -= dt;
    if (beam.tick_timer <= 0.0f) {
      if (beam.skill_id == 5) {
        const auto *profile = SkillSystem::GetBakedSkillProfile(registry, entity, 5u);

        // 501 剑意共鸣: 随着引导时间增加，发射频率逐步提升 (2秒达最大)
        if (profile && (profile->delivery.feature_flags & 1) != 0) {
          float progress = std::min(1.0f, beam.current_channel_time / 2.0f);
          int pts_501 = GetSkill5Point(registry, entity, 501);
          float freq_boost = progress * (0.15f * static_cast<float>(pts_501));
          // 幂等公式: 每次从 Baker 组合基准 (sub_interval，含 570/572 频率因子)
          // 重新计算，避免相对式除法随 tick 复利缩小 interval 而突破频率上限
          const float base_interval = (profile->delivery.sub_interval > 0.0f)
                                          ? profile->delivery.sub_interval
                                          : 0.3f;
          beam.tick_interval = base_interval / (1.0f + freq_boost);
        }

        // 531 不坏剑身: 引导期间获得 15..45 护甲/持续秒数 (最高叠加 10 层)，停止引导后 2s 清空
        if (profile && (profile->delivery.feature_flags & 1024) != 0) {
          int pts_531 = GetSkill5Point(registry, entity, 531);
          if (pts_531 > 0) {
            // 每层护甲 = armor_per_sec_per_point(15.0) × 投入点数；刷新时按当前层数重写 modifiers
            const float armorPerStack =
                data::SkillMechanicsRegistry::Get().GetFloat(5u, 531, "armor_per_sec_per_point", 15.0f) *
                static_cast<float>(pts_531);
            auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(entity);
            BuffEffect *existing = effects.Get("SteeledBodyArmor");
            if (existing) {
              existing->duration = 2.0f;
              existing->remaining = 2.0f;
              if (existing->stacks < 10) {
                existing->stacks++;
              }
              existing->max_stacks = 10;
              existing->modifiers.clear();
              existing->modifiers.push_back({
                  .value = armorPerStack * static_cast<float>(existing->stacks),
                  .type = StatType::Armor,
                  .mode = ModifierMode::Flat});
            } else {
              BuffEffect armorBuff{
                  .id = "SteeledBodyArmor",
                  .name = "Steeled Body",
                  .type = BuffType::DefenseUp,
                  .duration = 2.0f,
                  .remaining = 2.0f,
                  .stacks = 1,
                  .max_stacks = 10,
                  .is_debuff = false};
              armorBuff.modifiers.push_back({
                  .value = armorPerStack,
                  .type = StatType::Armor,
                  .mode = ModifierMode::Flat});
              effects.AddOrRefresh(armorBuff);
            }
            (void)registry.get_or_emplace<StatsDirty>(entity);
          }
        }

        // 532 剑气充盈: 引导期间每秒回复 10..30 点护盾 (Ward)
        if (profile && (profile->delivery.feature_flags & 2048) != 0) {
          int pts_532 = GetSkill5Point(registry, entity, 532);
          if (pts_532 > 0) {
            if (auto *st = registry.try_get<CombatStats>(entity)) {
              const float wardPerSecPerPoint =
                  data::SkillMechanicsRegistry::Get().GetFloat(5u, 532, "ward_per_sec_per_point", 10.0f);
              const float defaultBarrierCap =
                  data::SkillMechanicsRegistry::Get().GetFloat(5u, 532, "max_barrier", 1000.0f);
              st->barrier = std::min(st->max_barrier > 0.0f ? st->max_barrier : defaultBarrierCap,
                                     st->barrier + wardPerSecPerPoint * static_cast<float>(pts_532) * beam.tick_interval);
              (void)registry.get_or_emplace<StatsDirty>(entity);
            }
          }
        }
      }

      beam.tick_timer = std::max(0.05f, beam.tick_interval);

      if (beam.skill_id == 5) {
        // 持续引导每秒消耗 20 法力 (drain)；effective_mana_cost 为每秒法耗绝对值
        //（Baker 已对技能5 写入 20.0f，并将 500 减免/510 增耗乘入该值）
        const auto *profile = SkillSystem::GetBakedSkillProfile(registry, entity, 5u);
        float mana_rate = profile ? profile->effective_mana_cost : 20.0f;
        float mana_cost = mana_rate * beam.tick_interval;
        auto *stats = registry.try_get<CombatStats>(entity);
        if (stats) {
          if (stats->mana < mana_cost) {
            // 蓝尽属引导中断：534 天剑降世按设计仅响应「结束一段 ≥2 秒的引导」，
            // 中断不触发终结技，避免法力耗尽瞬间额外召出巨剑（N12 收窄）。
            s_beam_to_remove.push_back(entity);
            continue;
          }
          stats->mana -= mana_cost;
          (void)registry.get_or_emplace<StatsDirty>(entity);
        }
      }

      Vector2 targetPos = beam.target_pos;

      // 515 万剑归阵: 位于剑阵内时，集中轰击该剑阵区域且剑阵持续时间判定暂停
      if (beam.skill_id == 5) {
        const auto *profile = SkillSystem::GetBakedSkillProfile(registry, entity, 5u);
        const bool has_515 = profile ? ((profile->delivery.feature_flags & 256) != 0)
                                     : (GetSkill5Point(registry, entity, 515) > 0);
        if (has_515) {
          auto arrayView = registry.view<SwordArrayComponent, Position>();
          for (auto arrayEnt : arrayView) {
            auto &array = arrayView.get<SwordArrayComponent>(arrayEnt);
            const auto &arrPos = arrayView.get<Position>(arrayEnt);
            if (array.owner == entity) {
              if (Vector2Distance(beam.target_pos, {arrPos.x, arrPos.y}) <= array.radius) {
                targetPos = {arrPos.x, arrPos.y};
                array.duration += beam.tick_interval; // 暂停剑阵持续时间判定
                array.total_duration += beam.tick_interval;
                if (auto *field = registry.try_get<AreaFieldComponent>(arrayEnt)) {
                  field->remaining_duration += beam.tick_interval;
                }
                break;
              }
            }
          }
        }

        // 552 随影: 剑气雨固定落在自身周围半径 circle_radius 的圆环上（严格环形，非圆盘）
        if (profile && (profile->delivery.feature_flags & 131072) != 0) {
          const float circleRadius =
              data::SkillMechanicsRegistry::Get().GetFloat(5u, 552, "circle_radius", 150.0f);
          const float angle = static_cast<float>(GetRandomValue(0, 360));
          targetPos = skills::SampleFollowingShadowRingPoint({pos.x, pos.y}, circleRadius, angle);
        }
      }

      if (beam.aim_assist) {
        // 锁敌半径基准挂 510 节点 (lock_range)，511 无处遁形按点数扩大
        float lock_radius = data::SkillMechanicsRegistry::Get().GetFloat(5u, 510, "lock_range", 450.0f);
        if (beam.skill_id == 5) {
          const auto *profile = SkillSystem::GetBakedSkillProfile(registry, entity, 5u);
          if (profile && (profile->delivery.feature_flags & 16) != 0) {
            int pts_511 = GetSkill5Point(registry, entity, 511);
            lock_radius *= (1.0f + data::SkillMechanicsRegistry::Get()
                                       .GetFloat(5u, 511, "lock_radius_pct_per_point", 0.15f) *
                                       static_cast<float>(pts_511));
          }
        }
        float bestDistSq = lock_radius * lock_radius;
        entt::entity bestTarget = entt::null;
        grid.query({targetPos.x, targetPos.y}, lock_radius,
                   [&](entt::entity e, const Position &ep) {
                     if (registry.any_of<EnemyTag>(e) && !registry.any_of<KilledTag>(e)) {
                       float distSq = Vector2DistanceSqr({targetPos.x, targetPos.y}, {ep.x, ep.y});
                       if (distSq < bestDistSq) {
                         bestDistSq = distSq;
                         bestTarget = e;
                       }
                     }
                   });
        if (registry.valid(bestTarget) && registry.all_of<Position>(bestTarget)) {
          const auto &tp = registry.get<Position>(bestTarget);
          targetPos = {tp.x, tp.y};
        }
      }

      if (beam.mode == BeamChannelMode::BarrageEmitter) {
        Vector2 dirToTarget = Vector2Normalize(Vector2Subtract(targetPos, {pos.x, pos.y}));
        const auto *profile = SkillSystem::GetBakedSkillProfile(registry, entity, beam.skill_id ? beam.skill_id : 5);

        // 533 巨剑术: 数量减半 (3 -> 1), 体积+100% (radius 35 -> giant_radius)
        bool isColossal = profile && ((profile->delivery.feature_flags & 4096) != 0);
        int count = isColossal ? 1 : (beam.is_empowered ? 4 : 3);
        float proj_radius = isColossal
                                ? data::SkillMechanicsRegistry::Get().GetFloat(5u, 533, "giant_radius", 70.0f)
                                : 35.0f;

        Tag effectiveTags = SkillSystem::GetEffectiveSkillTags(registry, entity, beam.skill_id ? beam.skill_id : 5);
        if (profile && profile->effective_tags != Tag::None) {
          effectiveTags = profile->effective_tags;
        }
        if (beam.conversion_tag != Tag::None) {
          effectiveTags = (effectiveTags & ~Tag::Physical) | beam.conversion_tag;
        }

        Color swordColor = beam.is_empowered ? GOLD : ColorAlpha(Color{0, 170, 255, 255}, 0.5f);
        if (HasTag(effectiveTags, Tag::Fire)) {
          swordColor = ORANGE;
        } else if (HasTag(effectiveTags, Tag::Cold)) {
          swordColor = SKYBLUE;
        } else if (HasTag(effectiveTags, Tag::Lightning)) {
          swordColor = PURPLE;
        }

        float bonus_crit = beam.bonus_crit_chance;
        float bonus_armor_pen = beam.bonus_armor_pen;
        float bonus_crit_dmg = profile ? profile->delivery.bonus_crit_damage : 0.0f;

        float speedMult = 1.0f;
        if (beam.skill_id == 5 && profile && (profile->delivery.feature_flags & 16) != 0) {
          int pts_511 = GetSkill5Point(registry, entity, 511);
          speedMult = 1.0f +
                      data::SkillMechanicsRegistry::Get()
                              .GetFloat(5u, 511, "fall_speed_mult_per_point", 0.25f) *
                          static_cast<float>(pts_511);
        }
        float finalSpeed = 1000.0f * speedMult;
        auto *stats = registry.try_get<CombatStats>(entity);

        for (int i = 0; i < count; ++i) {
          float spreadAmt = static_cast<float>(GetRandomValue(-20, 20)) * DEG2RAD;
          Vector2 fireDir = Vector2Rotate(dirToTarget, spreadAmt);

          auto proj_ent = registry.create();
          registry.emplace<LocalLevelTag>(proj_ent);
          registry.emplace<Position>(proj_ent, pos.x + fireDir.x * 20.0f, pos.y + fireDir.y * 20.0f);
          registry.emplace<Velocity>(proj_ent, fireDir.x * finalSpeed, fireDir.y * finalSpeed);
          registry.emplace<ColorComponent>(proj_ent, swordColor);

          auto &proj = registry.emplace<Projectile>(proj_ent);
          proj.owner = entity;
          proj.cast_id = beam.cast_id;
          proj.lifeTime = 1.0f;
          proj.radius = proj_radius;
          proj.speed = finalSpeed;
          proj.pierce = true;
          proj.max_pierce = isColossal ? 3 : 1;
          proj.visualType = 2;

          if (stats) {
            DamagePayloadContext ctx{};
            ctx.base_damage_min = stats->min_weapon_damage;
            ctx.base_damage_max = stats->max_weapon_damage;
            ctx.crit_chance = stats->crit_chance + bonus_crit;
            ctx.crit_multiplier = stats->crit_damage + bonus_crit_dmg;
            ctx.increased_damage = 0.0f;
            ctx.more_damage = 0.40f * beam.bonus_damage_mult; // 40% 基础物理伤害
            ctx.effective_tags = effectiveTags;
            ctx.source_skill_id = beam.skill_id ? beam.skill_id : 5;
            proj.payload_context = ctx;

            proj.snapshot = *stats;
            for (auto &mult : proj.snapshot.damage_multipliers) {
              mult *= (0.40f * beam.bonus_damage_mult);
            }
            proj.snapshot.crit_chance += bonus_crit;
            proj.snapshot.armor_pen += bonus_armor_pen;
            proj.snapshot.crit_damage += bonus_crit_dmg;
            registry.emplace<CombatStats>(proj_ent, proj.snapshot);
          }
          registry.emplace<SkillComponent>(proj_ent, beam.skill_id ? beam.skill_id : 5, entity);

          if (!HasTag(effectiveTags, Tag::Physical)) {
            Tag elem = Tag::None;
            if (HasTag(effectiveTags, Tag::Fire)) elem = Tag::Fire;
            else if (HasTag(effectiveTags, Tag::Cold)) elem = Tag::Cold;
            else if (HasTag(effectiveTags, Tag::Lightning)) elem = Tag::Lightning;
            if (elem != Tag::None) {
              auto &mods = registry.emplace<SkillModifierComponent>(proj_ent);
              mods.damage_modifiers.push_back(
                  DamageModifier{Tag::Physical, elem, 1.0f, ModifierType::Convert});
            }
          }

          auto &particleSys = systems::GPUParticleSystem::Get();
          components::GPUParticle p;
          p.position = {pos.x + fireDir.x * 30.0f, pos.y + fireDir.y * 30.0f};
          p.velocity = Vector2Scale(fireDir, 200.0f);
          p.color = beam.is_empowered ? GOLD : (HasTag(effectiveTags, Tag::Fire) ? ORANGE : (HasTag(effectiveTags, Tag::Cold) ? SKYBLUE : ColorAlpha(WHITE, 0.6f)));
          p.lifetime = 0.2f;
          p.maxLifetime = 0.2f;
          p.scale = isColossal ? 3.5f : 2.0f;
          p.flags = 2;
          particleSys.Emit(p);
        }
      } else if (beam.mode == BeamChannelMode::ContinuousLaser && beam.skill_id != kMindBladeSkillId) {
        auto *stats = registry.try_get<CombatStats>(entity);
        grid.query({targetPos.x, targetPos.y}, 60.0f, [&](entt::entity e, const Position &ep) {
          if (registry.any_of<EnemyTag>(e) && !registry.any_of<KilledTag>(e)) {
            DamageRequest req{};
            req.attacker = entity;
            req.defender = e;
            req.skill_id = beam.skill_id ? beam.skill_id : 7;
            req.source_entity = entity;
            req.added_effectiveness = beam.bonus_damage_mult;
            if (stats) {
              req.base_pool.Add(Tag::Physical, (stats->min_weapon_damage + stats->max_weapon_damage) * 0.5f);
            } else {
              req.base_pool.Add(Tag::Physical, 30.0f);
            }
            (void)ResolveDamage(registry, req, e);
          }
        });
      }
    }
  }

  for (auto e : s_beam_to_remove) {
    if (registry.valid(e)) {
      registry.remove<BeamChannelComponent>(e);
    }
  }

  // 1b. 技能7 772 神雷天罡遗留感电区域：脉冲并施加感电，超时销毁
  {
    static thread_local std::vector<entt::entity> s_shock_fields_to_remove;
    s_shock_fields_to_remove.clear();
    auto field_view = registry.view<ShockFieldComponent, Position>();
    for (auto fieldEnt : field_view) {
      auto &field = field_view.get<ShockFieldComponent>(fieldEnt);
      // m7：owner 失效时跳过结算并销毁区域，避免以无效实体作 attacker/source
      if (!registry.valid(field.owner)) {
        s_shock_fields_to_remove.push_back(fieldEnt);
        continue;
      }
      field.remaining -= dt;
      if (field.remaining <= 0.0f) {
        s_shock_fields_to_remove.push_back(fieldEnt);
        continue;
      }
      field.tick_timer -= dt;
      if (field.tick_timer > 0.0f) {
        continue;
      }
      field.tick_timer = std::max(0.2f, field.tick_interval);
      // 灵剑决 (372) 静电场零伤害（裁决①）：BladeFormation 不再写入 field.damage，
      // 此处对技能3 显式施加零强度感电，仅使目标进入感电状态、不产生 DoT；
      // 技能7 772 的场保持 field.damage 的 10% 感电 DoT 语义不变。
      const bool zeroDamageField = field.skill_id == 3u;
      grid.query({field.center.x, field.center.y}, field.radius,
                 [&](entt::entity e, const Position &) {
                   if (!registry.any_of<EnemyTag>(e) || registry.any_of<KilledTag>(e)) return;
                   ApplyAilmentTo(registry, e, field.owner, AilmentType::Shock,
                                  zeroDamageField ? 0.0f : field.damage * 0.1f, 1.0f, 1);
                 });
    }
    for (auto fe : s_shock_fields_to_remove) {
      if (registry.valid(fe)) {
        registry.destroy(fe);
      }
    }
  }

}

} // namespace NoMoreDay
