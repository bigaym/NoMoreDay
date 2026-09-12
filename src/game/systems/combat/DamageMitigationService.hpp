#pragma once

#include "game/foundation/components/Stats.hpp"
#include "game/systems/combat/EndgameModifierContract.hpp"
#include <cstdint>
#include <entt/entt.hpp>

namespace NoMoreDay {

class DamageMitigationService {
public:
  [[nodiscard]] static float Apply(
      entt::registry &registry, entt::entity attacker, entt::entity defender,
      uint32_t skill_id,
      Tag instance_tags, Tag final_type, float damage,
      const CombatStats *defender_stats,
      const systems::EndgameModifierAggregate &endgame, bool skip_mitigation,
      bool was_blocked, float block_multiplier, entt::entity source_entity,
      // 单目标快照路径传入冻结的 armor_pen；<0 表示无覆盖，回退实时查询。
      float armor_pen_override = -1.0f);

  // 减抗来源过滤 (SkillOnly scope) 聚合量：
  // ElementalErosion 等带来源技能归属 (source_skill_id != 0) 的 Flat 减抗 debuff
  // 仅对该技能的伤害生效。抗性烘焙值 (CombatStats.resistances[]) 不含 debuff 修饰符
  // (AttributePipeline 不聚合 ActiveEffects)，此处遍历 defender 的 ActiveEffects，
  // 累加对当前伤害生效的 debuff 减抗并参与结算：
  //   - source_skill_id == 0 的减抗对全体伤害生效（含 skill_id == 0 的无归属伤害）；
  //   - source_skill_id != 0 的减抗仅当 == 当前 skill_id 时生效，否则跳过。
  // 返回需加到抗性上的聚合量（可为负，表示减抗）。单实体 (Apply) 与批处理 (CalculateBatch) 共用。
  [[nodiscard]] static float ApplySkillScopedResistEffects(
      entt::registry &registry, entt::entity defender, uint32_t skill_id,
      DamageType type);
};

// Type E 抗性上限压制聚合（实现位于 DamageMitigationService.cpp）。
// 遍历 defender 的带来源技能归属减抗，返回需叠加到抗性上限压制上的聚合量；
// 单实体 (Apply) 与批处理 (CalculateBatch) 路径共用，声明集中于此，
// 避免 DamagePipeline.cpp 自行前向声明导致签名漂移。
float AggregateSkillScopedResistCapSuppression(entt::registry &registry,
                                               entt::entity defender,
                                               uint32_t skill_id,
                                               DamageType type);

} // namespace NoMoreDay
