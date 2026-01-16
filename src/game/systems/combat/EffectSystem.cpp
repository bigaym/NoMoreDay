#include "game/systems/combat/EffectSystem.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "raymath.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/systems/combat/DamagePopupManager.hpp"
#include "game/systems/combat/EffectSystem.hpp"
#include "raymath.h"

using namespace NoMoreDay;

namespace NoMoreDay::systems {

void EffectSystem::update(entt::registry &registry, float dt) {
  // 1. 更新遗留的伤害飘字 (如果有)
  auto viewDamage = registry.view<DamagePopup, Position>();
  for (auto entity : viewDamage) {
    auto &popup = viewDamage.get<DamagePopup>(entity);
    popup.timer += dt;
    if (popup.timer >= popup.lifeTime) {
      registry.destroy(entity);
      continue;
    }
    auto &pos = registry.get<Position>(entity);
    pos.x += popup.velX * dt;
    pos.y += popup.velY * dt;
    popup.velY += 200.0f * dt;
    popup.velX *= (1.0f - 3.0f * dt);
  }

  // 2. 更新高性能飘字系统
  DamagePopupManager::Get().Update(dt);

  // 3. 更新攻击特效
  auto viewEffect = registry.view<AttackEffect>();
  for (auto entity : viewEffect) {
    auto &effect = viewEffect.get<AttackEffect>(entity);
    effect.timer += dt;
    if (effect.timer >= effect.lifeTime) {
      registry.destroy(entity);
    }
  }

  // 4. 更新通用视觉特效
  auto viewVisual = registry.view<VisualEffect, Position>();
  for (auto entity : viewVisual) {
    auto &effect = viewVisual.get<VisualEffect>(entity);
    auto &pos = viewVisual.get<Position>(entity);

    if (effect.timer == 0.0f) {
      for (int i = 0; i < 15; i++) { // Reduced count for standard effects
        NoMoreDay::components::GPUParticle p;
        p.position = {pos.x, pos.y};
        float angle = (float)(rand() % 360) * DEG2RAD;
        float speed = (float)(rand() % 150 + 50);
        p.velocity = {cosf(angle) * speed, sinf(angle) * speed};
        p.color = effect.color;
        p.lifetime = 0.5f + (rand() % 100) / 100.0f;
        p.maxLifetime = p.lifetime;
        p.scale = 2.0f + (rand() % 40) / 10.0f;
        NoMoreDay::systems::GPUParticleSystem::Get().Emit(p);
      }
    }

    effect.timer += dt;
    if (effect.timer >= effect.lifeTime) {
      if (effect.type != VisualEffectType::AoeArray) {
        registry.destroy(entity);
      }
    }
  }
}

void EffectSystem::EmitDamagePopup(entt::registry &registry, Vector2 position,
                                   float amount, bool isCrit,
                                   NoMoreDay::Tag damageType) {
  Color col = WHITE;
  if (isCrit) {
    col = GOLD;
  } else {
    if (HasTag(damageType, Tag::Fire))
      col = ORANGE;
    else if (HasTag(damageType, Tag::Cold))
      col = SKYBLUE;
    else if (HasTag(damageType, Tag::Lightning))
      col = YELLOW;
    else if (HasTag(damageType, Tag::Poison))
      col = LIME;
    else if (HasTag(damageType, Tag::Shadow))
      col = PURPLE;
  }

  DamagePopupManager::Get().Emit(position, amount, isCrit, col);
}

void EffectSystem::EmitStatusPopup(entt::registry &registry, Vector2 position,
                                   const std::string &text, Color color) {
  DamagePopupManager::Get().Emit(position, 0.0f, false, color, true, text);
}

} // namespace NoMoreDay::systems