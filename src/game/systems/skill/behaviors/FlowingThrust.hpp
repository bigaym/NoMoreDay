#pragma once

#include <entt/entt.hpp>

namespace NoMoreDay::skills {

// 更新流云刺余烬带 (170 劫火 / 171 业火焚途)：
//  - 对进入余烬的敌人施加点燃（同余烬去重）；
//  - 171 分配时，主人站在自己余烬上刷新火焰伤害加成 buff。
// 由 SkillSystem::Update 每帧调用，dt 为帧间隔秒。
void UpdateFlowingThrustEmbers(entt::registry &registry, float dt);

// 更新流云刺 135 虚实相生：
//  - 每帧检查玩家与自身残影的距离，从"残影离开判定区"离开时
//    获得相当于敏捷 ×(2.0×点数) 的临时护盾 (Ward)，持续 3s。
// 由 SkillSystem::Update 每帧调用，dt 为帧间隔秒。
void UpdateFlowingThrustPhantomShield(entt::registry &registry, float dt);

} // namespace NoMoreDay::skills