#include "game/systems/ui/UISkillSpecRenderer.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "engine/render/UIRenderer.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include "engine/resource/AssetLoadingSystem.hpp"
#include "rlgl.h" 

namespace NoMoreDay {

    static void DrawRingGradient(Vector2 center, float innerRadius, float outerRadius, Color startColor, Color endColor) {
        if (innerRadius >= outerRadius) return;
        
        rlBegin(RL_QUADS);
        for (int i = 0; i < 360; i += 10) {
            float angle1 = (float)i * DEG2RAD;
            float angle2 = (float)(i + 10) * DEG2RAD;
            
            float cin1 = cosf(angle1), sin1 = sinf(angle1);
            float cin2 = cosf(angle2), sin2 = sinf(angle2);
            
            rlColor4ub(startColor.r, startColor.g, startColor.b, startColor.a);
            rlVertex2f(center.x + cin1 * innerRadius, center.y + sin1 * innerRadius);
            rlVertex2f(center.x + cin2 * innerRadius, center.y + sin2 * innerRadius);
            
            rlColor4ub(endColor.r, endColor.g, endColor.b, endColor.a);
            rlVertex2f(center.x + cin2 * outerRadius, center.y + sin2 * outerRadius);
            rlVertex2f(center.x + cin1 * outerRadius, center.y + sin1 * outerRadius);
        }
        rlEnd();
    }

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
                               const SkillSpecView& view,
                               uint32_t hoveredNodeId) {
    Color theme = GetThemeColor(skillData);
    DrawBackground(view, skillData);
    DrawConnections(tree, specialized, view, theme);
    DrawHub(view, skillData);
    DrawNodes(tree, specialized, active, view, theme, hoveredNodeId);
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

void UISkillSpecRenderer::DrawNodes(const SkillTreeDefinition* tree, const SpecializedSkill* specialized, const ActiveSkillsComponent* active, const SkillSpecView& view, Color theme, uint32_t hoveredNodeId) {
    for (const auto& [id, node] : tree->nodes) {
        Vector2 pos = GetNodeScreenPos(node, view);
        NodeType type = GetNodeType(node);
        float baseRadius = GetNodeRadius(node, view);
        
        // Hover Effect
        bool isHovered = (id == hoveredNodeId);
        float radius = isHovered ? baseRadius * 1.15f : baseRadius;

        int currentPts = specialized->allocated_points.contains(id) ? specialized->allocated_points.at(id) : 0;
        bool isMaxed = currentPts >= node.max_points;
        bool isAllocated = currentPts > 0;
        
        bool canUnlock = true;
        for (uint32_t preId : node.prerequisites) {
            if (!specialized->allocated_points.contains(preId) || specialized->allocated_points.at(preId) <= 0) {
                canUnlock = false; break;
            }
        }
        
        Color baseColor = canUnlock ? Fade(theme, 0.8f) : Color{ 60, 60, 65, 255 };
        if (isAllocated) baseColor = isMaxed ? GOLD : theme;
        Color borderColor = Fade(baseColor, view.alpha);
        
        // 1. Node Background Glow (Dynamic based on allocation/availability)
        // MOVED to outside only, as requested
        if (isAllocated) {
            float glowStart = radius + 2.0f;
            float glowEnd = radius + (isMaxed ? 12.0f : 8.0f) * view.zoom;
            DrawRingGradient(pos, glowStart, glowEnd, Fade(baseColor, 0.5f * view.alpha), Fade(BLANK, 0.0f));
        } else if (canUnlock) {
            float breath = (sinf((float)GetTime() * 4.0f) + 1.0f) * 0.5f;
            float glowStart = radius + 2.0f;
            float glowEnd = radius + (6.0f + breath * 4.0f) * view.zoom;
            DrawRingGradient(pos, glowStart, glowEnd, Fade(theme, (0.3f + breath * 0.2f) * view.alpha), Fade(BLANK, 0.0f));
        }

        // 2. Shape Base & Icon Background
        Color bgColor = Fade(BLACK, 0.95f * view.alpha);
        // If allocated, we might want a colored background inside?
        // Let's keep it dark for contrast with the icon/progress, but maybe tinted slightly
        if (isAllocated) bgColor = ColorTint(bgColor, Fade(theme, 0.2f));

        if (type == NodeType::Keystone) {
            // Octagon-like via rotated squares or Poly(8)? Raylib Poly(8) is good.
            // Let's stick to the "Diamond with heavy frame" look or Octagon.
            // Let's try regular Octagon for "Foundation/Keystone" feel.
            DrawPoly(pos, 8, radius, 22.5f, bgColor); // 8 sides
            DrawPolyLinesEx(pos, 8, radius, 22.5f, 3.0f * view.zoom, borderColor);
            
            if (isMaxed) {
                DrawPolyLinesEx(pos, 8, radius + 4.0f * view.zoom, 22.5f, 1.5f * view.zoom, Fade(GOLD, 0.7f * view.alpha));
            }
        } else if (type == NodeType::Modifier) {
            // Hexagon for Modifier
            DrawPoly(pos, 6, radius, 0.0f, bgColor);
            DrawPolyLinesEx(pos, 6, radius, 0.0f, 2.5f * view.zoom, borderColor);
        } else {
            // Circle for Passive
            DrawCircleV(pos, radius, bgColor);
            float borderThick = canUnlock ? 2.5f : 1.5f;
            DrawRing(pos, radius - borderThick * view.zoom, radius, 0, 360, 32, borderColor);        
        }

        // 3. Progress Filling (SDF-like radial fill or Inner fill)
        if (isAllocated && !isMaxed) {
             float pct = (float)currentPts / node.max_points;
             float innerR = radius * 0.85f;
             
             // Draw inner filled shape based on pct
             // Maybe just fill the whole background partially?
             // Let's use a "Sector" approach for all, or an inner scaling shape.
             // Inner scaling shape looks decent.
             
             Color fillCol = Fade(borderColor, 0.5f);
             if (type == NodeType::Keystone) DrawPoly(pos, 8, innerR * pct, 22.5f, fillCol);
             else if (type == NodeType::Modifier) DrawPoly(pos, 6, innerR * pct, 0.0f, fillCol);
             else DrawCircleV(pos, innerR * pct, fillCol);
        }
        else if (isMaxed) {
             // Full fill for maxed
             float innerR = radius * 0.85f;
             Color fillCol = Fade(theme, 0.3f);
             if (type == NodeType::Keystone) DrawPoly(pos, 8, innerR, 22.5f, fillCol);
             else if (type == NodeType::Modifier) DrawPoly(pos, 6, innerR, 0.0f, fillCol);
             else DrawCircleV(pos, innerR, fillCol);
        }

        // 4. Icons
        if (node.icon_id > 0) {
            Texture2D icon = AssetLoadingSystem::GetTexture(node.icon_id);
            if (icon.id != 0) {
                float iconSize = radius * 1.4f; 
                Rectangle dest = { pos.x - iconSize/2, pos.y - iconSize/2, iconSize, iconSize };
                Rectangle src = { 0, 0, (float)icon.width, (float)icon.height };
                DrawTexturePro(icon, src, dest, {0,0}, 0.0f, Fade(WHITE, isAllocated || canUnlock ? view.alpha : 0.5f * view.alpha));
            }
        } else {
            // Draw a generic glyph (dot or smaller shape) if no icon
            if (isAllocated) {
                DrawCircleV(pos, radius * 0.3f, Fade(theme, 0.8f * view.alpha));
            }
        }
        
        // 5. Points Text (Improved with Pilled Background)
        if (isHovered || isAllocated) {
            int fontSize = (int)(20 * view.zoom);
            const char* text = TextFormat("%d/%d", currentPts, node.max_points);
            Vector2 textSize = MeasureTextEx(UISystem::GetFont(), text, (float)fontSize, 1.0f);
            
            float padding = 4.0f;
            Rectangle pillHitbox = { 
                pos.x - textSize.x/2 - padding, 
                pos.y + radius + 14 * view.zoom - padding, 
                textSize.x + padding*2, 
                textSize.y + padding*2 
            };
            
            DrawRectangleRounded(pillHitbox, 0.5f, 4, Fade(BLACK, 0.7f * view.alpha));
            DrawRectangleRoundedLinesEx(pillHitbox, 0.5f, 4, 1.0f, Fade(borderColor, 0.5f * view.alpha));

            Color textColor = isAllocated ? (isMaxed ? GOLD : WHITE) : (canUnlock ? Fade(WHITE, 0.9f) : Fade(WHITE, 0.4f));
            DrawTextEx(UISystem::GetFont(), text, {pillHitbox.x + padding, pillHitbox.y + padding}, (float)fontSize, 1.0f, Fade(textColor, view.alpha));
        }
    }
}


} // namespace NoMoreDay
