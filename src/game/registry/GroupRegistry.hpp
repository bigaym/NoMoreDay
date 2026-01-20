#pragma once
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include <entt/entt.hpp>


namespace NoMoreDay::groups {

// Group 类型别名
// 注意: Position, Velocity, AIComponent 等目前在 Global Namespace
// CombatStats, ModifierList 在 NoMoreDay Namespace

// Combat Group: 属性计算核心
// Modified from Spec: Removed ModifierList and PrimaryStats from group
// definition because they are optional components. Including them would exclude
// entities (like Enemies) that don't have them from the updates. We include
// StatsDirty to optimize 'StatsSystem::update' iteration.
using CombatGroup =
    decltype(std::declval<entt::registry>()
                 .group<NoMoreDay::StatsDirty, NoMoreDay::CombatStats>());

// Render Group: GPU 同步核心
// Position, Velocity, Radius, GPUIndex are likely present on all renderable
// entities.
using RenderGroup =
    decltype(std::declval<entt::registry>()
                 .group<Position, Velocity, Radius, GPUIndex>());

// AI Group: AI 更新核心
// AIComponent, Position, Velocity, EnemyTag.
// Note: Position and Velocity are owned by RenderGroup. AIGroup observes them to avoid ownership conflicts.
using AIGroup =
    decltype(std::declval<entt::registry>()
                 .group<AIComponent>(entt::get<Position, Velocity, EnemyTag>));

// 必须在添加 any 组件前调用
inline void RegisterGroups(entt::registry &registry) {
  // 注册 CombatGroup
  registry.group<NoMoreDay::StatsDirty, NoMoreDay::CombatStats>();

  // 注册 RenderGroup
  registry.group<Position, Velocity, Radius, GPUIndex>();

  // 注册 AIGroup: 仅拥有 AIComponent，观察其他组件
  registry.group<AIComponent>(entt::get<Position, Velocity, EnemyTag>);
}

} // namespace NoMoreDay::groups
