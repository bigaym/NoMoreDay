#pragma once

#include <entt/entt.hpp>
#include <cstdint>
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"

namespace NoMoreDay {

/**
 * @brief 全量专精静态烘焙引擎 (AOT Specialization Baker)
 *
 * 在调整天赋或更换装备时，一次性计算出技能的最终形态常数与触发规则，
 * 消除运行期线性扫描与 map 查询开销。
 */
class SkillSpecializationBaker {
public:
  /**
   * @brief 全量烘焙单个技能槽位
   * @param registry ECS 注册表
   * @param caster 施法者实体
   * @param skill_id 技能 ID
   * @param spec 专精数据指针 (可为空)
   * @param out_profile 输出的烘焙属性
   * @param out_triggers 可选的触发规则组件指针
   */
  static void Bake(entt::registry &registry,
                   entt::entity caster,
                   uint32_t skill_id,
                   const SpecializedSkill *spec,
                   BakedSkillProfile &out_profile,
                   TriggerRuleComponent *out_triggers = nullptr);

  /**
   * @brief 同步实体的触发规则（将专精触发规则应用到实体）
   */
  static void SyncTriggerRules(entt::registry &registry,
                               entt::entity caster,
                               uint32_t skill_id,
                               const SpecializedSkill *spec);

private:
  static void ApplyNodeModifiersToProfile(entt::registry &registry,
                                         entt::entity caster,
                                         uint32_t skill_id,
                                         uint32_t node_id,
                                         int points,
                                         BakedSkillProfile &out_profile);
};

} // namespace NoMoreDay
