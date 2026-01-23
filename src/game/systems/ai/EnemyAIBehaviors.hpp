#pragma once

#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/systems/world/MapSystem.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay::AI {

/**
 * @brief 支援者行为：远离玩家，定期给周围友军施加 Buff
 *
 * 行为逻辑：
 * 1. 保持与玩家 200-400 像素的安全距离
 * 2. 每 5 秒向周围 300 像素内的友军施加 Shield 或 SpeedUp Buff
 * 3. 如果玩家过于接近，优先逃跑
 */
void UpdateSupportBehavior(entt::registry &registry, entt::entity entity,
                           AIComponent &ai, Position &pos, Velocity &vel,
                           const Position &playerPos, float dt);

/**
 * @brief 刺客行为：潜行 + 背刺
 *
 * 行为逻辑：
 * 1. 进入隐身状态（渲染 Alpha 降低到 30%）
 * 2. 等待时机（监听玩家施放长硬直技能或冷却结束）
 * 3. 瞬移到玩家背后 30 像素，造成 2.5x 伤害的背刺攻击
 * 4. 背刺后进入短暂冷却
 */
void UpdateAssassinBehavior(entt::registry &registry, entt::entity entity,
                            AIComponent &ai, Position &pos, Velocity &vel,
                            const MapSystem &mapSystem,
                            const Position &playerPos, float dt);

/**
 * @brief 坦克行为：阻挡玩家与远程友军之间的视线
 *
 * 行为逻辑：
 * 1. 识别最近的 RANGER 类型友军作为保护目标
 * 2. 计算"玩家 → 远程友军"连线的中点偏移位置
 * 3. 移动到该位置进行阻挡
 * 4. 高质量属性使其难以被击退
 */
void UpdateTankBehavior(entt::registry &registry, entt::entity entity,
                        AIComponent &ai, Position &pos, Velocity &vel,
                        const Position &playerPos, float dt);

/**
 * @brief 查找周围的远程友军（用于坦克保护目标选择）
 */
entt::entity FindNearestRanger(entt::registry &registry, const Position &pos,
                               float searchRadius, entt::entity exclude);

/**
 * @brief 给周围友军施加 Buff
 * @param buffType 要施加的 Buff 类型 (Shield 或 SpeedUp)
 */
void ApplyBuffToNearbyAllies(entt::registry &registry, entt::entity source,
                             const Position &pos, float radius);

/**
 * @brief 执行背刺攻击
 * @return 是否成功执行背刺
 */
bool ExecuteBackstab(entt::registry &registry, entt::entity assassin,
                     const MapSystem &mapSystem, const Position &playerPos,
                     float backstabMultiplier);

} // namespace NoMoreDay::AI
