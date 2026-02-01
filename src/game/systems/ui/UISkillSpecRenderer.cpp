#include "game/systems/ui/UISkillSpecRenderer.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "engine/render/UIRenderer.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include "engine/resource/AssetLoadingSystem.hpp"

namespace NoMoreDay {

// Helper to get screen position
Vector2 UISkillSpecRenderer::GetNodeScreenPos(const TalentNode& node, const SkillSpecView& view) {
    if (node.x == 0 && node.y == 0) {
        return { view.center.x + view.offset.x, view.center.y + view.offset.y };
    }

    // Last Epoch Style: Radial-Structured Grid
    // We treat node.x and node.y as grid coordinates, but apply a radial expansion
    // to make the tree feel organic and fill the widescreen properly.
    
    float spacingX = 140.0f * view.zoom;
    float spacingY = 120.0f * view.zoom; // Slightly tighter vertically for standard POV
    
    // Applying a slight "fisheye" expansion to prevent clusters from overlapping
    float r = std::sqrt(node.x * node.x + node.y * node.y);
    float expansion = 1.0f + 0.15f * std::log1p(r);
    
    float finalX = node.x * spacingX * expansion;
    float finalY = node.y * spacingY * expansion;

    return { view.center.x + view.offset.x + finalX, 
             view.center.y + view.offset.y + finalY };
}

Color UISkillSpecRenderer::GetThemeColor(const SkillData* skillData) {
    if (!skillData) return GOLD;
    
    if (HasTag(skillData->tags, Tag::Fire)) return Color{ 255, 120, 30, 255 };
    if (HasTag(skillData->tags, Tag::Cold)) return Color{ 100, 200, 255, 255 };
    if (HasTag(skillData->tags, Tag::Lightning)) return Color{ 220, 240, 60, 255 };
    if (HasTag(skillData->tags, Tag::Void)) return Color{ 180, 70, 255, 255 };
    if (HasTag(skillData->tags, Tag::Poison)) return Color{ 50, 220, 80, 255 };
    if (HasTag(skillData->tags, Tag::Physical) || HasTag(skillData->tags, Tag::Melee)) return Color{ 255, 60, 60, 255 };
    
    return GOLD;
}

UISkillSpecRenderer::NodeType UISkillSpecRenderer::GetNodeType(const TalentNode& node) {
    if (node.max_points == 1) return NodeType::Keystone;
    if (node.max_points <= 4) return NodeType::Modifier;
    return NodeType::Passive;
}

float UISkillSpecRenderer::GetNodeRadius(const TalentNode& node, const SkillSpecView& view) {
    float baseRadius = 40.0f * view.zoom;
    NodeType type = GetNodeType(node);
    if (type == NodeType::Keystone) return baseRadius * 1.35f;
    if (type == NodeType::Modifier) return baseRadius * 0.95f;
    return baseRadius * 0.75f;
}

void UISkillSpecRenderer::Draw(const SkillTreeDefinition* tree, 
                               const SpecializedSkill* specialized, 
                               const ActiveSkillsComponent* active, 
                               const SkillData* skillData,
                               const SkillSpecView& view) {
    Color theme = GetThemeColor(skillData);
    DrawBackground(view, skillData);
    DrawConnections(tree, specialized, view, theme);
    DrawHub(view, skillData);
    DrawNodes(tree, specialized, active, view, theme);
}

void UISkillSpecRenderer::DrawBackground(const SkillSpecView& view, const SkillData* skillData) {
    Color theme = GetThemeColor(skillData);
    Vector2 treeCenter = { view.center.x + view.offset.x, view.center.y + view.offset.y };
    
    // 1. Central Ambient Glow (Multiple layers for soft falloff)
    for(int i = 0; i < 3; i++) {
        float r = (600.0f + i * 300.0f) * view.zoom;
        float alphaMult = 0.12f / (i + 1);
        DrawCircleGradient((int)treeCenter.x, (int)treeCenter.y, r, Fade(theme, alphaMult * view.alpha), Fade(BLANK, 0.0f));
    }
    
    // 2. Subtle Grid Pattern
    float gridSize = 100.0f * view.zoom;
    float startX = fmodf(view.offset.x, gridSize);
    float startY = fmodf(view.offset.y, gridSize);
    Color gridColor = Fade(theme, 0.04f * view.alpha);
    
    for (float x = startX; x < GetScreenWidth(); x += gridSize)
        DrawLineEx({x, 0}, {x, (float)GetScreenHeight()}, 1.0f, gridColor);
    for (float y = startY; y < GetScreenHeight(); y += gridSize)
        DrawLineEx({0, y}, {(float)GetScreenWidth(), y}, 1.0f, gridColor);

    // 3. Vignette (Simulated with large radial gradient)
    DrawCircleGradient(GetScreenWidth()/2, GetScreenHeight()/2, (float)GetScreenWidth(), Fade(BLANK, 0.0f), Fade(BLACK, 0.65f * view.alpha));
}

void UISkillSpecRenderer::DrawHub(const SkillSpecView& view, const SkillData* skillData) {
    Vector2 centerPos = { view.center.x + view.offset.x, view.center.y + view.offset.y };
    float radius = 55.0f * view.zoom;
    Color theme = GetThemeColor(skillData);
    
    // Multi-layered Hub shell
    DrawCircleV(centerPos, radius * 1.25f, Fade(theme, 0.15f * view.alpha));
    DrawCircleV(centerPos, radius, Fade(BLACK, 0.95f * view.alpha));
    
    // Skill Icon
    if (skillData && skillData->icon_id != 0) {
        Texture2D icon = AssetLoadingSystem::GetTexture(skillData->icon_id);
        if (icon.id != 0) {
            float iconSize = radius * 1.6f;
            Rectangle dest = { centerPos.x - iconSize/2, centerPos.y - iconSize/2, iconSize, iconSize };
            Rectangle src = { 0, 0, (float)icon.width, (float)icon.height };
            DrawTexturePro(icon, src, dest, {0,0}, 0.0f, Fade(WHITE, view.alpha));
        }
    }
    
    DrawPolyLinesEx(centerPos, 40, radius, 0.0f, 3.0f * view.zoom, Fade(theme, 0.8f * view.alpha));
    DrawPolyLinesEx(centerPos, 40, radius + 8 * view.zoom, 0.0f, 1.0f, Fade(theme, 0.3f * view.alpha));
}

void UISkillSpecRenderer::DrawConnections(const SkillTreeDefinition* tree, const SpecializedSkill* specialized, const SkillSpecView& view, Color theme) {
    float lineThick = 9.0f * view.zoom;
    Color colorLocked = Color{ 45, 45, 50, (unsigned char)(200 * view.alpha) };
    Color colorAvailable = Fade(theme, 0.4f * view.alpha); // Brighter than locked

    auto DrawLink = [&](Vector2 p1, Vector2 p2, bool active, bool available) {
        // Multi-layered line rendering
        if (active) {
            // Active: High-energy glow
            DrawLineEx(p1, p2, lineThick + 4 * view.zoom, Fade(theme, 0.3f * view.alpha));
            DrawLineEx(p1, p2, lineThick, Fade(theme, 0.9f * view.alpha));
            DrawLineEx(p1, p2, lineThick * 0.4f, Fade(WHITE, 0.6f * view.alpha));
            
            // Flow particles
            float time = (float)GetTime() * 1.5f;
            for(int i = 0; i < 2; i++) {
                float pulsePos = fmodf(time + i * 0.5f, 1.0f);
                Vector2 pulse = Vector2Lerp(p1, p2, pulsePos);
                DrawCircleV(pulse, lineThick * 0.6f, Fade(WHITE, 0.8f * view.alpha));
            }
        } else if (available) {
            // Available: Dimmer but clearly different from locked
            DrawLineEx(p1, p2, lineThick + 1 * view.zoom, Fade(BLACK, 0.4f * view.alpha));
            DrawLineEx(p1, p2, lineThick, colorAvailable);
            DrawLineEx(p1, p2, lineThick * 0.3f, Fade(WHITE, 0.2f * view.alpha));
        } else {
            // Locked: Very dark/subtle
            DrawLineEx(p1, p2, lineThick, colorLocked);
        }
    };

    for (const auto& [id, node] : tree->nodes) {
        Vector2 targetPos = GetNodeScreenPos(node, view);
        bool nodeAlloc = specialized->allocated_points.contains(id) && specialized->allocated_points.at(id) > 0;
        
        // 1. Explicit Prerequisites
        for (uint32_t preId : node.prerequisites) {
            if (tree->nodes.count(preId)) {
                Vector2 sourcePos = GetNodeScreenPos(tree->nodes.at(preId), view);
                bool preAlloc = specialized->allocated_points.contains(preId) && specialized->allocated_points.at(preId) > 0;
                
                // A link is "available" if the source is allocated
                DrawLink(sourcePos, targetPos, preAlloc && nodeAlloc, preAlloc);
            }
        }
        
        // 2. Implicit connection to Root (Hub)
        if (node.prerequisites.empty()) {
             Vector2 sourcePos = { view.center.x + view.offset.x, view.center.y + view.offset.y };
             DrawLink(sourcePos, targetPos, nodeAlloc, true); // Root links always available
        }
    }
}

void UISkillSpecRenderer::DrawNodes(const SkillTreeDefinition* tree, const SpecializedSkill* specialized, const ActiveSkillsComponent* active, const SkillSpecView& view, Color theme) {
    for (const auto& [id, node] : tree->nodes) {
        Vector2 pos = GetNodeScreenPos(node, view);
        NodeType type = GetNodeType(node);
        float radius = GetNodeRadius(node, view);
        
        int currentPts = specialized->allocated_points.contains(id) ? specialized->allocated_points.at(id) : 0;
        bool isMaxed = currentPts >= node.max_points;
        bool isAllocated = currentPts > 0;
        
        bool canUnlock = true;
        for (uint32_t preId : node.prerequisites) {
            if (!specialized->allocated_points.contains(preId) || specialized->allocated_points.at(preId) <= 0) {
                canUnlock = false; break;
            }
        }
        
        Color baseColor = canUnlock ? Fade(theme, 0.6f) : Color{ 70, 70, 75, 255 };
        if (isAllocated) baseColor = isMaxed ? GOLD : theme;
        Color borderColor = Fade(baseColor, view.alpha);
        
        // 1. Node Background Glow (Dynamic based on allocation/availability)
        if (isAllocated) {
            float glowSize = radius * (isMaxed ? 2.3f : 1.8f);
            DrawCircleGradient((int)pos.x, (int)pos.y, glowSize, Fade(baseColor, 0.3f * view.alpha), Fade(BLANK, 0.0f));
        } else if (canUnlock) {
            // Breathing effect for unlockable nodes
            float breath = (sinf((float)GetTime() * 4.0f) + 1.0f) * 0.5f;
            float glowSize = radius * (1.3f + breath * 0.4f);
            DrawCircleGradient((int)pos.x, (int)pos.y, glowSize, Fade(theme, (0.1f + breath * 0.15f) * view.alpha), Fade(BLANK, 0.0f));
        }

        // 2. Shape Base
        Color bgColor = Fade(BLACK, 0.98f * view.alpha);
        if (type == NodeType::Keystone) {
            DrawPoly(pos, 4, radius, 0.0f, bgColor);
            DrawPolyLinesEx(pos, 4, radius, 0.0f, 4.0f * view.zoom, borderColor);
            if (isMaxed) {
                DrawPolyLinesEx(pos, 4, radius + 5 * view.zoom, 0.0f, 1.5f * view.zoom, Fade(borderColor, 0.6f));
                DrawPolyLinesEx(pos, 4, radius + 10 * view.zoom, 0.0f, 1.0f * view.zoom, Fade(borderColor, 0.3f));
            } else if (canUnlock) {
                // Subtle highlight for available keystone
                DrawPolyLinesEx(pos, 4, radius + 3 * view.zoom, 0.0f, 1.0f * view.zoom, Fade(theme, 0.4f));
            }
        } else if (type == NodeType::Modifier) {
            DrawPoly(pos, 4, radius, 45.0f, bgColor);
            DrawPolyLinesEx(pos, 4, radius, 45.0f, 3.5f * view.zoom, borderColor);
        } else {
            DrawCircleV(pos, radius, bgColor);
            // Available passives get a slightly thicker ring
            float borderThick = canUnlock ? 3.0f : 2.0f;
            DrawRing(pos, radius - borderThick * view.zoom, radius, 0, 360, 32, borderColor);        
        }
        // 3. Progress Filling (SDF-like radial fill)
        if (isAllocated && !isMaxed) {
             float pct = (float)currentPts / node.max_points;
             if (type == NodeType::Passive) {
                 DrawCircleSector(pos, radius * 0.85f, -90, -90 + 360 * pct, 32, Fade(borderColor, 0.6f));
             } else {
                 float innerR = radius * 0.8f * pct;
                 if (type == NodeType::Modifier) DrawPoly(pos, 4, innerR, 45.0f, Fade(borderColor, 0.6f));
                 else DrawPoly(pos, 4, innerR, 0.0f, Fade(borderColor, 0.6f));
             }
        }
        
        // 4. Points Text
        int fontSize = (int)(22 * view.zoom);
        const char* text = TextFormat("%d/%d", currentPts, node.max_points);
        Vector2 textSize = MeasureTextEx(UISystem::GetFont(), text, (float)fontSize, 1.0f);
        Vector2 textPos = {pos.x - textSize.x/2, pos.y + radius + 12 * view.zoom};
        
        // Higher contrast text for available nodes
        Color textColor = isAllocated ? (isMaxed ? GOLD : WHITE) : (canUnlock ? Fade(WHITE, 0.9f) : Fade(WHITE, 0.4f));

        DrawTextEx(UISystem::GetFont(), text, {textPos.x + 1, textPos.y + 1}, (float)fontSize, 1.0f, Fade(BLACK, 0.8f * view.alpha));
        DrawTextEx(UISystem::GetFont(), text, textPos, (float)fontSize, 1.0f, Fade(textColor, view.alpha));
    }
}


} // namespace NoMoreDay
