#pragma once
#include <entt/entt.hpp>
#include "game/data/TalentData.hpp"
#include "game/components/Progression.hpp"

namespace NoMoreDay {

class AstrolabeSystem {
public:
    enum class NodeStatus { Locked, Available, Activated, FullyActivated, Sealed };

    // --- 解锁逻辑 ---
    
    // 检查节点是否可解锁 (基于亲和度阈值，而非前置节点)
    static bool canUnlockNode(
        const TalentGraph& graph,
        const AstrolabeComponent& comp,
        uint32_t nodeId
    );
    
    // 为节点投入 1 点
    // Returns true if successful
    static bool addPointToNode(
        entt::registry& registry,
        entt::entity player,
        const TalentGraph& graph,
        uint32_t nodeId
    );
    
    // --- 誓约机制 ---
    
    // 检查是否可以誓约某职业
    static bool canTakeVow(
        const AstrolabeComponent& comp,
        ProfessionID profession
    );
    
    // 执行誓约 (不可逆)
    static bool takeVow(
        entt::registry& registry,
        entt::entity player,
        ProfessionID profession
    );
    
    // --- 查询接口 ---
    
    // 获取节点状态
    static NodeStatus getNodeStatus(
        const TalentGraph& graph,
        const AstrolabeComponent& comp,
        uint32_t nodeId
    );
    
    // 获取节点当前/最大点数
    static std::pair<int, int> getNodePoints(
        const TalentGraph& graph,
        const AstrolabeComponent& comp,
        uint32_t nodeId
    );
    
    // --- 属性应用 ---
    static void applyTalentStats(
        entt::registry& registry,
        entt::entity player,
        const TalentGraph& graph
    );
};

} // namespace NoMoreDay