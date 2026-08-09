#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/contracts/impl/ProcBudgetManager.hpp"
#include "game/contracts/impl/CombatTelemetry.hpp"
#include "core/logging/Logger.hpp"
#include <algorithm>

namespace NoMoreDay {

std::array<std::vector<CombatEventDispatcher::HandlerEntry>, static_cast<size_t>(CombatEventType::Count)>& 
CombatEventDispatcher::GetHandlers() {
    static std::array<std::vector<HandlerEntry>, static_cast<size_t>(CombatEventType::Count)> s_handlers;
    return s_handlers;
}

uint32_t& CombatEventDispatcher::GetNextId() {
    static uint32_t s_next_id = 1;
    return s_next_id;
}

uint32_t CombatEventDispatcher::Register(CombatEventType type, Handler handler, int priority) {
    std::unique_lock lock(s_dispatcherMutex);
    auto& handlers = GetHandlers();
    size_t idx = static_cast<size_t>(type);
    
    if (idx >= handlers.size()) {
        LOG_WARN("CombatEventDispatcher: Invalid event type {}", static_cast<int>(type));
        return 0;
    }
    
    uint32_t id = GetNextId()++;
    
    // Insert in priority order (higher priority first)
    auto& vec = handlers[idx];
    auto it = std::lower_bound(vec.begin(), vec.end(), priority,
        [](const HandlerEntry& entry, int p) { return entry.priority > p; });
    
    vec.insert(it, HandlerEntry{std::move(handler), priority, id});
    
    LOG_DEBUG("CombatEventDispatcher: Registered handler {} for event type {} with priority {}", 
              id, static_cast<int>(type), priority);
    
    return id;
}

void CombatEventDispatcher::Unregister(CombatEventType type, uint32_t handler_id) {
    std::unique_lock lock(s_dispatcherMutex);
    auto& handlers = GetHandlers();
    size_t idx = static_cast<size_t>(type);
    
    if (idx >= handlers.size()) return;
    
    auto& vec = handlers[idx];
    vec.erase(
        std::remove_if(vec.begin(), vec.end(), 
            [handler_id](const HandlerEntry& e) { return e.id == handler_id; }),
        vec.end()
    );
}

void CombatEventDispatcher::Dispatch(entt::registry& registry, const CombatEvent& event) {
  if (!ProcBudgetManager::Get().RequestEventEmit()) {
    return;
    }

    std::shared_lock lock(s_dispatcherMutex);
    auto& handlers = GetHandlers();
    size_t idx = static_cast<size_t>(event.type);
    
  if (idx >= handlers.size()) {
    LOG_WARN("CombatEventDispatcher: Invalid event type {}", static_cast<int>(event.type));
    return;
  }

  CombatTelemetry::Get().RecordCombatEvent(event);
    
  const auto& vec = handlers[idx];
    
    for (const auto& entry : vec) {
        try {
            entry.handler(registry, event);
        } catch (const std::exception& e) {
            LOG_ERROR("CombatEventDispatcher: Handler {} threw exception: {}", entry.id, e.what());
        }
    }
}

void CombatEventDispatcher::Clear() {
    std::unique_lock lock(s_dispatcherMutex);
    auto& handlers = GetHandlers();
    for (auto& vec : handlers) {
        vec.clear();
    }
    GetNextId() = 1;
    LOG_INFO("CombatEventDispatcher: All handlers cleared");
}

void CombatEventDispatcher::Init() {
    Clear();
    
    // --- Register default system handlers ---
    
    
    // Unified hit recovery path (budget-gated life/mana on hit)
    Register(CombatEventType::OnSkillHit, [](entt::registry& registry, const CombatEvent& evt) {
        if (!registry.valid(evt.source)) return;
        
        auto* stats = registry.try_get<CombatStats>(evt.source);
        if (!stats) return;

        auto& procBudget = ProcBudgetManager::Get();

        if (stats->mana_on_hit > 0.0f) {
            const float manaMissing = std::max(0.0f, stats->max_mana - stats->mana);
            const float manaGain = std::min(manaMissing, stats->mana_on_hit);
            if (manaGain > 0.0f &&
                procBudget.RequestProc(evt.source, ProcBudgetType::ManaOnHit, manaGain)) {
                stats->mana += manaGain;
                LOG_DEBUG("Entity {} restored {:.1f} mana on hit (budgeted)",
                          static_cast<uint32_t>(evt.source), manaGain);
            }
        }

        if (stats->life_on_hit > 0.0f) {
            auto* hp = registry.try_get<HealthComponent>(evt.source);
            if (!hp) return;

            const float lifeMissing = std::max(0.0f, hp->max - hp->current);
            const float lifeGain = std::min(lifeMissing, stats->life_on_hit);
            if (lifeGain > 0.0f &&
                procBudget.RequestProc(evt.source, ProcBudgetType::LifeOnHit, lifeGain)) {
                hp->current += lifeGain;
                LOG_DEBUG("Entity {} restored {:.1f} life on hit (budgeted)",
                          static_cast<uint32_t>(evt.source), lifeGain);
            }
        }
    }, 50);
    
    LOG_INFO("CombatEventDispatcher: Default handlers initialized");
}

} // namespace NoMoreDay
