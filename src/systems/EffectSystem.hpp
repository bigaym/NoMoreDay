#pragma once
#include <entt/entt.hpp>

class EffectSystem {
public:
    // 更新特效生命周期和动画
    static void update(entt::registry& registry, float dt);
};