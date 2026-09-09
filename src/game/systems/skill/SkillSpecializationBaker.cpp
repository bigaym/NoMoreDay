#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/contracts/CombatEvents.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorBase.hpp"
#include <algorithm>

namespace NoMoreDay {

void SkillSpecializationBaker::Bake(
    entt::registry &registry,
    entt::entity caster,
    uint32_t skill_id,
    const SpecializedSkill *spec,
    BakedSkillProfile &out_profile,
    TriggerRuleComponent *out_triggers)
{
  const auto *skillData = SkillRegistry::Get().GetSkill(skill_id);
  if (!skillData) {
    out_profile = BakedSkillProfile{};
    out_profile.skill_id = skill_id;
    return;
  }

  // 1. 基础配置初始化
  out_profile.skill_id = skill_id;
  out_profile.effective_level = 1;
  out_profile.effective_cooldown = skillData->cooldown;
  out_profile.effective_mana_cost = skillData->mana_cost;
  out_profile.effective_tags = SkillSystem::GetEffectiveSkillTags(registry, caster, skill_id);
  out_profile.projectile_count = static_cast<int>(skillData->GetParam("projectile_count", 1.0f));
  if (out_profile.projectile_count <= 0) {
    out_profile.projectile_count = 1;
  }
  out_profile.area_radius = skillData->GetParam("area_radius", 1.0f);
  if (out_profile.area_radius <= 0.0f) {
    out_profile.area_radius = 1.0f;
  }
  out_profile.proc_coefficient = skillData->GetParam("proc_coefficient", 1.0f);
  if (out_profile.proc_coefficient <= 0.0f) {
    out_profile.proc_coefficient = 1.0f;
  }
  out_profile.more_damage_mult = 1.0f;
  out_profile.effective_charges = skillData->max_charges;
  out_profile.injected_count = 0;

  // 默认交付模式推导
  BakedDeliveryParams &del = out_profile.delivery;
  del = BakedDeliveryParams{};
  switch (skill_id) {
  case 1: // 流云刺
    del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::Mobility);
    del.secondary_archetype = static_cast<uint8_t>(DeliveryArchetype::DirectStrike);
    del.speed = 400.0f;
    del.duration = 0.375f;
    break;
  case 2: // 裂空斩
    del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::BallisticProjectile);
    // 弹速为相对倍率语义：RendingWave::DoCast 以基准弹速 300 相乘（baseSpeed = 300 × delivery.speed），
    // 1.0 即基准 300 弹速；272 雷光按 speed_bonus_pct 追加至 2.0（弹速 +100%）
    del.speed = 1.0f;
    del.range = 500.0f;
    out_profile.area_radius = 35.0f;
    break;
  case 3: // 御剑术
    del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::OrbitingSentinel);
    out_profile.projectile_count = 3;
    break;
  case 4: // 剑气护体
    del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::OrbitingSentinel);
    del.secondary_archetype = static_cast<uint8_t>(DeliveryArchetype::ReactiveWard);
    break;
  case 5: // 万剑归宗
    del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::BeamChannel);
    del.duration = 5.0f;
    del.sub_interval = 0.2f;
    break;
  case 6: // 剑阵·诛仙
    del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::AreaField);
    out_profile.area_radius = 120.0f;
    del.duration = 6.0f;
    del.sub_interval = 0.3f;
    break;
  case 7: // 心剑·无影
    del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::BeamChannel);
    del.duration = 5.0f;
    del.sub_interval = 0.12f;
    break;
  case 8: // 御剑·回旋
    del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::BoomerangProjectile);
    del.speed = 500.0f;
    del.range = 300.0f;
    del.duration = 0.3f;
    break;
  case 9: // 绝影绝剑
    del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::Mobility);
    del.secondary_archetype = static_cast<uint8_t>(DeliveryArchetype::ReactiveWard);
    del.speed = 600.0f;
    del.duration = 0.25f;
    break;
  default:
    break;
  }

  // 2. 专精树天赋烘焙 (AOT Baking)
  if (spec && spec->skill_id == skill_id) {
    out_profile.effective_level += spec->bonus_levels;

    for (const auto &[node_id, points] : spec->allocated_points) {
      if (points <= 0) {
        continue;
      }

      const auto *node_contract = SkillRegistry::Get().GetNodeContract(skill_id, node_id);

      // A. 触发契约写入实体的 TriggerRuleComponent
      // trigger_skill_id == 0 表示行为层自行处理触发：如 254 回响斩由 RendingWave::DoCast
      // 消耗剑意时直接向背后发射回响波（skill_mechanics 254 节点 echo_effectiveness/echo_icd），
      // 引擎轨道禁用以免与行为层轨道双重触发
      if (node_contract && node_contract->role == SpecNodeRole::Trigger) {
        if (out_triggers && node_contract->trigger.trigger_skill_id != 0) {
          TriggerRule rule;
          rule.rule_id = node_id;
          if (node_id == 452) {
            rule.listen_event = CombatEventType::OnDodge;
            rule.target_mode = TriggerTargetPolicy::Attacker;
          } else {
            rule.listen_event = CombatEventType::OnSkillHit;
            rule.target_mode = TriggerTargetPolicy::Victim;
          }
          rule.requires_crit = node_contract->trigger.requires_crit;
          rule.cast_skill_id = node_contract->trigger.trigger_skill_id;
          rule.effectiveness = node_contract->trigger.effectiveness;
          rule.internal_cooldown = node_contract->trigger.internal_cooldown;
          out_triggers->AddRule(rule);
        }
      }

      // B. 数值累加与形态参数写入
      ApplyNodeModifiersToProfile(skill_id, node_id, points, out_profile);
    }

    if (skill_id == 3) {
      if ((out_profile.delivery.feature_flags & 2) != 0) {
        // 330 巨剑降临：固定为 1 柄
        out_profile.projectile_count = 1;
      } else if ((out_profile.delivery.feature_flags & 1) != 0) {
        // 311 无尽剑匣：上限翻倍
        out_profile.projectile_count *= 2;
      }
    }
  }

  // 3. 装备修饰器烘焙 (Equipment Skill Modifiers)
  if (const auto *equipment = registry.try_get<EquipmentComponent>(caster)) {
    for (const auto itemEnt : equipment->slots) {
      if (!registry.valid(itemEnt) || !registry.all_of<ItemComponent>(itemEnt)) {
        continue;
      }
      const auto &item = registry.get<ItemComponent>(itemEnt);
      for (const auto &mod : item.skill_modifiers) {
        if (mod.target_skill_id != 0 && mod.target_skill_id != skill_id) {
          continue;
        }

        out_profile.effective_cooldown = std::max(0.0f, out_profile.effective_cooldown + mod.flat_cooldown_delta);
        out_profile.effective_mana_cost = std::max(0.0f, out_profile.effective_mana_cost + mod.mana_cost_delta);
        out_profile.projectile_count += mod.extra_projectiles;
        if (mod.area_radius_mult > 0.0f) {
          out_profile.area_radius *= mod.area_radius_mult;
        }

        // 动态标签转化
        if (mod.convert_from != Tag::None && mod.convert_to != Tag::None) {
          if (HasTag(out_profile.effective_tags, mod.convert_from)) {
            out_profile.effective_tags = (out_profile.effective_tags & ~mod.convert_from) | mod.convert_to;
          }
        }

        // 注入异常载荷
        if (mod.inject_ailment_id != 0 && out_profile.injected_count < BakedSkillProfile::kMaxInjectedPayloads) {
          PayloadDefinition pdef{};
          pdef.type = PayloadType::Ailment;
          pdef.ailment_id = mod.inject_ailment_id;
          pdef.value_mult = (mod.inject_ailment_chance > 0.0f) ? mod.inject_ailment_chance : 1.0f;
          pdef.damage_tags = (mod.convert_to != Tag::None) ? mod.convert_to : out_profile.effective_tags;
          out_profile.injected_payloads[out_profile.injected_count++] = pdef;
        }
      }
    }
  }
}

