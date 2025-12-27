#include "../components/EnemyComponent.hpp"

// 实现 EnemyArchetype 中声明的静态行为函数
// 这些函数目前作为占位符，以满足链接器要求。
// 实际的 AI 逻辑主要由 AISystem 处理，或者可以在这里扩展特定的行为逻辑。

void EnemyArchetype::FodderBehavior(entt::registry& reg, entt::entity entity, float dt) {
    // 可以在这里实现特定的 AI 逻辑，例如特殊的移动模式或技能释放
    // 目前留空，因为通用移动逻辑由 AISystem 统一处理
}

void EnemyArchetype::TankBehavior(entt::registry& reg, entt::entity entity, float dt) {
}

void EnemyArchetype::RangerBehavior(entt::registry& reg, entt::entity entity, float dt) {
}

void EnemyArchetype::AssassinBehavior(entt::registry& reg, entt::entity entity, float dt) {
}