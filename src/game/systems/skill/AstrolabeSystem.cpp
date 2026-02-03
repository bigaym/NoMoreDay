#include "game/systems/skill/AstrolabeSystem.hpp"
#include "game/systems/stats/AttributePipeline.hpp"

namespace NoMoreDay {

bool AstrolabeSystem::canUnlockNode(const TalentGraph& graph, const AstrolabeComponent& comp, uint32_t nodeId) {
    const auto* node = graph.findNode(nodeId);
    if (!node) return false;

    // Check Max Points
    if (comp.getNodePoints(nodeId) >= node->maxPoints) return false;
    
    // Check Tier Requirement
    int affinity = comp.getAffinity(node->profession);
    int required = 0;
    if (node->tier == 2) required = TierThreshold::TIER_2;
    else if (node->tier == 3) required = TierThreshold::TIER_3;
    
    if (affinity < required) return false;
    
    // Check Core Vow Requirement
    if (node->type == TalentNodeType::Core) {
        if (!comp.hasVow()) return false; // Must have vowed
        if (!comp.isMainProfession(node->profession)) return false; // Must be main
    }

    return true;
}

bool AstrolabeSystem::addPointToNode(entt::registry& registry, entt::entity player, const TalentGraph& graph, uint32_t nodeId) {
    auto& comp = registry.get<AstrolabeComponent>(player);
    
    if (comp.available_points <= 0) return false;
    if (!canUnlockNode(graph, comp, nodeId)) return false;

    const auto* node = graph.findNode(nodeId);
    if (!node) return false;

    // Deduct point
    comp.available_points--;
    
    // Add point to node
    comp.nodePoints[nodeId]++;
    
    // Add affinity
    comp.professionAffinity[static_cast<uint8_t>(node->profession)]++;
    
    // Mark activated (useful for simple iteration if needed, though nodePoints is primary now)
    comp.activated_nodes.insert(nodeId);
    
    // Recalculate stats
    AttributePipeline::Calculate(registry, player);
    
    return true;
}

bool AstrolabeSystem::canTakeVow(const AstrolabeComponent& comp, ProfessionID profession) {
    return !comp.hasVow();
}

bool AstrolabeSystem::takeVow(entt::registry& registry, entt::entity player, ProfessionID profession) {
    auto& comp = registry.get<AstrolabeComponent>(player);
    if (comp.hasVow()) return false;
    
    comp.mainProfession = static_cast<int>(profession);
    
    // Trigger Recalc (Core nodes might unlock or stats might change if Vow grants bonuses)
    AttributePipeline::Calculate(registry, player);
    
    return true;
}

AstrolabeSystem::NodeStatus AstrolabeSystem::getNodeStatus(const TalentGraph& graph, const AstrolabeComponent& comp, uint32_t nodeId) {
    const auto* node = graph.findNode(nodeId);
    if (!node) return NodeStatus::Locked;
    
    uint8_t points = comp.getNodePoints(nodeId);
    if (points >= node->maxPoints) return NodeStatus::FullyActivated;
    if (points > 0) return NodeStatus::Activated;
    
    // Check Core Lock
    if (node->type == TalentNodeType::Core) {
        if (comp.hasVow() && !comp.isMainProfession(node->profession)) {
            return NodeStatus::Sealed;
        }
    }
    
    if (canUnlockNode(graph, comp, nodeId)) return NodeStatus::Available;
    
    return NodeStatus::Locked;
}

std::pair<int, int> AstrolabeSystem::getNodePoints(const TalentGraph& graph, const AstrolabeComponent& comp, uint32_t nodeId) {
    const auto* node = graph.findNode(nodeId);
    if (!node) return {0, 0};
    
    return { comp.getNodePoints(nodeId), node->maxPoints };
}

void AstrolabeSystem::applyTalentStats(entt::registry& registry, entt::entity player, const TalentGraph& graph) {
    AttributePipeline::Calculate(registry, player);
}

} // namespace NoMoreDay