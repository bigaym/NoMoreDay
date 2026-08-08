#include "game/systems/combat/DamageResolutionHooks.hpp"

#include "core/logging/Logger.hpp"

#include <mutex>

namespace NoMoreDay {
namespace {

std::mutex &HooksMutex() {
  static std::mutex mutex;
  return mutex;
}

DamageResolutionHooks &ActiveHooks() {
  static DamageResolutionHooks hooks;
  return hooks;
}

} // namespace

void RegisterDamageResolutionHooks(const DamageResolutionHooks &hooks) {
  std::lock_guard<std::mutex> lock(HooksMutex());
  ActiveHooks() = hooks;
}

void ClearDamageResolutionHooks() {
  std::lock_guard<std::mutex> lock(HooksMutex());
  ActiveHooks() = DamageResolutionHooks();
}

DamageExecutionResult ResolveDamage(entt::registry &registry,
                                    const DamageRequest &request,
                                    entt::entity target) {
  // Copy the hooks under the lock, then invoke without holding it so nested
  // damage resolution (events -> counters -> more damage) cannot deadlock.
  DamageResolutionHooks hooks;
  {
    std::lock_guard<std::mutex> lock(HooksMutex());
    hooks = ActiveHooks();
  }

  if (!hooks.execute) {
    LOG_WARN("[DamageResolution] ResolveDamage called but no damage resolution "
             "hooks are registered; returning no-op result.");
    return DamageExecutionResult{};
  }

  return hooks.execute(registry, request, target);
}

std::vector<DamageResult>
ResolveDamageBatch(entt::registry &registry, const DamageRequest &request) {
  DamageResolutionHooks hooks;
  {
    std::lock_guard<std::mutex> lock(HooksMutex());
    hooks = ActiveHooks();
  }

  if (!hooks.calculateBatch) {
    LOG_WARN("[DamageResolution] ResolveDamageBatch called but no damage "
             "resolution hooks are registered; returning empty results.");
    return {};
  }

  return hooks.calculateBatch(registry, request);
}

} // namespace NoMoreDay
