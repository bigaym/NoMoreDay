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
  void AddMonsterEventOnUpdate(uint32_t affixId);
  void AddMonsterEventOnHit(uint32_t affixId);
  void AddMonsterEventOnDeath(uint32_t affixId);
  void AddMonsterBehaviorOnUpdate(uint16_t opcode);
  void AddMonsterBehaviorOnHit(uint16_t opcode);
  void AddMonsterBehaviorOnDeath(uint16_t opcode);

  [[nodiscard]] float GetSkillLevelBonus(uint32_t skillId) const;
  [[nodiscard]] float GetManaCostMultiplier(uint32_t skillId) const;

  std::unordered_map<uint32_t, float> flat;
  std::unordered_map<uint32_t, float> percent_add;
  std::unordered_map<uint32_t, float> percent_mult;
  std::unordered_map<uint32_t, float> skill_levels;
  // 技能魔耗连乘系数：key 为技能 ID，key 0 为全局通配
  std::unordered_map<uint32_t, float> mana_cost_mult;
  std::unordered_set<uint32_t> monster_event_on_update_affix_ids;
  std::unordered_set<uint32_t> monster_event_on_hit_affix_ids;
  std::unordered_set<uint32_t> monster_event_on_death_affix_ids;
  std::unordered_set<uint16_t> monster_behavior_on_update_opcodes;
  std::unordered_set<uint16_t> monster_behavior_on_hit_opcodes;
  std::unordered_set<uint16_t> monster_behavior_on_death_opcodes;
};

class ModifierEvaluator {
public:
  [[nodiscard]] static ModifierDelta
  Evaluate(std::span<const ModifierRecord> records, const ModifierEvalContext &ctx);

  [[nodiscard]] static ModifierDelta
  Evaluate(const ModifierRuntimeRegistry &registry,
           std::span<const uint32_t> recordIds,
           const ModifierEvalContext &ctx);

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
