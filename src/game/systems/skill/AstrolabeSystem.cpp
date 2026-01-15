#include "game/systems/skill/AstrolabeSystem.hpp"
#include "entt/entt.hpp"
#include "game/components/Progression.hpp"
#include "game/components/Stats.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include "core/logging/Logger.hpp"

namespace NoMoreDay {

static void handle_node_effect(entt::registry& registry, entt::entity entity, const AstrolabeNodeEffect& effect, bool active) {
    switch (effect.type) {
        case AstrolabeEffectType::GrantComponent:
            if (effect.value == "SwordHeart") {
                if (active) {
                    (void)registry.get_or_emplace<SwordHeartComponent>(entity);
                    LOG_INFO("AstrolabeSystem: Entity {} granted SwordHeart trait", (uint32_t)entity);
                } else {
                    if (registry.all_of<SwordHeartComponent>(entity)) {
                        registry.remove<SwordHeartComponent>(entity);
                        LOG_INFO("AstrolabeSystem: Entity {} revoked SwordHeart trait", (uint32_t)entity);
                    }
                }
            } else if (effect.value == "SwordIntentUnlock") {
                if (active) {
                    (void)registry.get_or_emplace<SwordIntentComponent>(entity);
                    LOG_INFO("AstrolabeSystem: Entity {} unlocked Sword Intent", (uint32_t)entity);
                } else {
                    if (registry.all_of<SwordIntentComponent>(entity)) {
                        registry.remove<SwordIntentComponent>(entity);
                        LOG_INFO("AstrolabeSystem: Entity {} revoked Sword Intent", (uint32_t)entity);
                    }
                }
            }
            break;

        case AstrolabeEffectType::ModifyIntent:
            if (auto* intent = registry.try_get<SwordIntentComponent>(entity)) {
                if (effect.value.find("MaxSwordIntent:") == 0) {
                    int bonus = std::stoi(effect.value.substr(15));
                    if (active) intent->max_stacks += bonus;
                    else intent->max_stacks -= bonus;
                    intent->stacks = std::min(intent->stacks, intent->max_stacks);
                }
                else if (effect.value.find("SwordIntentGain:") == 0) {
                    float bonus = std::stof(effect.value.substr(16));
                    if (active) intent->gain_rate += bonus;
                    else intent->gain_rate -= bonus;
                }
                else if (effect.value.find("SwordIntentGrace:") == 0) {
                    float bonus = std::stof(effect.value.substr(17));
                    if (active) intent->grace_period += bonus;
                    else intent->grace_period -= bonus;
                }
            }
            break;

        case AstrolabeEffectType::ModifyStat:
            // This is primarily handled in StatsSystem::Recalculate, 
            // but we could handle immediate non-cached effects here if needed.
            break;
    }
}

bool AstrolabeSystem::can_activate(entt::registry& registry, entt::entity entity, uint32_t node_id) {
    auto* astrolabe = registry.try_get<AstrolabeComponent>(entity);
    if (!astrolabe) return false;

    // Check if already activated
    if (astrolabe->activated_nodes.contains(node_id)) {
        return false;
    }

    // Check if points available
    if (astrolabe->available_points <= 0) {
        return false;
    }

    // Check prerequisites
    const auto* node = AstrolabeRegistry::Get().GetNode(node_id);
    if (!node) return false;

    for (uint32_t prereq_id : node->prerequisites) {
        if (!astrolabe->activated_nodes.contains(prereq_id)) {
            return false;
        }
    }

    return true;
}

bool AstrolabeSystem::activate_node(entt::registry& registry, entt::entity entity, uint32_t node_id) {
    if (!can_activate(registry, entity, node_id)) {
        return false;
    }

    auto& astrolabe = registry.get<AstrolabeComponent>(entity);
    astrolabe.activated_nodes.insert(node_id);
    astrolabe.available_points--;

    // Handle special effects
    const auto* node = AstrolabeRegistry::Get().GetNode(node_id);
    if (node) {
        for (const auto& effect : node->effects) {
            handle_node_effect(registry, entity, effect, true);
        }
    }

    // Mark stats as dirty to trigger recalculation
    registry.emplace_or_replace<StatsDirty>(entity);

    LOG_INFO("AstrolabeSystem: Entity {} activated node {}", (uint32_t)entity, node_id);
    return true;
}

bool AstrolabeSystem::deactivate_node(entt::registry& registry, entt::entity entity, uint32_t node_id) {
    auto* astrolabe = registry.try_get<AstrolabeComponent>(entity);
    if (!astrolabe || !astrolabe->activated_nodes.contains(node_id)) {
        return false;
    }

    // Check if it's a leaf node
    const auto& nodes = AstrolabeRegistry::Get().GetAllNodes();
    bool isPrereq = false;
    for (const auto& otherId : astrolabe->activated_nodes) {
        const auto* otherNode = AstrolabeRegistry::Get().GetNode(otherId);
        if (otherNode) {
            for (uint32_t prereqId : otherNode->prerequisites) {
                if (prereqId == node_id) {
                    isPrereq = true;
                    break;
                }
            }
        }
        if (isPrereq) break;
    }

    if (isPrereq) {
        LOG_WARN("Cannot deactivate node {}: It is required by other active nodes.", node_id);
        return false;
    }

    astrolabe->activated_nodes.erase(node_id);
    astrolabe->available_points++;

    // Revoke special effects
    const auto* node = AstrolabeRegistry::Get().GetNode(node_id);
    if (node) {
        for (const auto& effect : node->effects) {
            handle_node_effect(registry, entity, effect, false);
        }
    }

    // Mark stats as dirty
    registry.emplace_or_replace<StatsDirty>(entity);

    LOG_INFO("AstrolabeSystem: Entity {} deactivated node {}", (uint32_t)entity, node_id);
    return true;
}

} // namespace NoMoreDay
