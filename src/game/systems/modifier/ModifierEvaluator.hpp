#pragma once

#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <unordered_set>

namespace NoMoreDay {

struct ModifierDelta {
  void AddFlat(uint32_t statType, float value);
  void AddPercentAdd(uint32_t statType, float value);
  void AddPercentMult(uint32_t statType, float value);
  void AddSkillLevel(uint32_t skillId, float value);
  void AddManaCostMultiplier(uint32_t skillId, float mult);
  void AddSkillMoreDamageMult(uint32_t skillId, float mult);
  void AddSkillCooldownFlat(uint32_t skillId, float value);
  void AddSkillCooldownMult(uint32_t skillId, float mult);
  void AddSkillCharges(uint32_t skillId, int value);
  void AddSkillBonusCrit(uint32_t skillId, float value);
  void AddSkillAreaMult(uint32_t skillId, float mult);
  void AddMonsterEventOnUpdate(uint32_t affixId);
  void AddMonsterEventOnHit(uint32_t affixId);
  void AddMonsterEventOnDeath(uint32_t affixId);
  void AddMonsterBehaviorOnUpdate(uint16_t opcode);
  void AddMonsterBehaviorOnHit(uint16_t opcode);
  void AddMonsterBehaviorOnDeath(uint16_t opcode);

  [[nodiscard]] float GetSkillLevelBonus(uint32_t skillId) const;
  [[nodiscard]] float GetManaCostMultiplier(uint32_t skillId) const;
  [[nodiscard]] float GetSkillMoreDamageMult(uint32_t skillId) const;
  [[nodiscard]] float GetSkillCooldownFlat(uint32_t skillId) const;
  [[nodiscard]] float GetSkillCooldownMult(uint32_t skillId) const;
  [[nodiscard]] int GetSkillCharges(uint32_t skillId) const;
  [[nodiscard]] float GetSkillBonusCrit(uint32_t skillId) const;
  [[nodiscard]] float GetSkillAreaMult(uint32_t skillId) const;

  // 全容器合并：加性容器累加、乘性容器累乘、怪物集合取并集。
  // 供按槽分组多次求值后合并结果使用。
  void MergeFrom(const ModifierDelta &other);

  std::unordered_map<uint32_t, float> flat;
  std::unordered_map<uint32_t, float> percent_add;
  std::unordered_map<uint32_t, float> percent_mult;
  std::unordered_map<uint32_t, float> skill_levels;
  // 技能魔耗连乘系数：key 为技能 ID，key 0 为全局通配
  std::unordered_map<uint32_t, float> mana_cost_mult;
  // 技能交付参数容器：key 均为技能 ID，缺省值按容器语义（乘性 1.0f / 加性 0.0f / 0）
  std::unordered_map<uint32_t, float> skill_more_damage_mult;
  std::unordered_map<uint32_t, float> skill_cooldown_flat;
  std::unordered_map<uint32_t, float> skill_cooldown_mult;
  std::unordered_map<uint32_t, int> skill_charges_add;
  std::unordered_map<uint32_t, float> skill_bonus_crit;
  std::unordered_map<uint32_t, float> skill_area_mult;
  std::unordered_set<uint32_t> monster_event_on_update_affix_ids;
  std::unordered_set<uint32_t> monster_event_on_hit_affix_ids;
  std::unordered_set<uint32_t> monster_event_on_death_affix_ids;
  std::unordered_set<uint16_t> monster_behavior_on_update_opcodes;
  std::unordered_set<uint16_t> monster_behavior_on_hit_opcodes;
  std::unordered_set<uint16_t> monster_behavior_on_death_opcodes;
};

// 技能交付参数算子判定（opcode 30..36）。
// 技能路径借此筛选交付记录，避免在适配器内维护会随枚举重编号而失效的区间常量。
[[nodiscard]] bool IsSkillDeliveryOpCode(ModifierOpCode opcode);

class ModifierEvaluator {
public:
  [[nodiscard]] static ModifierDelta
  Evaluate(std::span<const ModifierRecord> records, const ModifierEvalContext &ctx);

  /**
   * @brief 按 record_id 序列求值（每条记录施加一次，不做滚值覆盖）。
   *
   * categories 用于跳过无关算子组：非技能消费者可传入不含 SkillDelivery 的掩码，
   * 避免装备/天赋等记录意外施加 30..36 的技能交付算子；缺省 All 保持既有行为。
   */
  [[nodiscard]] static ModifierDelta
  Evaluate(const ModifierRuntimeRegistry &registry,
           std::span<const uint32_t> recordIds,
           const ModifierEvalContext &ctx,
           ModifierOpCategory categories = ModifierOpCategory::All);

  /**
   * @brief 按“每次出现一条求值请求”求值。
   *
   * 请求顺序即施加顺序；同一 record_id 出现多次会施加多次，各自使用其滚值。
   * categories 用于跳过无关算子组（窄入口性能优化）。
   */
  [[nodiscard]] static ModifierDelta
  Evaluate(const ModifierRuntimeRegistry &registry,
           std::span<const ModifierRecordRequest> requests,
           const ModifierEvalContext &ctx,
           ModifierOpCategory categories = ModifierOpCategory::All);

  [[nodiscard]] static float ApplyStat(float baseValue, uint32_t statType,
                                       const ModifierDelta &delta);
};

} // namespace NoMoreDay
