#pragma once
#include "game/contracts/DamagePipelineTypes.hpp"
#include <entt/entt.hpp>
#include <functional>
#include <vector>

namespace NoMoreDay {

/**
 * @brief Registration contract for damage resolution hooks.
 *
 * The combat domain (DamagePipeline) registers itself at static-init time so
 * that gameplay domains (skill, summon, etc.) can resolve damage through
 * ResolveDamage / ResolveDamageBatch without a compile-time dependency on the
 * combat domain.
 */
struct DamageResolutionHooks {
  std::function<DamageExecutionResult(entt::registry &, const DamageRequest &,
                                      entt::entity)>
      execute;
  std::function<std::vector<DamageResult>(entt::registry &,
                                          const DamageRequest &)>
      calculateBatch;
};

void RegisterDamageResolutionHooks(const DamageResolutionHooks &hooks);
void ClearDamageResolutionHooks();

/**
 * @brief Resolves a single damage request through the registered hooks.
 *
 * When no hooks are registered the call is a no-op that returns a default
 * DamageExecutionResult and logs a warning.
 */
DamageExecutionResult ResolveDamage(entt::registry &registry,
                                    const DamageRequest &request,
                                    entt::entity target);

/**
 * @brief Resolves a single damage request through the registered batch hook.
 *
 * Returns the per-target damage results for the request's defender. When no
 * hooks are registered the call is a no-op that returns an empty vector and
 * logs a warning.
 */
std::vector<DamageResult>
ResolveDamageBatch(entt::registry &registry, const DamageRequest &request);

} // namespace NoMoreDay
