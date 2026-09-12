#include "game/systems/combat/EffectSystem.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EffectComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/SkillDefs.hpp" // PhantomTranceComponent (988)
#include "game/foundation/components/Stats.hpp"    // StatsDirty
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/DamagePopupManager.hpp"
#include "game/systems/combat/MonsterAffixSystem.hpp"
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

  // 5. 更新所有 ActiveEffects 的生命周期 (CRITICAL FIX: Buff System Lifecycle)
  auto buffView = registry.view<ActiveEffectsComponent>();
  // 收集同步状态
  bool playerRooted = false;
  bool playerSilenced = false;
  
  for (auto entity : buffView) {
      auto& activeEffects = buffView.get<ActiveEffectsComponent>(entity);
      const std::size_t before = activeEffects.effects.size();
      // 988 御剑化影：绝影形态内御剑步衰减减半。
      float swordStepDrainMult = 1.0f;
      if (const auto* trance = registry.try_get<PhantomTranceComponent>(entity)) {
          if (trance->remaining > 0.0f) {
              swordStepDrainMult = trance->params.sword_step_drain_mult;
          }
      }
      activeEffects.Update(dt, swordStepDrainMult);
      // 到期删除会改变生效集合，需标记 StatsDirty 触发属性重算。本函数是
      // ActiveEffects 生命周期的唯一 owner（StatsSystem::UpdateBuffs 不再衰减），
      // 因此由这里承担集合变化感知。
      if (activeEffects.effects.size() != before) {
          registry.get_or_emplace<StatsDirty>(entity);
      }
      
      // 状态同步逻辑 (SyncStatusFlags)
      if (registry.any_of<PlayerTag>(entity)) {
          bool hasRoot = false;
          bool hasSilence = false;
          for (const auto& effect : activeEffects.effects) {
              if (effect.type == BuffType::Root) hasRoot = true;
              if (effect.type == BuffType::Silence) hasSilence = true;
          }
          playerRooted = hasRoot;
          playerSilenced = hasSilence;
      }
  }

  // 同步到 PlayerStats
  auto playerView = registry.view<PlayerTag, PlayerStats>();
  for (auto entity : playerView) {
      auto& stats = playerView.get<PlayerStats>(entity);
      stats.isRooted = playerRooted;
      stats.isSilenced = playerSilenced;
  }

  // 6. 统一异常系统 DoT tick
  AilmentTickDriver::Tick(registry, dt);

  // 7. 更新通用视觉特效
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
                                   NoMoreDay::render::StatusPopupKind kind,
                                   Color color) {
  DamagePopupManager::Get().Emit(position, 0.0f, false, color, kind);
}

} // namespace NoMoreDay::systems
