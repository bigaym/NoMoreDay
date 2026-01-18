#include "game/systems/combat/EffectSystem.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/systems/combat/DamagePopupManager.hpp"
#include "game/systems/combat/MonsterAffixSystem.hpp"
#include "raymath.h"
#include <vector>

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

  // 4. 延迟销毁组件处理
  std::vector<entt::entity> toDestroy;
  auto viewDelayed = registry.view<DelayedDestroyComponent>();
  for (auto entity : viewDelayed) {
    auto &delayed = viewDelayed.get<DelayedDestroyComponent>(entity);
    delayed.timer -= dt;
    if (delayed.timer <= 0.0f) {
      toDestroy.push_back(entity);
    }
  }
  for (auto entity : toDestroy) {
    if (registry.valid(entity)) {
      registry.destroy(entity);
    }
  }

  // 5. 熔火伤害区域处理 (Molten Trail Damage)
  static constexpr float MOLTEN_DAMAGE_PER_SEC = 15.0f;
  static constexpr float MOLTEN_DAMAGE_TICK = 0.25f; // 每0.25秒造成一次伤害
  static float moltenDamageTimer = 0.0f;
  moltenDamageTimer += dt;
  
  if (moltenDamageTimer >= MOLTEN_DAMAGE_TICK) {
    moltenDamageTimer = 0.0f;
    
    auto playerView = registry.view<PlayerTag, Position, HealthComponent>();
    auto moltenView = registry.view<MoltenTrailTag, Position, Radius>();
    
    for (auto player : playerView) {
      const auto& playerPos = playerView.get<Position>(player);
      auto& hp = playerView.get<HealthComponent>(player);
      
      for (auto molten : moltenView) {
        const auto& moltenPos = moltenView.get<Position>(molten);
        const auto& radius = moltenView.get<Radius>(molten);
        
        float dx = playerPos.x - moltenPos.x;
        float dy = playerPos.y - moltenPos.y;
        float distSq = dx * dx + dy * dy;
        
        if (distSq < radius.value * radius.value) {
          // Deal fire damage
          float damage = MOLTEN_DAMAGE_PER_SEC * MOLTEN_DAMAGE_TICK;
          hp.current -= damage;
          
          // Emit fire damage popup
          EmitDamagePopup(registry, {playerPos.x, playerPos.y - 20.0f}, damage, false, Tag::Fire);
          
          // Only hit once per tick
          break;
        }
      }
    }
  }

  // 6. 熔火视觉粒子效果 (每火焰区域偶尔发射粒子)
  auto moltenParticleView = registry.view<MoltenTrailTag, Position>();
  for (auto entity : moltenParticleView) {
    // 10% 几率每帧发射粒子 (避免过多)
    if (rand() % 100 < 3) {
      const auto& pos = moltenParticleView.get<Position>(entity);
      NoMoreDay::components::GPUParticle p;
      p.position = {pos.x + (rand() % 20 - 10), pos.y + (rand() % 20 - 10)};
      p.velocity = {(float)(rand() % 20 - 10), -50.0f - (float)(rand() % 30)};
      p.color = {255, (unsigned char)(80 + rand() % 100), 0, 220};
      p.lifetime = 0.4f + (rand() % 30) / 100.0f;
      p.maxLifetime = p.lifetime;
      p.scale = 3.0f + (rand() % 20) / 10.0f;
      NoMoreDay::systems::GPUParticleSystem::Get().Emit(p);
    }
  }

  // 7. 更新通用视觉特效
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