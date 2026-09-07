#pragma once

#include <entt/entt.hpp>
#include <vector>

namespace NoMoreDay {

/**
 * @brief 流云刺 170 劫火 余烬带区域实体组件。
 *
 * 实体挂 Position（线段中点，供渲染定位）；区域本身是
 * (start_x,start_y) -> (end_x,end_y) 的线段加半宽，由
 * skills::UpdateFlowingThrustEmbers（SkillSystem::Update 内）驱动：
 *  - 敌人进入线段范围 → 施加点燃（同一余烬对同一敌人只触发一次）；
 *  - 171 业火焚途分配时，主人站在自己余烬内 → 火焰伤害加成 buff 逐帧刷新。
 */
struct FlowingEmberZoneComponent {
  entt::entity owner = entt::null;
  float start_x = 0.0f;
  float start_y = 0.0f;
  float end_x = 0.0f;
  float end_y = 0.0f;
  float width = 60.0f;      // 线段半宽（码）
  float duration = 2.0f;    // 总持续（秒）
  float remaining = 2.0f;   // 剩余时间
  float tick_interval = 0.25f;
  float tick_accum = 0.0f;
  int infernal_points = 0;  // 171 分配点数（站位火焰伤害加成幅度）
  std::vector<entt::entity> ignited; // 已被该余烬点燃的敌人（去重）
};

/**
 * @brief 流云刺 175 元素余韵 传染 ICD 状态 + 135 虚实相生 离场检测状态（挂攻击者）。
 * last_infect_time 与 raylib GetTime() 秒值比较，1.0s 内部冷却内不再传染。
 * last_in_shadow_zone_time 记录玩家最后一次"处于残影离开判定区内"的时刻，
 * 用于检测"从残影附近离开"（离开时才触发护盾，防止进入区域瞬间误触发）。
 */
struct FlowingThrustStateComponent {
  float last_infect_time = -1.0e9f;
  float last_in_shadow_zone_time = -1.0e9f;
};

} // namespace NoMoreDay