#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/contracts/CombatEvents.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorBase.hpp"
#include "game/systems/modifier/EquipmentModifierAdapter.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/SkillSpecModifierAdapter.hpp"
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
  if (skill_id == 5) {
    out_profile.effective_mana_cost = 20.0f; // 持续引导基础每秒法耗 20 点
  }
  if (skill_id == 7) {
    // 心剑·无影持续引导基础每秒法耗，数值外置于技能级键 mana_cost_per_sec
    out_profile.effective_mana_cost =
        data::SkillMechanicsRegistry::Get().GetFloat(7, 0, "mana_cost_per_sec", 15.0f);
  }
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
    del.speed = 400.0f;
    del.duration = 0.375f;
    break;
  case 2: // 裂空斩
    // 弹速为相对倍率语义：RendingWave::DoCast 以基准弹速 300 相乘（baseSpeed = 300 × delivery.speed），
    // 1.0 即基准 300 弹速；272 雷光按 speed_bonus_pct 追加至 2.0（弹速 +100%）
    del.speed = 1.0f;
    del.range = 500.0f;
    out_profile.area_radius = 35.0f;
    break;
  case 3: // 御剑术
    out_profile.projectile_count = 3;
    break;
  case 4: // 剑气护体
    // 基础交付无专属数值参数，行为层按技能语义选择交付方式
    break;
  case 5: // 万剑归宗
    // 引导上限 max_channel_time 外置于 skill_mechanics 技能5/0 节点，不再在交付层重复写入
    del.sub_interval = 0.3f;
    break;
  case 6: // 剑阵·诛仙
    out_profile.area_radius = 150.0f;
    del.duration = 5.0f;
    del.sub_interval = 0.5f;
    break;
  case 7: // 心剑·无影
    // 引导时长由行为层经 skill_mechanics 技能7/0 的 max_channel_time 读取，交付层不再写入 duration
    del.sub_interval = 0.3f;
    // 射程基准外置于技能级键 base_range，交付层缺省回退同键同默认值。
    // 703 心念映射会在该基准上按 range_pct_per_point 放大；若此处缺省，
    // 703 将回落到公式中的 200 基线，导致点满反而比 0 点射程更短。
    del.range = data::SkillMechanicsRegistry::Get().GetFloat(7, 0, "base_range", 350.0f);
    break;
  case 8: // 御剑·回旋
    // 飞行速度与最远距离以技能级 params 为唯一事实源，禁止在交付层硬编码 500/300
    del.speed = skillData->GetParam("speed", 400.0f);
    del.range = skillData->GetParam("max_distance", 300.0f);
    // 滞空时长默认为 0（未点 810 立即折返），由 810 覆写为 0.8s
    del.duration = 0.0f;
    break;
  case 9: // 绝影绝剑：突进形态
    // 突进基准速度取技能级 dash_speed（不再把 duration 当作旧版反击窗口）
    del.speed = skillData->GetParam("dash_speed", 600.0f);
    // duration 承载绝影形态时长语义，与 trance.duration_sec 同步；975 可在其上追加
    del.duration = data::SkillMechanicsRegistry::Get().GetFloat(9, 0, "form_duration", 3.0f);
    del.trance.duration_sec = del.duration;
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
          // 透传技能9 触发契约新增字段：基础概率 / 近战命中门槛 / 前置窗口
          rule.base_chance = node_contract->trigger.base_chance;
          rule.requires_melee_hit = node_contract->trigger.requires_melee_hit;
          rule.required_window = node_contract->trigger.required_window;
          out_triggers->AddRule(rule);
        }
      }

      // B. 数值累加与形态参数写入
      ApplyNodeModifiersToProfile(registry, caster, skill_id, node_id, points, out_profile);
    }

    if (skill_id == 3) {
      if ((out_profile.delivery.feature_flags & 2) != 0) {
        // 330 巨剑降临：固定为 1 柄
        out_profile.projectile_count = 1;
      } else if ((out_profile.delivery.feature_flags & 1) != 0) {
        // 311 无尽剑匣：上限翻倍
        out_profile.projectile_count *= 2;
      }
    } else if (skill_id == 6) {
      // 互斥安全守卫: 焚天烈焰阵 (670) 与 九幽雷池 (672) 互斥
      if ((out_profile.delivery.feature_flags & 262144) && (out_profile.delivery.feature_flags & 1048576)) {
        out_profile.delivery.feature_flags &= ~1048576; // 保留优先转质 670，剔除 672
        out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Lightning) | Tag::Fire;
        out_profile.delivery.sub_interval = 0.5f;
      }
    }

    // 3. UMR 技能交付参数：按节点加点线性缩放后合成到烘焙档（opcode 30..36）
    const ModifierDelta specDelta =
        SkillSpecModifierAdapter::EvaluateSkillDeliveryDeltas(
            skill_id, out_profile.effective_tags, *spec);
    out_profile.effective_mana_cost *=
        specDelta.GetManaCostMultiplier(skill_id);
    out_profile.effective_cooldown = std::max(
        0.0f, (out_profile.effective_cooldown +
               specDelta.GetSkillCooldownFlat(skill_id)) *
                  specDelta.GetSkillCooldownMult(skill_id));
    out_profile.effective_charges += specDelta.GetSkillCharges(skill_id);
    out_profile.more_damage_mult *= specDelta.GetSkillMoreDamageMult(skill_id);
    out_profile.delivery.bonus_crit += specDelta.GetSkillBonusCrit(skill_id);
    out_profile.area_radius *= specDelta.GetSkillAreaMult(skill_id);

    // 4. 154 孤注一掷终局覆盖：置于普通节点 delta 之后，使赋值式语义不受
    //    allocated_points 遍历顺序影响。
    if (skill_id == 1) {
      const auto node154 = spec->allocated_points.find(154);
      if (node154 != spec->allocated_points.end() && node154->second > 0) {
        out_profile.effective_charges = 1;
        out_profile.effective_cooldown = 8.0f;
        out_profile.effective_mana_cost *= 2.0f;
        out_profile.more_damage_mult *= 2.0f;
        out_profile.delivery.bonus_crit += 1.0f; // 必暴（归一化 1.0 = 100%）
      }
    }
  }

  // 5. 装备修饰器烘焙 (Equipment Skill Modifiers)
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

  // 装备 UMR 词缀的法力消耗乘算统一折叠于此：施法、引导每秒抽蓝、触发施法
  // 与 UI 显示均读取该结果，避免各消费点重复求值造成结算口径分裂。
  // 过滤条件沿用静态技能标签，与接入前各消费点语义保持一致。
  out_profile.effective_mana_cost *=
      EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
          registry, caster, skill_id, skillData->tags);
}

