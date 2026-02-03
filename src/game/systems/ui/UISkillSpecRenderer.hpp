#pragma once
#include "raylib.h"

namespace NoMoreDay {

struct SkillData;
struct SkillTreeDefinition;
struct SpecializedSkill;
struct ActiveSkillsComponent;
struct TalentNode;

struct SkillSpecView {
    Vector2 offset = {0, 0};
    float zoom = 1.0f;
    float alpha = 1.0f;
    Vector2 center = {0, 0}; // Screen center
};

class UISkillSpecRenderer {
public:
    static void Draw(const SkillTreeDefinition* tree, 
                     const SpecializedSkill* specialized, 
                     const ActiveSkillsComponent* active,
                     const SkillData* skillData,
                     const SkillSpecView& view,
                     uint32_t hoveredNodeId = 0);
                     
    static Vector2 GetNodeScreenPos(const TalentNode& node, const SkillSpecView& view);
    static float GetNodeRadius(const TalentNode& node, const SkillSpecView& view);

private:
    static void DrawBackground(const SkillSpecView& view, const SkillData* skillData);
    static void DrawHub(const SkillSpecView& view, const SkillData* skillData);
    static void DrawConnections(const SkillTreeDefinition* tree, const SpecializedSkill* specialized, const SkillSpecView& view, Color theme);
    static void DrawNodes(const SkillTreeDefinition* tree, const SpecializedSkill* specialized, const ActiveSkillsComponent* active, const SkillSpecView& view, Color theme, uint32_t hoveredNodeId);
    
    // Helper to determine node shape/type
    enum class NodeType { Passive, Modifier, Keystone };
    static NodeType GetNodeType(const TalentNode& node);
    
    static Color GetThemeColor(const SkillData* skillData);
};

} // namespace NoMoreDay
