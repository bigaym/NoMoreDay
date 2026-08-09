#pragma once
#include "raylib.h"
#include <unordered_set>

namespace NoMoreDay {

struct SkillData;
struct SkillTreeDefinition;
struct SpecializedSkill;
struct ActiveSkillsComponent;
struct TalentNode;
struct NodeContractData;

struct SkillSpecView {
    Vector2 offset = {0, 0};
    float zoom = 1.0f;
    float alpha = 1.0f;
    Vector2 center = {0, 0}; // Screen center
};

class UISkillSpecRenderer {
public:
    enum class NodeType { Passive, Modifier, Keystone, Trigger, Synergy, Transmuter };

    static void Draw(const SkillTreeDefinition* tree, 
                     const SpecializedSkill* specialized, 
                     const ActiveSkillsComponent* active,
                     const SkillData* skillData,
                     const SkillSpecView& view,
                     uint32_t hoveredNodeId = 0,
                      const std::unordered_set<uint32_t>* excludedNodeIds = nullptr);
                      
    static Vector2 GetNodeScreenPos(const TalentNode& node, const SkillSpecView& view);
    static NodeType ClassifyNodeVisual(const TalentNode& node,
                                       const NodeContractData* contract = nullptr);
    static float GetNodeRadius(const TalentNode& node, const SkillSpecView& view,
                               const NodeContractData* contract = nullptr);

private:
    static void DrawBackground(const SkillTreeDefinition* tree,
                               const SkillSpecView& view);
    static void DrawHub(const SkillTreeDefinition* tree, const SkillData* skillData,
                        const SkillSpecView& view);
    static void DrawConnections(const SkillTreeDefinition* tree,
                                const SpecializedSkill* specialized,
                                const SkillSpecView& view);
    static void DrawNodes(const SkillTreeDefinition* tree,
                          const SpecializedSkill* specialized,
                          const ActiveSkillsComponent* active,
                          const SkillData* skillData,
                          const SkillSpecView& view, uint32_t hoveredNodeId,
                          const std::unordered_set<uint32_t>* excludedNodeIds);
};

} // namespace NoMoreDay
