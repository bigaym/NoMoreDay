#include "game/systems/combat/EliteModifierSystem.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Combat.hpp"
#include "game/components/Common.hpp"
#include "game/components/EliteModifierComponents.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include <algorithm>
#include <cmath>

namespace NoMoreDay {

uint32_t EliteModifierSystem::s_killHandlerId = 0;
bool EliteModifierSystem::s_initialized = false;

// 辅助函数：计算两点之间的距离平方
static float DistanceSq(const Position &a, const Position &b) {
  float dx = a.x - b.x;
  float dy = a.y - b.y;
  return dx * dx + dy * dy;
}

void EliteModifierSystem::Init() {
  if (s_initialized)
    return;

  // 注册 OnKill 事件处理器
  using namespace NoMoreDay::Constants::Combat::Elite;
  s_killHandlerId = CombatEventDispatcher::Register(
      CombatEventType::OnKill, &EliteModifierSystem::OnEnemyKilled,
      50 // 中等优先级
  );

  s_initialized = true;
  LOG_INFO("EliteModifierSystem: Initialized");
}

void EliteModifierSystem::Shutdown() {
  if (!s_initialized)
    return;

  if (s_killHandlerId != 0) {
    CombatEventDispatcher::Unregister(CombatEventType::OnKill, s_killHandlerId);
    s_killHandlerId = 0;
  }

  s_initialized = false;
  LOG_INFO("EliteModifierSystem: Shutdown");
}

void EliteModifierSystem::Update(entt::registry &registry, float dt) {
  // 更新 Link 连接状态
  UpdateSoulLinks(registry);

  // 更新 Avenger 视觉效果（发光强度等）
  auto avengerView = registry.view<AvengerComponent, SpriteComponent>();
  for (auto [entity, avenger, sprite] : avengerView.each()) {
    // 根据层数更新发光强度
    avenger.glowIntensity =
        static_cast<float>(avenger.avengerStacks) / avenger.maxStacks;

    // 更新体型（通过 scale）
    float sizeMultiplier = avenger.GetSizeMultiplier();
    // 注意：这里假设有一个 baseScale 字段，或者我们直接设置
    // sprite.scale = baseScale * sizeMultiplier;
    // 为了避免累积，我们使用原始比例
  }
}

void EliteModifierSystem::UpdateSoulLinks(entt::registry &registry) {
  // 注意：这里理想情况下应该使用 PhysicsSystem::Get().GetSpatialGrid() 进行查询
  // 为了简化展示且保持高性能，我们按间隔更新链接关系
  static int frameCounter = 0;
  using namespace NoMoreDay::Constants::Combat::Elite;
  if (++frameCounter % UPDATE_INTERVAL_FRAMES != 0) return;

  auto view = registry.view<SoulLinkComponent, Position, EnemyTag>();

  for (auto [entity, link, pos] : view.each()) {
    link.linkedEntities.clear();
    float rangeSq = link.linkRange * link.linkRange;

    // 优化：仅查找周围的敌人（此处逻辑应对接 SpatialGrid，暂保留 view 但添加有效性检查）
    auto searchView = registry.view<EnemyTag, Position, HealthComponent>();
    for (auto [other, otherPos, health] : searchView.each()) {
      if (other == entity || !registry.valid(other)) continue;
      if (link.linkedEntities.size() >= static_cast<size_t>(link.maxLinks)) break;

      float distSq = DistanceSq(pos, otherPos);
      if (distSq <= rangeSq) {
        link.linkedEntities.push_back(other);
      }
    }

    // 如果有链接，计算共享血量池
    if (!link.linkedEntities.empty()) {
      link.isLinkLeader = true;
      link.totalSharedHealth = 0.0f;
      link.maxSharedHealth = 0.0f;

      // 包含自己的血量
      if (auto *myHealth = registry.try_get<HealthComponent>(entity)) {
        link.totalSharedHealth += myHealth->current;
        link.maxSharedHealth += myHealth->max;
      }

      // 累加链接成员的血量
      for (auto linked : link.linkedEntities) {
        if (auto *health = registry.try_get<HealthComponent>(linked)) {
          link.totalSharedHealth += health->current;
          link.maxSharedHealth += health->max;
        }
      }
    }
  }
}

bool EliteModifierSystem::DistributeDamageToLinkGroup(entt::registry &registry,
                                                      entt::entity target,
                                                      float damage) {
  // 检查目标是否有 SoulLink 组件
  auto *link = registry.try_get<SoulLinkComponent>(target);
  if (!link || link->linkedEntities.empty()) {
    return false; // 不在 Link 组中，正常处理伤害
  }

  // 计算伤害分配
  size_t groupSize = link->linkedEntities.size() + 1; // +1 包含自己
  float damagePerMember = damage / static_cast<float>(groupSize);

  LOG_DEBUG("SoulLink: Distributing {} damage to {} members ({} each)", damage,
            groupSize, damagePerMember);

  // 对自己造成分摊伤害
  if (auto *myHealth = registry.try_get<HealthComponent>(target)) {
    myHealth->current -= damagePerMember;
    if (myHealth->current < 0)
      myHealth->current = 0;
  }

  // 对链接成员造成分摊伤害
  for (auto linked : link->linkedEntities) {
    if (!registry.valid(linked))
      continue;

    if (auto *health = registry.try_get<HealthComponent>(linked)) {
      health->current -= damagePerMember;
      if (health->current < 0)
        health->current = 0;

      // 发射链接伤害粒子
      if (auto *linkedPos = registry.try_get<Position>(linked)) {
        for (int i = 0; i < 5; ++i) {
          NoMoreDay::components::GPUParticle p;
          p.position = {linkedPos->x, linkedPos->y};
          float angle = static_cast<float>(rand() % 360) * DEG2RAD;
          float speed = static_cast<float>(rand() % 50 + 30);
          p.velocity = {cosf(angle) * speed, sinf(angle) * speed};
          p.color = {200, 50, 200, 255}; // 粉紫色表示链接伤害
          p.lifetime = 0.4f;
          p.maxLifetime = p.lifetime;
          p.scale = 2.0f;
          NoMoreDay::systems::GPUParticleSystem::Get().Emit(p);
        }
      }
    }
  }

  return true; // 伤害已分配
}

void EliteModifierSystem::OnEnemyKilled(entt::registry &registry,
                                        const CombatEvent &evt) {
  // 检查被击杀者是否是敌人
  if (!registry.valid(evt.target) || !registry.any_of<EnemyTag>(evt.target)) {
    return;
  }

  // 获取被击杀者的位置
  const auto *victimPos = registry.try_get<Position>(evt.target);
  if (!victimPos)
    return;

  // 查找周围的 Avenger 敌人
  auto view =
      registry.view<AvengerComponent, Position, EnemyTag, CombatStats>();

  for (auto [entity, avenger, pos, combat] : view.each()) {
    if (entity == evt.target)
      continue; // 跳过被击杀者自己

    // 检查是否在范围内
    float distSq = DistanceSq(*victimPos, pos);
    float rangeSq = avenger.stackRadius * avenger.stackRadius;

    if (distSq <= rangeSq) {
      // 增加层数
      int oldStacks = avenger.avengerStacks;
      avenger.AddStack(1);

      LOG_DEBUG("Avenger {} gained stack ({} -> {})",
                static_cast<uint32_t>(entity), oldStacks,
                avenger.avengerStacks);

      // 更新伤害加成逻辑已重构：StatsSystem 会在计算时调用 AvengerComponent::GetDamageMultiplier()
      // 这里不再直接修改 CombatStats 原始数值，避免指数爆炸

      // 视觉效果：红色愤怒粒子
      for (int i = 0; i < 15; ++i) {
        NoMoreDay::components::GPUParticle p;
        p.position = {pos.x, pos.y};
        float angle = static_cast<float>(rand() % 360) * DEG2RAD;
        float speed = static_cast<float>(rand() % 80 + 40);
        p.velocity = {cosf(angle) * speed, sinf(angle) * speed};
        using namespace NoMoreDay::Constants::Combat::Elite;
        p.color = {255, 50, 50, 255}; // 红色
        p.lifetime = 0.6f;
        p.maxLifetime = p.lifetime;
        p.scale = Vfx_Scale_Base + avenger.avengerStacks * Vfx_Scale_Step;
        NoMoreDay::systems::GPUParticleSystem::Get().Emit(p);
      }

      // 体型增加视觉反馈
      if (auto *sprite = registry.try_get<SpriteComponent>(entity)) {
        // 假设 Sprite 有 scale 字段
        // sprite->scale *= (1.0f + avenger.sizePerStack);
      }
    }
  }
}

} // namespace NoMoreDay
