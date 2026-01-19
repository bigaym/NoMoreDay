#include "game/systems/combat/EffectSystem.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/Buff.hpp"
#include "game/systems/combat/DamagePopupManager.hpp"
#include "game/systems/combat/MonsterAffixSystem.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "core/math/ThreadSafeRandom.hpp"
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

  // 5. 处理 DoT (Damage Over Time) Buff
  auto dotView = registry.view<ActiveEffectsComponent, HealthComponent, Position>();
  for (auto entity : dotView) {
    auto& activeEffects = dotView.get<ActiveEffectsComponent>(entity);
    const auto& pos = dotView.get<Position>(entity);
    
    for (auto& effect : activeEffects.effects) {
      if (effect.type == BuffType::DamageOverTime && effect.remaining > 0.0f) {
        effect.tick_timer += dt;
        if (effect.tick_timer >= effect.tick_interval) {
          effect.tick_timer = 0.0f;
          
          // 使用 DamagePipeline 计算伤害
          DamagePool base_pool;
          base_pool.Add(Tag::Poison, effect.tick_damage);
          
          auto result = DamagePipeline::Calculate(registry, effect.source, entity, 0, base_pool, Tag::None);
          
          // 发射伤害飘字
          EmitDamagePopup(registry, {pos.x, pos.y - 20.0f}, result.total_damage, result.is_crit, Tag::Poison);
        }
      }
    }
  }

  // 6. 更新通用视觉特效
  auto viewVisual = registry.view<VisualEffect, Position>();
  for (auto entity : viewVisual) {
    auto &effect = viewVisual.get<VisualEffect>(entity);
    auto &pos = viewVisual.get<Position>(entity);

    if (effect.timer == 0.0f) {
      for (int i = 0; i < 15; i++) {
        NoMoreDay::components::GPUParticle p;
        p.position = {pos.x, pos.y};
        float angle = NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.0f, 360.0f) * DEG2RAD;
        float speed = NoMoreDay::utils::ThreadSafeRandom::GetFloat(50.0f, 200.0f);
        p.velocity = {cosf(angle) * speed, sinf(angle) * speed};
        p.color = effect.color;
        p.lifetime = NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.5f, 1.5f);
        p.maxLifetime = p.lifetime;
        p.scale = NoMoreDay::utils::ThreadSafeRandom::GetFloat(2.0f, 6.0f);
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