void SkillSpecializationBaker::ApplyNodeModifiersToProfile(
    uint32_t skill_id, uint32_t node_id, int points, BakedSkillProfile &out_profile)
{
  BakedDeliveryParams &del = out_profile.delivery;

  switch (skill_id) {
  case 1: // 流云刺
    if (node_id == 100) {
      // 迅捷之刃：攻速加成通过 stat_modifiers (t17 AttackSpeed) 由 StatsSystem 交付
    } else if (node_id == 101) {
      // 气聚：降低法力消耗 15%...45% (max 3)
      out_profile.effective_mana_cost *= std::max(0.0f, 1.0f - 0.15f * static_cast<float>(points));
    } else if (node_id == 102) {
      // 剑心洞明：基础暴击率增加 2%...10% (max 5)
      del.bonus_crit += 2.0f * static_cast<float>(points);
    } else if (node_id == 103) {
      // 流云劲：伤害提升 10%...40% (max 4)
      out_profile.more_damage_mult *= (1.0f + 0.10f * static_cast<float>(points));
    } else if (node_id == 110) {
      // 贯日：冷却时间减少 1s，伤害降低 15% (max 1)
      out_profile.effective_cooldown = std::max(0.0f, out_profile.effective_cooldown - 1.0f * static_cast<float>(points));
      out_profile.more_damage_mult *= std::max(0.0f, 1.0f - 0.15f * static_cast<float>(points));
    } else if (node_id == 111) {
      // 连环：最大充能 +1/+2，充能时间 +15% (max 2)
      out_profile.effective_charges += points;
      out_profile.effective_cooldown *= (1.0f + 0.15f * static_cast<float>(points));
    } else if (node_id == 112) {
      // 势如破竹：位移距离增加 10%...40%，每多移动 10 码 More +2% (max 4)
      // 仅标记分配（feature_flags 位 256），实际效果在运行时按真实位移结算：
      //   - 133 传送距离 ×(1 + 10%×点数)（FlowingThrust::DoCast Swap 分支）；
      //   - 命中伤害 More ×(1 + 2%×floor(实际位移/10))（下游命中结算按实际距离）。
      // 不再在烘焙期无条件修改 more_damage_mult / speed。
      del.feature_flags |= 256;
    } else if (node_id == 113) {
      // 风行者：疾风状态 (2s) 移速 +40%，无视体积碰撞，激活御剑步 (max 1)
      del.feature_flags |= 4; // 疾风 / 御剑步
    } else if (node_id == 114) {
      // 御风而行：处于御剑步期间近战暴击 +8%...24% (条件生效, 运行时检查剑步状态)
      out_profile.riding_wind_bonus_crit += 8.0f * static_cast<float>(points);
    } else if (node_id == 115) {
      // 无止境：击杀几率回复 1 充能 (max 3)
      del.feature_flags |= 16;
    } else if (node_id == 130) {
      // 留影：施放时在起点留下残影 (max 1)
      del.sub_count = 1;
      del.feature_flags |= 2;
    } else if (node_id == 131) {
      // 影域：范围提升通过 stat_modifiers (t32 AreaScale) 交付
    } else if (node_id == 132) {
      // 影之突袭：残影同步流云刺 (max 1)
      del.feature_flags |= 32;
    } else if (node_id == 133) {
      // 移形换位：流云刺变传送，起终点范围爆炸 (max 1)
      del.feature_flags |= 8;
      out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Movement) | Tag::Teleport;
    } else if (node_id == 134) {
      // 瞬狱影爆：传送爆炸伤害 +25%...75%，半径 +20%...60% (max 3)
      out_profile.more_damage_mult *= (1.0f + 0.25f * static_cast<float>(points));
      out_profile.area_radius *= (1.0f + 0.20f * static_cast<float>(points));
    } else if (node_id == 150) {
      // 要害感知：对高生命值或受控敌人暴击倍率提升 (max 4)
      del.feature_flags |= 64;
    } else if (node_id == 154) {
      // 孤注一掷：移除充能，CD 增至 8s，法力消耗翻倍，必暴，More +100% (max 1)
      out_profile.effective_charges = 1;
      out_profile.effective_cooldown = 8.0f;
      out_profile.effective_mana_cost *= 2.0f;
      out_profile.more_damage_mult *= 2.0f;
      del.bonus_crit += 100.0f;
    } else if (node_id == 155) {
      // 斩断因果：强化版击杀几率重置 CD 并回剑意 (max 3)
      del.feature_flags |= 128;
    } else if (node_id == 170 || node_id == 172) {
      // 劫火 (170) / 凛风 (172)：元素转质
      auto conv = skills::ResolveElementalConversion(node_id, points);
      if (conv.IsActive()) {
        out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | conv.target_element;
      }
    }
    break;

  case 2: // 裂空斩
    if (node_id == 200) {
      // 剑气纵横：宽度和飞行距离增加 10%...40% (max 4)
      out_profile.area_radius *= (1.0f + 0.1f * static_cast<float>(points));
      del.range *= (1.0f + 0.1f * static_cast<float>(points));
    } else if (node_id == 201) {
      // 凝神：法力消耗降低 1...4 点 (max 4)
      out_profile.effective_mana_cost = std::max(0.0f, out_profile.effective_mana_cost - 1.0f * static_cast<float>(points));
    } else if (node_id == 202) {
      // 锋芒：基础物理伤害增加 10%...50% (max 5)
      out_profile.more_damage_mult *= (1.0f + 0.10f * static_cast<float>(points));
    } else if (node_id == 203) {
      // 气劲爆发：异常状态效果提升 15%...60% (max 4)
      // 由 RendingWave 运行时按 getPts(203) 读 skill_mechanics 结算异常强度缩放，Baker 无需置位
    } else if (node_id == 210) {
      // 多重剑气：数量 +1/+2/+3，扇形发射，每发伤害降低 25%/20%/15% (max 3)
      out_profile.projectile_count += points;
      const float penalty = (points == 1) ? 0.75f : ((points == 2) ? 0.80f : 0.85f);
      out_profile.more_damage_mult *= penalty;
    } else if (node_id == 211) {
      // 碎裂之刃：命中首个敌人或最大距离分裂成 3 道较小追踪剑气 (max 1)
      del.sub_count = 3;
      del.feature_flags |= 4; // 分裂标记
    } else if (node_id == 212) {
      // 连锁反应：小剑气追踪角度强化 15%...45%，速度提升 (max 3)
      // 由 RendingWave 运行时按 getPts(212) 读 skill_mechanics 强化追踪参数，Baker 无需置位
    } else if (node_id == 213) {
      // 万剑归宗-残篇 (Keystone)：不穿透，命中直接引爆散落 8 道微型穿刺剑气 (max 1)
      del.feature_flags |= 8;
    } else if (node_id == 214) {
      // 星环护体 (Keystone)：环绕周身旋转 3s 持续切割 (max 1)
      del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::OrbitingSentinel);
      del.feature_flags |= 64;
    } else if (node_id == 215) {
      // 灵剑追击 (Synergy)：活跃灵剑决时 1 柄灵剑伴飞 (max 1)
      del.feature_flags |= 128;
    } else if (node_id == 230) {
      // 回旋劲：最大距离向施法者折返，折返伤害减少 30% (max 1)
      del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::BoomerangProjectile);
      del.feature_flags |= 1;
    } else if (node_id == 231) {
      // 重叠打击：折返击中敌人伤害 More +15%...60% (max 4)
      // 由 RendingWave 运行时按 getPts(231) 读 skill_mechanics 计入折返伤害乘区，Baker 无需置位
    } else if (node_id == 232) {
      // 引力陷阱：折返瞬间在最远端生成微型黑洞牵引 (max 1)
      del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::BoomerangProjectile);
      del.pull_radius = 120.0f;
      del.feature_flags |= (1 | 16);
    } else if (node_id == 233) {
      // 深渊边缘：黑洞牵引范围增加 20%...60%，消散时击晕 (max 3)
      del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::BoomerangProjectile);
      del.pull_radius = 120.0f * (1.0f + 0.20f * static_cast<float>(points));
      del.feature_flags |= (1 | 16 | 512);
    } else if (node_id == 234) {
      // 时空停滞 (Keystone)：最远端停滞旋转 2s 剑气风暴，不折返 (max 1)
      del.feature_flags |= 32;
    } else if (node_id == 235) {
      // 御剑引力：御剑步期间牵引半径效果 +15%...45%，折返几率护甲击碎 (max 3)
      // 由 RendingWave 运行时按 getPts(235) 在御剑步内增强牵引/折返，Baker 无需置位
    } else if (node_id == 250) {
      // 剑气烙印：命中几率施加受暴伤增加 debuff (max 4)
      // 由 RendingWave 运行时按 getPts(250) 读 skill_mechanics 施加烙印 debuff，Baker 无需置位
    } else if (node_id == 251) {
      // 剑意爆发：≥5层剑意消耗全部，每层范围+8%暴击+3% (max 1)
      del.feature_flags |= 4096;
    } else if (node_id == 252) {
      // 无底深渊：消耗剑意施放时 10%...30% 几率返还法力 (max 3)
      // 由 RendingWave 运行时按 getPts(252) 读 skill_mechanics 结算法力返还，Baker 无需置位
    } else if (node_id == 253) {
      // 湮灭波 (Keystone)：消耗满层(10)剑意巨波 More+80% 无视50%物抗 (max 1)
      del.feature_flags |= (1 << 15);
    } else if (node_id == 254) {
      // 回响斩 (Trigger)：消耗剑意时向背后触发 40% 效力裂空斩 (max 1)
      del.feature_flags |= (1 << 16);
    } else if (node_id == 255) {
      // 意念回流：消耗剑意的裂空斩每命中几率回1层剑意 (max 3)
      // 由 RendingWave 运行时按 getPts(255) 读 skill_mechanics 结算回能，Baker 无需置位
    } else if (node_id == 270) {
      // 霜寒之刃 (Transmuter)：物理转冰霜，命中寒冷，满血冻结 (max 1)
      // 元素转换写入 effective_tags（下方 ResolveElementalConversion）；冻结效果
      // 由 RendingWave 运行时按 getPts(270) 读 skill_mechanics 结算，Baker 无需置位
      auto conv = skills::ResolveElementalConversion(node_id, points);
      if (conv.IsActive()) {
        out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | conv.target_element;
      }
    } else if (node_id == 271) {
      // 冰晶碎裂：命中冻结目标冰爆溅射 (max 3)
      // 由 RendingWave 运行时按 getPts(271) 读 skill_mechanics 结算冰爆，Baker 无需置位
    } else if (node_id == 272) {
      // 雷光 (Transmuter)：物理转闪电，弹速+100%，暴击感电 (max 1)
      // 元素转换写入 effective_tags；弹速加成从 skill_mechanics 读取写入 delivery.speed
      // 相对倍率（RendingWave::DoCast 以基准弹速 300 相乘），数值不再硬编码
      auto conv = skills::ResolveElementalConversion(node_id, points);
      if (conv.IsActive()) {
        out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | conv.target_element;
      }
      const auto &mech = data::SkillMechanicsRegistry::Get();
      const float speedBonusPct = mech.GetFloat(
          skill_id, node_id, "speed_bonus_pct", 0.0f);
      del.speed = 1.0f + speedBonusPct / 100.0f;
    } else if (node_id == 273) {
      // 感电传导：命中感电目标连锁闪电 (max 3)
      // 由 RendingWave 运行时按 getPts(273) 读 skill_mechanics 结算连锁闪电，Baker 无需置位
    } else if (node_id == 274) {
      // 灵根亲和：对应元素抗性穿透增加 5%...20% (max 4)
      del.armor_pen = 5.0f * static_cast<float>(points);
      del.feature_flags |= (1 << 22);
    } else if (node_id == 275) {
      // 异常扩散：击杀异常敌人传染周围并回蓝 (max 3)
      // 由 RendingWave 运行时按 getPts(275) 读 skill_mechanics 结算传染与回蓝，Baker 无需置位
    }
    break;

  case 3: // 灵剑决
    {
      const auto &mech = data::SkillMechanicsRegistry::Get();
      if (node_id == 300) {
        // 剑池充盈：在场最大灵剑数量 +1/2/3/4 (max 4)
        out_profile.projectile_count += points;
      } else if (node_id == 301) {
        // 疾风意：自动攻击频率增加 8%...40% (max 5)
        const float haste_pct = mech.GetFloat(skill_id, node_id, "haste_pct_per_point", 8.0f);
        del.sub_interval = (haste_pct / 100.0f) * static_cast<float>(points);
      } else if (node_id == 302) {
        // 锋灵：物理伤害增加 10%...50% (max 5)
        const float phys_pct = mech.GetFloat(skill_id, node_id, "phys_damage_pct_per_point", 10.0f);
        out_profile.more_damage_mult *= (1.0f + (phys_pct / 100.0f) * static_cast<float>(points));
      } else if (node_id == 303) {
        // 五行归元：属性伤害转换效率提升 10%...40% (max 4)
        del.feature_flags |= 256;
      } else if (node_id == 310) {
        // 索敌范围：追踪半径增加 20%...60% (max 3)；基准 200 与 BladeFormation fallback 一致
        const float range_pct = mech.GetFloat(skill_id, node_id, "range_pct_per_point", 20.0f);
        del.range = 200.0f * (1.0f + (range_pct / 100.0f) * static_cast<float>(points));
      } else if (node_id == 311) {
        // 无尽剑匣：灵剑上限翻倍，单发伤害降低 40% (max 1)
        // 确定性处理：翻倍在 Bake 循环结束后统一根据 feature_flags |= 1 执行
        del.feature_flags |= 1;
      } else if (node_id == 312) {
        // 灵力网络：每柄灵剑回蓝 3...9 点/秒，维持消耗降低 5%...15% (max 3)
        const float cost_red_pct = mech.GetFloat(skill_id, node_id, "cost_reduction_pct_per_point", 5.0f);
        out_profile.effective_mana_cost *= std::max(0.0f, 1.0f - (cost_red_pct / 100.0f) * static_cast<float>(points));
        del.feature_flags |= 512;
      } else if (node_id == 313) {
        // 神速 (Keystone)：灵剑攻击频率受角色攻速 75% 加成 (max 1)
        del.feature_flags |= 16;
      } else if (node_id == 314) {
        // 集中号令：优先攻击最近一次命中目标 (max 1)
        del.feature_flags |= 32;
      } else if (node_id == 315) {
        // 御剑共振：御剑步期间频率 +20%...60%，命中几率回剑意 (max 3)
        del.feature_flags |= 64;
      } else if (node_id == 330) {
        // 巨剑降临 (Keystone)：最多 1 柄，基础伤害提升 150%，范围提升，频率降低 50%
        // projectile_count 在 post-process 置 1；伤害由 BladeFormation 设置 damage_scale=1.25f (2.5x base 0.5f)
        out_profile.area_radius = 80.0f;
        del.feature_flags |= 2;
      } else if (node_id == 331) {
        // 弱点锁定：巨剑暴击率 +5%...25% (max 5)；crit_chance 为归一化 0..1，需 /100
        const float crit_pct = mech.GetFloat(skill_id, node_id, "crit_chance_per_point", 5.0f);
        del.bonus_crit += (crit_pct / 100.0f) * static_cast<float>(points);
        del.feature_flags |= 1024;
      } else if (node_id == 332) {
        // 致命锋芒：暴伤倍率 +25%...100% (max 4)
        const float cd_pct = mech.GetFloat(skill_id, node_id, "crit_damage_per_point", 25.0f);
        del.bonus_crit_damage += (cd_pct / 100.0f) * static_cast<float>(points);
      } else if (node_id == 333) {
        // 剑压：巨剑命中使敌人受物理伤害增加 5%...15% (max 3)
        del.feature_flags |= 2048;
      } else if (node_id == 334) {
        // 碎岩：巨剑命中几率击晕 0.5s 并破甲 (max 3)
        del.feature_flags |= 4096;
      } else if (node_id == 335) {
        // 巨剑裂空 (Trigger)：暴击触发裂空斩，由契约处理
      } else if (node_id == 350) {
        // 灵剑护体：每柄灵剑提供 1%...5% 全局减伤 (max 5)
        del.feature_flags |= 8192;
      } else if (node_id == 351) {
        // 剑影环身 (Keystone)：始终环绕自身高速旋转近战 (max 1)
        del.feature_flags |= 4;
      } else if (node_id == 352) {
        // 反击剑网：每环绕一柄灵剑，格挡几率增加 2%...6% (max 3)
        del.feature_flags |= 16384;
      } else if (node_id == 353) {
        // 不灭剑魂：致命伤抵消与回血 CD 90s (max 1)
        del.feature_flags |= 8;
      } else if (node_id == 354) {
        // 法术共鸣：施法时微型剑气协同齐射 20% (max 1)
        del.feature_flags |= 32768;
      } else if (node_id == 355) {
        // 剑阵共鸣 (Synergy)：剑阵范围内攻速 +50% 并附加元素 (max 1)
        del.feature_flags |= 65536;
      } else if (node_id == 370 || node_id == 372) {
        // 地火明夷 (370) / 紫电紫雷 (372) Transmuters
        auto conv = skills::ResolveElementalConversion(node_id, points);
        if (conv.IsActive()) {
          out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | conv.target_element;
        }
      } else if (node_id == 371) {
        // 灼魂剑舞：点燃持续伤害与时间增加 (max 3)
        del.feature_flags |= (1 << 17);
      } else if (node_id == 373) {
        // 雷弧连锁：感电闪电弧跳跃 (max 3)
        del.feature_flags |= (1 << 18);
      } else if (node_id == 374) {
        // 灵剑蚀甲：TypeB 降抗，由契约处理
      } else if (node_id == 375) {
        // 灵剑充能：攻击充能双倍元素伤害与引爆 (max 3)
        del.feature_flags |= (1 << 19);
      }
    }
    break;

  case 4: // 剑气护体 (Blade Ward)
    if (node_id == 400) {
      del.feature_flags |= 1; // 金钟罩 护甲/减伤
    } else if (node_id == 401) {
      del.feature_flags |= 2; // 拨云见日 偏转提升
    } else if (node_id == 411) {
      del.feature_flags |= 4; // 五行御守 全抗
    } else if (node_id == 412) {
      del.feature_flags |= 8; // 不动如山 (Keystone)
    } else if (node_id == 430) {
      del.feature_flags |= 16; // 剑意格挡
    } else if (node_id == 451) {
      del.feature_flags |= 32; // 借力打力 (闪避提速)
    } else if (node_id == 452) {
      del.feature_flags |= 64; // 瞬身反打 (Trigger)
    } else if (node_id == 470) {
      del.sub_count = 5;       // 设计基准: 5 道反击剑气 (解 M3)
      del.feature_flags |= 128; // 剑气反震 (Keystone)
    } else if (node_id == 471) {
      del.feature_flags |= 256; // 以眼还眼 (反击增伤)
    } else if (node_id == 472 || node_id == 474) {
      // 雷霆法环 (472) / 霜铠 (474) 双 Transmuter 接线 (解 C3)
      auto conv = skills::ResolveElementalConversion(node_id, points);
      if (conv.IsActive()) {
        out_profile.effective_tags =
            (out_profile.effective_tags & ~Tag::Physical) | conv.target_element;
      }
      if (node_id == 472) {
        del.feature_flags |= 512; // 雷霆法环
      } else {
        del.feature_flags |= 1024; // 霜铠
      }
    } else if (node_id == 473) {
      del.feature_flags |= 2048; // 雷贯长虹
    }
    break;

  case 5: // 万剑归宗
    if (node_id == 512) {
      del.sub_interval = 0.1f;
      del.feature_flags |= 1; // 快速引导
    } else if (node_id == 513) {
      del.feature_flags |= 2; // 天剑降世终结技
      del.sub_count = 2;
    } else if (node_id == 530) {
      del.feature_flags |= 4; // 全屏索敌锁定
    } else if (node_id == 533) {
      del.feature_flags |= 8; // 穿心
    } else if (node_id == 551) {
      del.sub_interval = 0.1f; // 剑意爆发
      del.feature_flags |= 16;
    } else if (node_id == 552) {
      out_profile.more_damage_mult *= (1.0f + 0.05f * static_cast<float>(points));
      del.feature_flags |= 32;
      del.bonus_crit = 1.5f * static_cast<float>(points); // bonus_crit_chance
    } else if (node_id == 570) {
      auto conv = skills::ResolveElementalConversion(node_id, points);
      if (conv.IsActive()) {
        out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | conv.target_element;
      }
      del.feature_flags |= 128;
    } else if (node_id == 571) {
      del.armor_pen = static_cast<float>(points) * 6.0f; // bonus_armor_pen
      del.feature_flags |= 64; // 元素穿透
    }
    break;

  case 6: // 剑阵·诛仙
    if (node_id == 630) {
      del.feature_flags |= 1; // 缓速领域
    } else if (node_id == 631) {
      del.feature_flags |= 2; // 破甲削抗
    } else if (node_id == 633) {
      out_profile.more_damage_mult *= 1.25f; // 斩杀增伤
      del.feature_flags |= 4;
    } else if (node_id == 652) {
      del.feature_flags |= 8; // 神识归一 剑意回复
    }
    break;

  case 7: // 心剑·无影
    if (node_id == 713) {
      del.feature_flags |= 1; // 天人合一
    } else if (node_id == 730) {
      del.feature_flags |= 2; // 射线神识吸附 MindLock
    } else if (node_id == 750) {
      out_profile.more_damage_mult *= 1.25f;
      out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | Tag::Void;
      del.feature_flags |= 4;
    } else if (node_id == 752) {
      out_profile.more_damage_mult *= 1.5f;
      del.feature_flags |= 8; // 万法归一
    } else if (node_id == 770) {
      out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | Tag::Lightning;
      del.feature_flags |= 16;
    }
    break;

  case 8: // 御剑·回旋
    if (node_id == 812) {
      out_profile.more_damage_mult *= (1.0f + 0.1f * static_cast<float>(points));
      del.feature_flags |= 1;
    } else if (node_id == 813) {
      del.sub_count = 2; // 幻影回旋
      del.feature_flags |= 2;
    } else if (node_id == 830) {
      del.pull_radius = 100.0f; // 磁力牵引
      del.feature_flags |= 4;
    } else if (node_id == 831) {
      del.feature_flags |= 8; // 接剑
    } else if (node_id == 832) {
      del.pull_radius = 150.0f; // 重力场
      del.feature_flags |= 16;
    } else if (node_id == 833) {
      del.pull_radius = 200.0f; // 黑洞牵引
      del.feature_flags |= 32;
    } else if (node_id == 850) {
      del.duration = 1.5f; // 延长停滞切割时间
      del.feature_flags |= 64;
    } else if (node_id == 851) {
      del.feature_flags |= 128; // 流血
    } else if (node_id == 852) {
      del.feature_flags |= 256; // 撕裂
    } else if (node_id == 870) {
      out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | Tag::Fire;
      del.feature_flags |= 512;
    } else if (node_id == 871) {
      out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | Tag::Lightning;
      del.feature_flags |= 1024;
    }
    break;

  case 9: // 绝影绝剑
    if (node_id == 930) {
      del.feature_flags |= 1; // 影遁潜行
    } else if (node_id == 950) {
      out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | Tag::Lightning;
      del.feature_flags |= 2;
    } else if (node_id == 951) {
      del.feature_flags |= 4; // 剑流重置
    } else if (node_id == 952) {
      del.sub_count = static_cast<uint8_t>(points); // 剑意溢出
      del.feature_flags |= 8;
    } else if (node_id == 970) {
      out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | Tag::Cold;
      del.feature_flags |= 16;
    }
    break;

  default:
    break;
  }
}

