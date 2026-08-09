#pragma once
#include "game/foundation/data/TagRegistry.hpp"
#include "raylib.h"
#include <entt/entt.hpp>
#include <string>


namespace NoMoreDay::systems {
class EffectSystem {
public:
  // 更新特效生命周期和动画
  static void update(entt::registry &registry, float dt);

  // 发射伤害飘字
  static void
  EmitDamagePopup(entt::registry &registry, Vector2 position, float amount,
                  bool isCrit,
                  NoMoreDay::Tag damageType = NoMoreDay::Tag::Physical);

  // 发射状态/文本飘字
  static void EmitStatusPopup(entt::registry &registry, Vector2 position,
                              const std::string &text, Color color);
};
} // namespace NoMoreDay::systems