#include "CombatEventDispatcher.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "core/logging/Logger.hpp"

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
    auto& handlers = GetHandlers();
    size_t idx = static_cast<size_t>(event.type);
    
    if (idx >= handlers.size()) {
        LOG_WARN("CombatEventDispatcher: Invalid event type {}", static_cast<int>(event.type));
        return;
    }
    
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
    
    // Sword Intent accumulation on hit
    Register(CombatEventType::OnSkillHit, [](entt::registry& registry, const CombatEvent& evt) {
        if (!registry.valid(evt.source)) return;
        
        auto* intent = registry.try_get<SwordIntentComponent>(evt.source);
        if (!intent) return;
        
        bool gainIntent = HasAnyTag(evt.tags, Tag::Melee | Tag::SwordSkill);
        
        // Critical hits always generate Intent
        if (evt.is_crit && evt.skill_id != 0) {
            gainIntent = true;
        }
        
        if (gainIntent && intent->stacks < intent->max_stacks) {
            intent->stacks++;
            intent->time_since_last_gain = 0.0f;
            intent->decay_tick_timer = 0.0f;
            LOG_DEBUG("Entity {} Sword Intent increased to {} (via event)", 
                      static_cast<uint32_t>(evt.source), intent->stacks);
        }
    }, 100); // High priority for core mechanics
    
    // Mana on hit
    Register(CombatEventType::OnSkillHit, [](entt::registry& registry, const CombatEvent& evt) {
        if (!registry.valid(evt.source)) return;
        
        auto* stats = registry.try_get<CombatStats>(evt.source);
        if (!stats || stats->mana_on_hit <= 0.0f) return;
        
        stats->mana = std::min(stats->max_mana, stats->mana + stats->mana_on_hit);
        LOG_DEBUG("Entity {} restored {:.1f} mana on hit (via event)", 
                  static_cast<uint32_t>(evt.source), stats->mana_on_hit);
    }, 50);
    
    LOG_INFO("CombatEventDispatcher: Default handlers initialized");
}

} // namespace NoMoreDay
