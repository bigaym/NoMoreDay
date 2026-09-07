#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
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
    del.speed = 600.0f;
    del.range = 500.0f;
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
      if (node_contract && node_contract->role == SpecNodeRole::Trigger) {
        if (out_triggers && node_contract->trigger.trigger_skill_id != 0) {
          TriggerRule rule;
          rule.rule_id = node_id;
          rule.listen_event = CombatEventType::OnSkillHit;
          rule.cast_skill_id = node_contract->trigger.trigger_skill_id;
          rule.effectiveness = node_contract->trigger.effectiveness;
          rule.internal_cooldown = node_contract->trigger.internal_cooldown;
          rule.target_mode = TriggerTargetPolicy::Victim;
          out_triggers->AddRule(rule);
        }
      }

      // B. 数值累加与形态参数写入
      ApplyNodeModifiersToProfile(skill_id, node_id, points, out_profile);
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
      out_profile.area_radius *= (1.0f + 0.1f * static_cast<float>(points));
    } else if (node_id == 210) {
      out_profile.projectile_count += points;
    } else if (node_id == 211) {
      del.sub_count = 3;      // 分裂数
      del.feature_flags |= 4; // 末端分裂
    } else if (node_id == 213) {
      del.feature_flags |= 8; // 碰撞引爆
    } else if (node_id == 230) {
      del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::BoomerangProjectile);
      del.feature_flags |= 1;
    } else if (node_id == 232) {
      del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::BoomerangProjectile);
      del.pull_radius = 120.0f; // 顶点牵引半径
      del.feature_flags |= (1 | 16);
    } else if (node_id == 233) {
      del.feature_flags |= 32; // 极点停滞 TimeLock
    } else if (node_id == 250) {
      out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | Tag::Void;
      del.feature_flags |= 64;
    } else if (node_id == 252) {
      del.feature_flags |= 128; // IntentBurst
    } else if (node_id == 253) {
      del.feature_flags |= 256; // IntentGain on hit
    } else if (node_id == 270) {
      auto conv = skills::ResolveElementalConversion(node_id, points);
      if (conv.IsActive()) {
        out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | conv.target_element;
      }
      del.feature_flags |= 512;
    }
    break;

  case 3: // 御剑术
    if (node_id == 300) {
      out_profile.projectile_count += points;
    } else if (node_id == 301) {
      del.sub_interval = 0.1f * static_cast<float>(points);
    } else if (node_id == 311) {
      out_profile.projectile_count = 8;
      out_profile.more_damage_mult *= 0.6f;
      del.feature_flags |= 1;
    } else if (node_id == 330) {
      out_profile.projectile_count = 1;
      out_profile.area_radius = 80.0f;
      out_profile.more_damage_mult *= 3.0f;
      del.feature_flags |= 2;
    } else if (node_id == 351) {
      del.feature_flags |= 4; // 剑气回流法力
    } else if (node_id == 353) {
      del.feature_flags |= 8; // 不朽准备
    } else if (node_id == 370) {
      auto conv = skills::ResolveElementalConversion(node_id, points);
      if (conv.IsActive()) {
        out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | conv.target_element;
      }
    }
    break;

  case 4: // 剑气护体
    if (node_id == 400) {
      del.feature_flags |= 1; // 金钟 减伤
    } else if (node_id == 401) {
      del.feature_flags |= 2; // 拨云 拦截提升
    } else if (node_id == 411) {
      del.feature_flags |= 4; // 五行御守
    } else if (node_id == 412) {
      del.feature_flags |= 8; // 不动如山
    } else if (node_id == 430) {
      del.feature_flags |= 16; // 剑意格挡
    } else if (node_id == 451) {
      del.sub_count = 1;
      del.feature_flags |= 32; // 瞬身反击
    } else if (node_id == 452) {
      del.feature_flags |= 64; // 灵动反击
    } else if (node_id == 470) {
      del.sub_count = 8;
      del.feature_flags |= 128; // 反制剑气
    } else if (node_id == 471) {
      del.feature_flags |= 256; // 剑气如虹
    } else if (node_id == 473) {
      del.feature_flags |= 512; // 剑刃风暴
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
      if (node_contract->trigger.trigger_skill_id != 0) {
        if (!triggers) {
          triggers = &registry.get_or_emplace<TriggerRuleComponent>(caster);
        }
        TriggerRule rule;
        rule.rule_id = node_id;
        rule.listen_event = CombatEventType::OnSkillHit;
        rule.cast_skill_id = node_contract->trigger.trigger_skill_id;
        rule.effectiveness = node_contract->trigger.effectiveness;
        rule.internal_cooldown = node_contract->trigger.internal_cooldown;
        rule.target_mode = TriggerTargetPolicy::Victim;
        triggers->AddRule(rule);
      }
    }
  }
}

} // namespace NoMoreDay