void SkillSpecializationBaker::SyncTriggerRules(
    entt::registry &registry,
    entt::entity caster,
    uint32_t skill_id,
    const SpecializedSkill *spec)
{
  auto *triggers = registry.try_get<TriggerRuleComponent>(caster);

  // 遍历专精树节点，清除旧规则（若实体已具有 TriggerRuleComponent）
  const auto *tree = SkillRegistry::Get().GetSkillTree(skill_id);
  if (tree && triggers) {
    for (const auto &[node_id, _] : tree->nodes) {
      triggers->RemoveRule(node_id);
    }
  }

  if (!spec || spec->skill_id != skill_id) {
    return;
  }

  for (const auto &[node_id, points] : spec->allocated_points) {
    if (points <= 0) {
      continue;
    }
    const auto *node_contract = SkillRegistry::Get().GetNodeContract(skill_id, node_id);
    if (node_contract && node_contract->role == SpecNodeRole::Trigger) {
      // trigger_skill_id == 0（如 254 回响斩）由行为层 DoCast 直接处理，不写 TriggerRule 防双发
      if (node_contract->trigger.trigger_skill_id != 0) {
        if (!triggers) {
          triggers = &registry.get_or_emplace<TriggerRuleComponent>(caster);
        }
        TriggerRule rule;
        rule.rule_id = node_id;
        if (node_id == 452) {
          rule.listen_event = CombatEventType::OnDodge;
          rule.target_mode = TriggerTargetPolicy::Attacker;
        } else {
          rule.listen_event = CombatEventType::OnSkillHit;
          rule.target_mode = TriggerTargetPolicy::Victim;
        }
        rule.requires_crit = node_contract->trigger.requires_crit;
        rule.cast_skill_id = node_contract->trigger.trigger_skill_id;
        rule.effectiveness = node_contract->trigger.effectiveness;
        rule.internal_cooldown = node_contract->trigger.internal_cooldown;
        triggers->AddRule(rule);
      }
    }
  }
}

} // namespace NoMoreDay