void SkillSpecializationBaker::ApplyNodeModifiersToProfile(
    entt::registry &registry,
    entt::entity caster,
    uint32_t skill_id,
    uint32_t node_id,
    int points,
    BakedSkillProfile &out_profile)
{
  BakedDeliveryParams &del = out_profile.delivery;

  switch (skill_id) {
  case 1: // 流云刺
    // 节点 101/102/103/110/111/134 的数值交付已迁移至 skill_spec UMR 交付算子
    // （见 Bake 步骤 3）；节点 154 的赋值式 Keystone 终局覆盖移交 Bake 步骤 4。
    // 以上节点在此不再产生烘焙期副作用。
    if (node_id == 100) {
      // 迅捷之刃：攻速加成通过 stat_modifiers (t17 AttackSpeed) 由 StatsSystem 交付
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
      out_profile.riding_wind_bonus_crit +=
          0.08f * static_cast<float>(points);
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
    } else if (node_id == 150) {
      // 要害感知：对高生命值或受控敌人暴击倍率提升 (max 4)
      del.feature_flags |= 64;
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
      del.feature_flags |= 64;
    } else if (node_id == 215) {
      // 灵剑追击 (Synergy)：活跃灵剑决时 1 柄灵剑伴飞 (max 1)
      del.feature_flags |= 128;
    } else if (node_id == 230) {
      // 回旋劲：最大距离向施法者折返，折返伤害减少 30% (max 1)
      del.feature_flags |= 1;
    } else if (node_id == 231) {
      // 重叠打击：折返击中敌人伤害 More +15%...60% (max 4)
      // 由 RendingWave 运行时按 getPts(231) 读 skill_mechanics 计入折返伤害乘区，Baker 无需置位
    } else if (node_id == 232) {
      // 引力陷阱：折返瞬间在最远端生成微型黑洞牵引 (max 1)
      del.pull_radius = 120.0f;
      del.feature_flags |= (1 | 16);
    } else if (node_id == 233) {
      // 深渊边缘：黑洞牵引范围增加 20%...60%，消散时击晕 (max 3)
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

  case 5: { // 万剑归宗
    const auto &mech = data::SkillMechanicsRegistry::Get();
    if (node_id == 500) { // 剑雨绵绵: 引导法耗 -10..40%
      const float red = mech.GetFloat(5, 500, "mana_reduction_pct_per_point", 0.10f) * static_cast<float>(points);
      out_profile.effective_mana_cost *= std::max(0.0f, 1.0f - red);
    } else if (node_id == 501) { // 剑意共鸣: 引导提频
      // 提频为交付层随引导时间 ramp 的动态加成 (+15%×点数 上限)，
      // Baker 侧只置标志位，不静态写入满额因子，避免与交付层 ramp 双重计入
      del.feature_flags |= 1;
    } else if (node_id == 502) { // 陨铁: 伤害+10..50% + 微小溅射
      out_profile.more_damage_mult *= (1.0f + mech.GetFloat(5, 502, "phys_damage_pct_per_point", 0.10f) * static_cast<float>(points));
      del.pull_radius = mech.GetFloat(5, 502, "splash_radius", 30.0f);
      del.feature_flags |= 2;
    } else if (node_id == 503) { // 灵动引导: 移速惩罚降低
      del.feature_flags |= 4;
    } else if (node_id == 510) { // 神识锁定: 光标锁敌 + 法耗+30%
      out_profile.effective_mana_cost *= (1.0f + mech.GetFloat(5, 510, "mana_cost_increase_pct", 0.30f));
      del.feature_flags |= 8;
    } else if (node_id == 511) { // 无处遁形: 锁定半径 + 下落加速
      del.range = mech.GetFloat(5, 510, "lock_range", 450.0f) * (1.0f + mech.GetFloat(5, 511, "lock_radius_pct_per_point", 0.15f) * static_cast<float>(points));
      del.speed = 1000.0f * (1.0f + mech.GetFloat(5, 511, "fall_speed_mult_per_point", 0.25f) * static_cast<float>(points));
      del.feature_flags |= 16;
    } else if (node_id == 512) { // 天降命印: 命印叠层 + 5层溅射
      del.feature_flags |= 32;
    } else if (node_id == 513) { // 天诛: 满层命印主剑触发
      del.feature_flags |= 64;
    } else if (node_id == 514) { // 剑刃风暴: 击杀分裂
      del.sub_count = 3;
      del.feature_flags |= 128;
    } else if (node_id == 515) { // 万剑归阵: 剑阵轰击 + 暂停
      del.feature_flags |= 256;
    } else if (node_id == 530) { // 气定神闲: 引导减伤 6..24%
      del.feature_flags |= 512;
    } else if (node_id == 531) { // 不坏剑身: 护甲叠层
      del.feature_flags |= 1024;
    } else if (node_id == 532) { // 剑气充盈: 引导回复 Ward
      del.feature_flags |= 2048;
    } else if (node_id == 533) { // 巨剑术: 数量减半、体积+100%、伤害+150%
      out_profile.more_damage_mult *= (1.0f + mech.GetFloat(5, 533, "damage_more_pct", 1.50f));
      // 取最大值而非直接赋值，避免覆盖其它来源 (如 535 余波) 设置的更大范围；
      // 巨剑半径外置: giant_radius (默认 70)，与 BeamChannelDeliverySystem 共用
      out_profile.area_radius = std::max(out_profile.area_radius, mech.GetFloat(5, 533, "giant_radius", 70.0f));
      del.feature_flags |= 4096;
    } else if (node_id == 534) { // 天剑降世: 引导>=2s 召唤 800% 范围巨剑
      del.feature_flags |= 8192;
    } else if (node_id == 535) { // 余波: 天剑范围+20..60% + 必晕
      del.feature_flags |= 16384;
    } else if (node_id == 550) { // 御剑行: 移动施法/移速-40%
      del.feature_flags |= 32768;
    } else if (node_id == 551) { // 御剑风雷: 御剑步免罚+闪避
      del.feature_flags |= 65536;
    } else if (node_id == 552) { // 随影: 圆形落剑
      del.range = mech.GetFloat(5, 552, "circle_radius", 150.0f);
      del.feature_flags |= 131072;
    } else if (node_id == 553) { // 剑意回流: 击杀/连击回剑意
      del.feature_flags |= 262144;
    } else if (node_id == 554) { // 意气爆发: 满剑意消耗->100%暴击
      del.bonus_crit =
          mech.GetFloat(5, 554, "crit_chance_bonus", 100.0f) * 0.01f;
      del.feature_flags |= 524288;
    } else if (node_id == 555) { // 意念合一: 暴伤+20..80%（分数制，直接累加到暴伤倍率）
      del.bonus_crit_damage += mech.GetFloat(5, 555, "crit_damage_pct_per_point", 0.20f) * static_cast<float>(points);
      del.feature_flags |= 1048576;
    } else if (node_id == 570) { // 天火流星: 火焰转质 / 低频高伤
      auto conv = skills::ResolveElementalConversion(node_id, points);
      if (conv.IsActive()) {
        out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | conv.target_element;
      }
      out_profile.more_damage_mult *= mech.GetFloat(5, 570, "damage_more_mult", 2.0f);
      // 570 低频: 频率大幅下降 (间隔除以 (1-惩罚比), 与 501 提频叠乘共存)
      del.sub_interval /= (1.0f - mech.GetFloat(5, 570, "frequency_penalty_pct", 0.60f));
      del.feature_flags |= 2097152;
    } else if (node_id == 571) { // 末日余烬: 燃烧地表 DoT
      del.feature_flags |= 4194304;
    } else if (node_id == 572) { // 凛冬暴雪: 冰霜转质 / 高频寒冷
      auto conv = skills::ResolveElementalConversion(node_id, points);
      if (conv.IsActive()) {
        out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | conv.target_element;
      }
      // 572 高频: 频率大幅提升 (间隔除以 (1+加成比), 与 501 提频叠乘共存)
      del.sub_interval /= (1.0f + mech.GetFloat(5, 572, "frequency_bonus_pct", 0.50f));
      del.feature_flags |= 8388608;
    } else if (node_id == 573) { // 绝对零度: 冻结延长+击碎增伤
      del.feature_flags |= 16777216;
    } else if (node_id == 574) { // 灵根感应: Type D 属性转穿透 (Int -> 穿透, 上限 30%)
      float intel = 0.0f;
      if (registry.valid(caster)) {
        if (const auto *ps = registry.try_get<PrimaryStats>(caster)) {
          intel = ps->intelligence;
        }
      }
      // 分段穿透系数外置 (数据代理向 skill_mechanics.json 技能5/574 补充同名键, 现值作默认)
      float pen_per_int = (points >= 4) ? mech.GetFloat(5, 574, "pen_ratio_per_point_tier4", 1.0f / 12.0f)
                                        : ((points >= 3) ? mech.GetFloat(5, 574, "pen_ratio_per_point_tier3", 1.0f / 18.0f)
                                                         : ((points >= 2) ? mech.GetFloat(5, 574, "pen_ratio_per_point_tier2", 1.0f / 24.0f)
                                                                          : mech.GetFloat(5, 574, "pen_ratio_per_point_tier1", 1.0f / 30.0f)));
      float pen = intel * pen_per_int;
      if (pen <= 0.0f) {
        // 无智力时以基准穿透兜底
        pen = static_cast<float>(points) * mech.GetFloat(5, 574, "base_pen_per_point", 6.0f);
      }
      del.armor_pen = std::min(mech.GetFloat(5, 574, "max_penetration", 30.0f), pen);
      del.feature_flags |= 33554432;
    } else if (node_id == 575) { // 天灾: 异常几率 +30..90%
      del.feature_flags |= 67108864;
    }
    break;
  }

  case 6: { // 剑阵·诛仙
    const auto &mech = data::SkillMechanicsRegistry::Get();
    // 基础核心 (Base Tier)
    if (node_id == 600) { // 灵气流转: 持续时间 +0.5s/点
      del.duration += mech.GetFloat(6, 600, "duration_per_point", 0.5f) * static_cast<float>(points);
    } else if (node_id == 601) { // 虚空法网: 基础半径增加 15%..60%
      out_profile.area_radius *= (1.0f + mech.GetFloat(6, 601, "radius_pct_per_point", 0.15f) * static_cast<float>(points));
    } else if (node_id == 602) { // 极刑: 阵内物理伤害增加 10%..50%
      out_profile.more_damage_mult *= (1.0f + mech.GetFloat(6, 602, "phys_damage_pct_per_point", 0.10f) * static_cast<float>(points));
    } else if (node_id == 603) { // 阵基稳固: 法力消耗降低 5%..20%，施法范围 +10%..40%
      out_profile.effective_mana_cost *= (1.0f - mech.GetFloat(6, 603, "mana_reduction_pct_per_point", 0.05f) * static_cast<float>(points));
      del.range = (del.range > 0.0f ? del.range : 400.0f) * (1.0f + mech.GetFloat(6, 603, "cast_range_pct_per_point", 0.10f) * static_cast<float>(points));
    }
    // 分支 A: 多阵联动与共鸣 (Link & Multi-Array)
    else if (node_id == 610) { // 双生剑阵: 数量上限 +1，单个伤害 -15%
      out_profile.more_damage_mult *= (1.0f - mech.GetFloat(6, 610, "damage_reduction_pct", 0.15f));
      del.feature_flags |= 16;
    } else if (node_id == 611) { // 三才阵: 数量上限再 +1，法力消耗 +30%
      out_profile.effective_mana_cost *= (1.0f + mech.GetFloat(6, 611, "mana_increase_pct", 0.30f));
      del.feature_flags |= 32;
    } else if (node_id == 612) { // 剑气共鸣: 重叠 More 20%..60%
      del.feature_flags |= 64;
    } else if (node_id == 613) { // 千丝万缕 (Keystone): 能量连线
      del.feature_flags |= 128;
    } else if (node_id == 614) { // 流云穿阵 (Synergy): 位移引爆 150%
      del.feature_flags |= 256;
    } else if (node_id == 615) { // 御剑阵威: 御剑步状态频率 +20%..60%
      del.feature_flags |= 512;
    }
    // 分支 B: 禁魔领域与禁锢 (Debuff & Execution)
    else if (node_id == 630) { // 迟缓剑压: 减速 10%..40%
      del.feature_flags |= 1;
    } else if (node_id == 631) { // 破甲剑意: 每次判定 50%..200% 施加护甲击碎
      del.feature_flags |= 2;
    } else if (node_id == 632) { // 虚弱领域: 敌人伤害 Less 6%..18%
      del.feature_flags |= 1024;
    } else if (node_id == 633) { // 绝命法场 (Keystone): 处决 <12% 非Boss敌人，对Boss伤害 More 20%
      del.feature_flags |= 4;
    } else if (node_id == 634) { // 剑阵牢笼 (Keystone): 实体剑墙，半径固定缩小 30%
      out_profile.area_radius *= (1.0f - mech.GetFloat(6, 634, "radius_penalty_pct", 0.30f));
      del.feature_flags |= 2048;
    } else if (node_id == 635) { // 阵斩回响 (Trigger): 处决触发裂空斩
      del.feature_flags |= 4096;
    }
    // 分支 C: 阵眼核心与自身增幅 (Buff & Focus)
    else if (node_id == 650) { // 阵眼: 站在阵内全局伤害 +15%..60%
      del.feature_flags |= 8192;
    } else if (node_id == 651) { // 灵力泉涌: 处于阵内每秒回蓝 2..8
      del.feature_flags |= 16384;
    } else if (node_id == 652) { // 意念合一: 阵内每秒 33%..100% 几率自然生成 1 层剑意
      del.feature_flags |= 8;
    } else if (node_id == 653) { // 随身剑垒 (Keystone): 随身光环，伤害降低 50%
      out_profile.more_damage_mult *= (1.0f - mech.GetFloat(6, 653, "damage_reduction_pct", 0.50f));
      del.feature_flags |= 32768;
    } else if (node_id == 654) { // 剑神领域: 光环覆盖期间 CDR +10%..30%
      del.feature_flags |= 65536;
    } else if (node_id == 655) { // 法阵回护: 阵内每秒智力 50%..150% 护盾
      del.feature_flags |= 131072;
    }
    // 分支 D: 灵根元素阵地 (Elemental Zone)
    else if (node_id == 670) { // 焚天烈焰阵 (Transmuter): 转火 [Fire]，熔岩必附烧灼
      out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | Tag::Fire;
      del.feature_flags |= 262144;
    } else if (node_id == 671) { // 炼狱余火: 点燃伤害 +20%..60%，离阵燃烧 3s
      del.feature_flags |= 524288;
    } else if (node_id == 672) { // 九幽雷池 (Transmuter): 转雷 [Lightning]，每 1s 随机落雷
      out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | Tag::Lightning;
      del.sub_interval = mech.GetFloat(6, 672, "lightning_interval", 1.0f);
      del.feature_flags |= 1048576;
    } else if (node_id == 673) { // 连珠落雷: 目标 +1..3，电弧
      del.feature_flags |= 2097152;
    } else if (node_id == 674) { // 法阵侵蚀: 每秒 1..4 层降抗 (TypeB)
      del.feature_flags |= 4194304;
    } else if (node_id == 675) { // 移形换阵: 重按挪阵
      del.feature_flags |= 8388608;
    }
    break;
  }

  case 7: { // 心剑·无影
    const auto &mech = data::SkillMechanicsRegistry::Get();
    if (node_id == 700) { // 神识凝聚: 引导法耗 -10..40%
      const float red =
          mech.GetFloat(7, 700, "mana_reduction_pct_per_point", 0.10f) * static_cast<float>(points);
      out_profile.effective_mana_cost *= std::max(0.0f, 1.0f - red);
    } else if (node_id == 701) { // 无影无形: 基础物理伤害 +10..50%
      out_profile.more_damage_mult *=
          (1.0f + mech.GetFloat(7, 701, "phys_damage_pct_per_point", 0.10f) * static_cast<float>(points));
    } else if (node_id == 702) { // 裂空: 基础范围半径 +10..40%
      // 显式以技能级 base_radius 为基准重算，保证不依赖技能数据默认值且幂等
      out_profile.area_radius =
          mech.GetFloat(7, 0, "base_radius", 60.0f) *
          (1.0f + mech.GetFloat(7, 702, "radius_pct_per_point", 0.10f) * static_cast<float>(points));
    } else if (node_id == 703) { // 心念映射: 视野/追踪范围 +10..40%
      del.range = (del.range > 0.0f ? del.range : 200.0f) *
                  (1.0f + mech.GetFloat(7, 703, "range_pct_per_point", 0.10f) * static_cast<float>(points));
    } else if (node_id == 710) { // 心流叠加: 叠层增伤由交付层按层数动态读取
      del.feature_flags |= 1;
    } else if (node_id == 711) { // 碎空爆 (Keystone): 蓄力引爆
      del.feature_flags |= 2;
    } else if (node_id == 712) { // 虚无牵引: 蓄力拖拽
      del.feature_flags |= 4;
    } else if (node_id == 713) { // 精神透支: 蓄力加速 + 满层自伤
      del.feature_flags |= 8;
    } else if (node_id == 715) { // 破绽洞察: 中心暴击伤害
      del.feature_flags |= 16;
    } else if (node_id == 730) { // 神识多开: 额外追踪撕裂
      del.feature_flags |= 32;
    } else if (node_id == 731) { // 千面阵: 额外追踪撕裂数量
      del.feature_flags |= 64;
    } else if (node_id == 732) { // 步影随行 (Keystone): 微步移动 + 引导法耗提升
      out_profile.effective_mana_cost *= (1.0f + mech.GetFloat(7, 732, "mana_penalty_pct", 0.50f));
      del.feature_flags |= 128;
    } else if (node_id == 733) { // 御剑神游: 小撕裂半径与伤害
      del.feature_flags |= 256;
    } else if (node_id == 734) { // 神游脱战: 瞬移打断
      del.feature_flags |= 512;
    } else if (node_id == 735) { // 精准切割: 孤立目标增伤
      del.feature_flags |= 1024;
    } else if (node_id == 750) { // 引力坍缩: 概率微牵引
      del.feature_flags |= 2048;
    } else if (node_id == 751) { // 空间粉碎: 护甲击碎
      del.feature_flags |= 4096;
    } else if (node_id == 752) { // 深渊侵蚀: 流血与爆发加速
      del.feature_flags |= 8192;
    } else if (node_id == 753) { // 意念风暴: 消耗剑意增伤
      del.feature_flags |= 16384;
    } else if (node_id == 754) { // 剑意化无 (Synergy): 飞剑瞬移穿刺
      del.feature_flags |= 32768;
    } else if (node_id == 755) { // 心念反哺: 击杀回蓝 / 精英回剑意
      del.feature_flags |= 65536;
    } else if (node_id == 770) { // 天外冰晶 (Transmuter): 物理转冰霜
      out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | Tag::Cold;
      del.feature_flags |= 131072;
    } else if (node_id == 771) { // 极寒碎骨: 击碎增伤
      del.feature_flags |= 262144;
    } else if (node_id == 772) { // 神雷天罡 (Transmuter): 物理转闪电 + 落雷间隔
      out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | Tag::Lightning;
      // 落雷间隔由交付层直接读取 mechanics(7,772,"pillar_interval")；
      // delivery.sub_interval 只有技能5的引导频率组合会消费，对技能7无消费方，故不再写入。
      del.feature_flags |= 524288;
    } else if (node_id == 773) { // 天劫落雷: 主目标增伤与感电暴击
      del.feature_flags |= 1048576;
    } else if (node_id == 774) { // 异常切割: 异常目标增伤
      del.feature_flags |= 2097152;
    } else if (node_id == 775) { // 心念灭抗: 抗性上限压制
      del.feature_flags |= 4194304;
    }
    break;
  }

  case 8: { // 御剑·回旋 — 语义以设计 §3.8 为准，旧版 812/813/830-833 映射已废弃
    const auto &mech8 = data::SkillMechanicsRegistry::Get();
    if (node_id == 800) { // 轻巧: 基础法力消耗 -1/点，攻速 +4%/点由 stat_modifiers 承担
      out_profile.effective_mana_cost =
          std::max(0.0f, out_profile.effective_mana_cost - 1.0f * static_cast<float>(points));
      del.feature_flags |= 1u;
    } else if (node_id == 801) { // 疾速: 飞行速度与最远距离同比例 +15%/点
      const float mult = 1.0f + 0.15f * static_cast<float>(points);
      del.speed *= mult;
      del.range *= mult;
      del.feature_flags |= 2u;
    } else if (node_id == 802) { // 锋锐: 附加物理点伤与暴击率由 stat_modifiers 承担
      del.feature_flags |= 4u;
    } else if (node_id == 803) { // 回力感应: 折返伤害 +10%/点
      del.return_damage_mult = 1.0f + 0.10f * static_cast<float>(points);
      del.feature_flags |= 8u;
    } else if (node_id == 810) { // 滞空切割: 顶点滞留时长
      del.duration = mech8.GetFloat(8, 810, "hover_duration", 0.8f);
      del.feature_flags |= 16u;
    } else if (node_id == 811) { // 放血: 命中施加一层流血的概率 +25%/点
      del.bleed_chance = 0.25f * static_cast<float>(points);
      del.feature_flags |= 32u;
    } else if (node_id == 812) { // 撕裂伤口: 对流血目标额外暴击倍率 +15%/点
      del.crit_mult_vs_bleeding = 0.15f * static_cast<float>(points);
      del.feature_flags |= 64u;
    } else if (node_id == 813) { // 剑鸣: 护甲击碎概率/延长由行为层按 mechanics(8,813) 读取
      del.feature_flags |= 128u;
    } else if (node_id == 814) { // 拔血流云: Synergy，由 ProcEngine(WS-D) 消费
      del.feature_flags |= 256u;
    } else if (node_id == 815) { // 风眼: 本次飞行流血总伤转治疗
      del.heal_bleed_pct = 1.0f;
      del.feature_flags |= 512u;
    } else if (node_id == 830) { // 幻影回旋: 额外 2 柄侧翼虚影剑
      del.sub_count = 2;
      del.feature_flags |= 1024u;
    } else if (node_id == 831) { // 无尽刃舞: 侧翼扇形角 -10%/点，穿透不衰减
      del.side_angle_mult = 1.0f - 0.10f * static_cast<float>(points);
      del.feature_flags |= 2048u;
    } else if (node_id == 832) { // 接剑: 接刃回蓝 2/点
      del.catch_mana = 2.0f * static_cast<float>(points);
      del.feature_flags |= 4096u;
    } else if (node_id == 833) { // 连环劲: 接刃后下次施放攻速 +15%/点
      // BuffEffect 的 PercentAdd 以百分点为单位，与 stat_modifiers 口径一致
      del.combo_attack_speed = 15.0f * static_cast<float>(points);
      del.feature_flags |= 8192u;
    } else if (node_id == 834) { // 御剑接踵: 御剑步延长 0.5s/点
      del.step_extend_sec = 0.5f * static_cast<float>(points);
      del.feature_flags |= 16384u;
    } else if (node_id == 835) { // 回旋游步: 无直接参数，标记供接刃层消费
      del.feature_flags |= 32768u;
    } else if (node_id == 850) { // 磁力场: 折返牵引，参数从技能 params 读取
      del.feature_flags |= 65536u;
    } else if (node_id == 851) { // 重力网: 牵引范围与判定体积 +15%/点
      del.pull_radius_mult = 1.0f + 0.15f * static_cast<float>(points);
      del.feature_flags |= 131072u;
    } else if (node_id == 852) { // 意随剑舞: 折返命中获得剑意概率 +15%/点
      del.intent_gain_chance = 0.15f * static_cast<float>(points);
      del.feature_flags |= 262144u;
    } else if (node_id == 853) { // 心剑合一: 每层剑意速度/命中盒 +2%/点（供交付层读取）
      del.intent_scaling = 0.02f * static_cast<float>(points);
      del.feature_flags |= 524288u;
    } else if (node_id == 854) { // 巨阙: 禁用侧刃，总护甲 5% 转基础物理，命中硬直
      del.sub_count = 0;
      del.giant_armor_scale = 0.05f;
      del.feature_flags |= 1048576u;
    } else if (node_id == 870) { // 劫灰路径: 物理转火焰
      out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | Tag::Fire;
      del.feature_flags |= 2097152u;
    } else if (node_id == 871) { // 燎原之势: 燃烧路径受击增伤 +15%/点
      del.path_amp = 0.15f * static_cast<float>(points);
      del.feature_flags |= 4194304u;
    } else if (node_id == 872) { // 电磁回旋: 物理转闪电
      out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | Tag::Lightning;
      del.feature_flags |= 8388608u;
    } else if (node_id == 873) { // 高压电弧: 电弧频率 +30%/点
      del.arc_freq_mult = 1.0f + 0.30f * static_cast<float>(points);
      del.feature_flags |= 16777216u;
    } else if (node_id == 874) { // 元素尾迹: 路径宽度与持续时间 +15%/点
      del.path_width_mult = 1.0f + 0.15f * static_cast<float>(points);
      del.path_duration_mult = del.path_width_mult;
      del.feature_flags |= 33554432u;
    } else if (node_id == 875) { // 灵根破壁: 本技能元素穿透 +6%/点
      del.path_pen = 0.06f * static_cast<float>(points);
      del.feature_flags |= 67108864u;
    } else if (node_id == 876) { // 元素护体: 元素路径内绝对减伤
      del.element_shield_pct = 0.15f;
      del.feature_flags |= 134217728u;
    }
    break;
  }

  case 9: { // 绝影绝剑：形态参数统一写入 del.trance（与 PhantomTrance 行为单源）
    const auto &mech9 = data::SkillMechanicsRegistry::Get();
    if (node_id == 902) { // 身轻如燕: 形态内移速/闪避
      del.trance.move_speed_pct +=
          mech9.GetFloat(9, 902, "move_per_point", 10.0f) * static_cast<float>(points);
      del.trance.dodge_pct +=
          mech9.GetFloat(9, 902, "dodge_per_point", 10.0f) * static_cast<float>(points);
      del.feature_flags |= 1u << 2;
    } else if (node_id == 913) { // 灵流穿透: 穿行敌人获虚弱
      del.trance.weaken_on_pass_pct =
          mech9.GetFloat(9, 913, "weaken_per_point", 0.06f) * static_cast<float>(points);
      del.feature_flags |= 1u << 8;
    } else if (node_id == 914) { // 虚境馈赠: 形态内每秒回蓝/全局减伤
      del.trance.void_gift_mana_per_sec =
          mech9.GetFloat(9, 914, "mana_per_point", 2.0f) * static_cast<float>(points);
      del.trance.void_gift_dr_pct =
          mech9.GetFloat(9, 914, "dr_per_point", 0.10f) * static_cast<float>(points);
      del.feature_flags |= 1u << 9;
    } else if (node_id == 934) { // 剑随心动: 攻速/施法速度
      del.trance.atk_cast_speed_pct =
          mech9.GetFloat(9, 934, "speed_per_point", 5.0f) * static_cast<float>(points);
      del.feature_flags |= 1u << 14;
    } else if (node_id == 935) {
      // 逆命反噬: 触发规则由 TriggerRuleComponent 承载（见 SyncTriggerRules），
      // 形态侧无独立参数，仅登记标记位
      del.feature_flags |= 1u << 28;
    } else if (node_id == 954) { // 时光逆流: 结束返还其他技能冷却
      del.trance.time_reversal_sec = mech9.GetFloat(9, 954, "refund_sec", 3.0f);
      del.feature_flags |= 1u << 19;
    } else if (node_id == 955) { // 全神贯注: 形态内其他技能法耗降低
      del.trance.focus_mana_reduce_pct =
          mech9.GetFloat(9, 955, "cost_per_point", 0.20f) * static_cast<float>(points);
      del.feature_flags |= 1u << 20;
    } else if (node_id == 972) { // 疾空惊雷: 转闪电（989 已选 Cold 时不覆盖，Cold 优先）
      if (del.trance.transmuter_tag != Tag::Cold) {
        del.trance.transmuter_tag = Tag::Lightning;
      }
      del.feature_flags |= 1u << 23;
    } else if (node_id == 973) { // 过载护盾: 雷盾期间移速/攻速
      del.trance.overload_speed_pct =
          mech9.GetFloat(9, 973, "speed_per_point", 0.10f) * static_cast<float>(points);
      del.feature_flags |= 1u << 24;
    } else if (node_id == 974) { // 空明心境: 形态内冷却缩减
      del.trance.recovery_pct =
          mech9.GetFloat(9, 974, "recovery_per_point", 10.0f) * static_cast<float>(points);
      del.feature_flags |= 1u << 3;
    } else if (node_id == 975) { // 延命: 延长绝影形态时长
      del.trance.duration_sec =
          mech9.GetFloat(9, 0, "form_duration", 3.0f) +
          mech9.GetFloat(9, 975, "duration_per_point", 0.25f) * static_cast<float>(points);
      del.feature_flags |= 1u << 0;
    } else if (node_id == 976) { // 气旋爆发: 结束爆发伤害/半径倍率
      del.trance.burst_damage_mult =
          1.0f + mech9.GetFloat(9, 976, "burst_per_point", 0.15f) * static_cast<float>(points);
      del.feature_flags |= 1u << 1;
    } else if (node_id == 977) { // 向死而生: 免死不提前结束
      del.trance.cheat_death_hold = true;
      del.feature_flags |= 1u << 4;
    } else if (node_id == 978) { // 浴血重生: 免死触发/未触发时回复
      del.trance.rebirth_lost_pct =
          mech9.GetFloat(9, 978, "rebirth_lost_per_point", 0.15f) * static_cast<float>(points);
      del.trance.rebirth_flat_pct =
          mech9.GetFloat(9, 978, "rebirth_flat_per_point", 0.05f) * static_cast<float>(points);
      del.feature_flags |= 1u << 5;
    } else if (node_id == 979) { // 绝影护甲: 施放时获得上限比例护盾
      del.trance.ward_pct =
          mech9.GetFloat(9, 979, "ward_per_point", 0.10f) * static_cast<float>(points);
      del.feature_flags |= 1u << 6;
    } else if (node_id == 980) { // 虚灵之躯: 潜行/穿行/不可选中
      del.trance.void_body = true;
      del.feature_flags |= 1u << 7;
    } else if (node_id == 981) { // 逆脉: 锁血禁疗增伤
      del.trance.death_seal = true;
      del.feature_flags |= 1u << 10;
    } else if (node_id == 982) { // 孤注一掷: 每缺失 1% 生命的暴伤加成（百分点/点，rank4=4%）
      del.trance.last_stand_crit_pct =
          mech9.GetFloat(9, 982, "crit_per_missing_per_point", 1.0f) *
          static_cast<float>(points);
      del.feature_flags |= 1u << 11;
    } else if (node_id == 983) { // 死亡螺旋: 每轮飞剑数与单柄伤害
      del.trance.death_spiral_count = static_cast<int>(
          mech9.GetFloat(9, 983, "spiral_count_per_point", 1.0f) * static_cast<float>(points));
      del.trance.death_spiral_damage_pct =
          mech9.GetFloat(9, 983, "spiral_damage_pct", 0.08f);
      del.feature_flags |= 1u << 12;
    } else if (node_id == 984) { // 嗜血本能: 结束时按期间伤害回复
      del.trance.bloodthirst_pct = mech9.GetFloat(9, 984, "heal_pct", 0.05f);
      del.feature_flags |= 1u << 13;
    } else if (node_id == 985) { // 破空一闪: 瞬移至光标再入形态
      del.trance.blink = true;
      del.feature_flags |= 1u << 15;
    } else if (node_id == 986) { // 缩地成寸: 基础冷却直接减免
      del.trance.cooldown_flat_reduce =
          mech9.GetFloat(9, 986, "cd_per_point", 1.0f) * static_cast<float>(points);
      out_profile.effective_cooldown = std::max(
          1.0f, out_profile.effective_cooldown - del.trance.cooldown_flat_reduce);
      del.feature_flags |= 1u << 16;
    } else if (node_id == 987) { // 意随神行: 每秒剑意
      del.trance.intent_per_sec = static_cast<int>(
          mech9.GetFloat(9, 987, "intent_per_point", 1.0f) * static_cast<float>(points));
      del.feature_flags |= 1u << 17;
    } else if (node_id == 988) { // 御剑化影: 御剑步时形态闪避/连击点流失倍率
      del.trance.sword_step_dodge_pct =
          mech9.GetFloat(9, 988, "dodge_per_point", 10.0f) * static_cast<float>(points);
      del.trance.sword_step_drain_mult = mech9.GetFloat(9, 988, "drain_mult", 0.5f);
      del.feature_flags |= 1u << 18;
    } else if (node_id == 989) { // 天山雪隐: 转冰霜（与 972 互斥，Cold 优先）
      del.trance.transmuter_tag = Tag::Cold;
      del.feature_flags |= 1u << 21;
    } else if (node_id == 990) { // 凛冬附魔: 对冰冻/冰缓目标增伤
      del.trance.frost_amp_pct =
          mech9.GetFloat(9, 990, "amp_per_point", 0.15f) * static_cast<float>(points);
      del.feature_flags |= 1u << 22;
    } else if (node_id == 991) { // 意念穿透: 每层剑意元素穿透（存小数）
      del.trance.enchant_pen_per_intent_pct =
          mech9.GetFloat(9, 991, "pen_per_intent_per_point", 1.0f) *
          static_cast<float>(points) * 0.01f;
      del.trance.enchant_pen_cap_pct = mech9.GetFloat(9, 991, "pen_cap_pct", 40.0f);
      del.feature_flags |= 1u << 25;
    } else if (node_id == 992) { // 灵气反哺: 对应异常击杀刷新附魔
      del.trance.enchant_refresh_on_kill = true;
      del.feature_flags |= 1u << 26;
    } else if (node_id == 993) { // 影剑回响: 触发器由行为层手写规则，仅置协同标记
      del.trance.echo_synergy = true;
      del.feature_flags |= 1u << 27;
    }
    break;
  }

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
        } else if (skill_id == 7 && node_id == 714) {
          // 寂灭: 被 711 碎空爆直接击杀的敌人在死亡位置触发小型万剑归宗
          rule.listen_event = CombatEventType::OnKill;
          rule.target_mode = TriggerTargetPolicy::Victim;
          // 仅接受技能7造成的击杀，避免其他技能/召唤物/持续伤害击杀误触发。
          // 设计语义为「被 711 碎空爆直接击杀」，但 CombatEvent 只携带来源 skill_id，
          // 没有逐次命中的节点来源，运行时无法区分引爆本体与 711 蓄力期间其他
          // 技能7伤害（如 754 穿透）；且 711 本就是 714 的前置节点，再按 711 门控
          // 不会新增信息。精确判定须先为事件补充命中节点来源后再实施。
          rule.required_skill_id = 7;
        } else if (skill_id == 8 && node_id == 855) {
          // 巨剑共鸣: 仅技能8巨剑投掷暴击可触发（required_skill_id=8），
          // 并需施法者已在技能3(灵剑决)点出 330 巨剑降临
          rule.listen_event = CombatEventType::OnSkillHit;
          rule.target_mode = TriggerTargetPolicy::Victim;
          rule.required_skill_id = 8;
          rule.required_source_node_id = 330;
          rule.required_source_skill_id = 3;
        } else {
          rule.listen_event = CombatEventType::OnSkillHit;
          rule.target_mode = TriggerTargetPolicy::Victim;
        }
        rule.requires_crit = node_contract->trigger.requires_crit;
        rule.cast_skill_id = node_contract->trigger.trigger_skill_id;
        rule.effectiveness = node_contract->trigger.effectiveness;
        rule.internal_cooldown = node_contract->trigger.internal_cooldown;
        // 透传技能9 触发契约新增字段：基础概率 / 近战命中门槛 / 前置窗口
        // 例如 935 逆命反噬: base_chance=0.2, requires_melee_hit=true, required_window=DeathSeal
        rule.base_chance = node_contract->trigger.base_chance;
        rule.requires_melee_hit = node_contract->trigger.requires_melee_hit;
        rule.required_window = node_contract->trigger.required_window;
        triggers->AddRule(rule);
      }
    }
  }
}

} // namespace NoMoreDay
