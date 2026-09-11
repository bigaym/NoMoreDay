#pragma once
#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorBase.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay::skills {

/**
 * @brief 绝影绝剑 (ID 9) 形态行为。
 *
 * 施放后进入绝影形态，形态期间按已点专精节点产生移动/闪避/元素脉冲/死亡螺旋等
 * 持续效果；形态结束后结算爆发、重生/嗜血回复与附魔窗口。
 */
struct PhantomTrance : SkillBehaviorBase<PhantomTrance> {
  static constexpr uint32_t kSkillId = 9;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec);

  /**
   * @brief 推进形态/附魔窗口状态。
   * @return true 表示组件可被移除（SkillSystem 负责发 BuffExit 并删除组件）。
   */
  static bool Update(entt::registry &registry, entt::entity entity,
                     PhantomTranceComponent &pt, float dt);
};

} // namespace NoMoreDay::skills
