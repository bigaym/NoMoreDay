#include "game/systems/skill/SummonLifecycleSystem.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include <algorithm>
#include <vector>

namespace NoMoreDay::systems {

void SummonLifecycleSystem::Update(entt::registry &registry, float dt) {
  static thread_local std::vector<entt::entity> toDestroy;
  toDestroy.clear();

  auto view = registry.view<SummonComponent>();
  for (auto entity : view) {
    auto &summon = view.get<SummonComponent>(entity);
    if (summon.max_lifetime > 0.0f) {
      summon.lifetime -= dt;
      if (summon.lifetime <= 0.0f || !registry.valid(summon.owner)) {
        if (registry.valid(summon.owner)) {
          CombatEventDispatcher::Dispatch(
              registry, CombatEventFactory::CreateMinionDeath(summon.owner, entity));
        }
        toDestroy.push_back(entity);
        continue;
      }
    } else {
      // 永久维持型召唤物：仅当宿主失效时销毁
      if (!registry.valid(summon.owner)) {
        toDestroy.push_back(entity);
        continue;
      }
    }

    auto *profile = registry.try_get<SummonCombatProfile>(entity);
    auto *runtime = registry.try_get<SummonRuntimeState>(entity);
    if (profile && runtime && profile->proc_budget_cap > 0.0f) {
      runtime->proc_budget =
          std::clamp(runtime->proc_budget + profile->proc_budget_per_second * dt,
                     0.0f, profile->proc_budget_cap);
    }
  }

  for (const auto entity : toDestroy) {
    if (registry.valid(entity)) {
      registry.destroy(entity);
    }
  }
}

} // namespace NoMoreDay::systems
