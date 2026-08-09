#pragma once
#include <functional>
#include <vector>
#include <array>
#include <algorithm>
#include <shared_mutex>
#include <entt/entt.hpp>
#include "game/contracts/CombatEvents.hpp"

namespace NoMoreDay {

/**
 * @brief Priority-based event dispatcher for combat events.
 * 
 * Allows talents, equipment affixes, AI systems, and skills to register
 * handlers for various combat events. Handlers are called in priority order.
 * 
 * Usage:
 *   CombatEventDispatcher::Register(CombatEventType::OnCrit, [](auto& reg, auto& evt) {
 *       // Handle critical hit
 *   }, 100); // Priority 100 (higher = earlier)
 * 
 *   CombatEventDispatcher::Dispatch(registry, evt);
 */
class CombatEventDispatcher {
public:
    using Handler = std::function<void(entt::registry&, const CombatEvent&)>;
    
    struct HandlerEntry {
        Handler handler;
        int priority = 0;
        uint32_t id = 0; // For removal
    };

    /**
     * @brief Register a handler for a specific event type.
     * @param type The event type to handle
     * @param handler The callback function
     * @param priority Higher priority handlers are called first (default: 0)
     * @return Handler ID for later removal
     */
    static uint32_t Register(CombatEventType type, Handler handler, int priority = 0);
    
    /**
     * @brief Remove a previously registered handler.
     * @param type The event type
     * @param handler_id The ID returned by Register()
     */
    static void Unregister(CombatEventType type, uint32_t handler_id);
    
    /**
     * @brief Dispatch an event to all registered handlers.
     * @param registry The ECS registry
     * @param event The event to dispatch
     */
    static void Dispatch(entt::registry& registry, const CombatEvent& event);
    
    /**
     * @brief Clear all registered handlers (for testing).
     */
    static void Clear();
    
    /**
     * @brief Initialize default event handlers.
     * Called once during game initialization.
     */
    static void Init();

private:
    static std::array<std::vector<HandlerEntry>, static_cast<size_t>(CombatEventType::Count)>& GetHandlers();
    static uint32_t& GetNextId();
    static inline std::shared_mutex s_dispatcherMutex;
};

} // namespace NoMoreDay
