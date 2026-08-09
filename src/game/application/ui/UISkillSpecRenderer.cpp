#include "game/application/ui/UISkillSpecRenderer.hpp"

#include "engine/resource/AssetLoadingSystem.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/application/ui/BladeMasteryUITheme.hpp"
#include "game/application/ui/UISystem.hpp"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>

namespace NoMoreDay {
namespace {

void DrawRingGradient(Vector2 center, float innerRadius, float outerRadius,
                      Color startColor, Color endColor) {
    if (innerRadius >= outerRadius) {
        return;
    }

    rlBegin(RL_QUADS);
    for (int i = 0; i < 360; i += 10) {
        const float angle1 = static_cast<float>(i) * DEG2RAD;
        const float angle2 = static_cast<float>(i + 10) * DEG2RAD;

        const float cos1 = cosf(angle1);
        const float sin1 = sinf(angle1);
        const float cos2 = cosf(angle2);
        const float sin2 = sinf(angle2);

        rlColor4ub(startColor.r, startColor.g, startColor.b, startColor.a);
        rlVertex2f(center.x + cos1 * innerRadius, center.y + sin1 * innerRadius);
        rlVertex2f(center.x + cos2 * innerRadius, center.y + sin2 * innerRadius);

        rlColor4ub(endColor.r, endColor.g, endColor.b, endColor.a);
        rlVertex2f(center.x + cos2 * outerRadius, center.y + sin2 * outerRadius);
        rlVertex2f(center.x + cos1 * outerRadius, center.y + sin1 * outerRadius);
    }
    rlEnd();
}

bool IsPrerequisiteSatisfiedOr(const TalentNode& node,
                              const SkillTreeDefinition* tree,
                              const SpecializedSkill* specialized) {
    if (!tree || !specialized || node.prerequisites.empty()) {
        return true;
    }

    bool hasValidPrereq = false;
    for (const auto& pre : node.prerequisites) {
        const uint32_t preId = pre.node_id;
        if (preId == 0 || !tree->nodes.contains(preId)) {
            continue;
        }
        hasValidPrereq = true;
        const int prePts = specialized->allocated_points.contains(preId)
                               ? specialized->allocated_points.at(preId)
                               : 0;
        const int requiredPoints = pre.required_points > 0 ? pre.required_points : 1;
        if (prePts >= requiredPoints) {
            return true;
        }
    }
    return !hasValidPrereq;
}

const BladeMasteryUIThemeProfile& ResolveThemeProfile(const SkillTreeDefinition* tree) {
    if (tree != nullptr) {
        return GetBladeMasteryUIThemeProfile(tree->mastery_id);
    }
    return GetBladeMasteryUIThemeProfile(BladeMasteryId::None);
}

Color GetTagAccentColor(const SkillData* skillData) {
    if (!skillData) {
        return WHITE;
    }
    if (HasTag(skillData->tags, Tag::Fire)) {
        return Color{255, 128, 72, 255};
    }
    if (HasTag(skillData->tags, Tag::Cold)) {
        return Color{134, 212, 255, 255};
    }
    if (HasTag(skillData->tags, Tag::Lightning)) {
        return Color{228, 236, 92, 255};
    }
    if (HasTag(skillData->tags, Tag::Void)) {
        return Color{178, 116, 255, 255};
    }
    if (HasTag(skillData->tags, Tag::Poison)) {
        return Color{88, 208, 114, 255};
    }
    return WHITE;
}

Color GetLocalNodeAccent(const SkillData* skillData,
                         UISkillSpecRenderer::NodeType type,
                         const BladeMasteryUIThemeProfile& theme) {
    if (type == UISkillSpecRenderer::NodeType::Transmuter ||
        type == UISkillSpecRenderer::NodeType::Trigger) {
        return GetTagAccentColor(skillData);
    }
    if (type == UISkillSpecRenderer::NodeType::Synergy) {
        return theme.highlight;
    }
    return theme.primary;
}

void DrawDiagonalCuts(const Vector2 center, const SkillSpecView& view,
                      const BladeMasteryUIThemeProfile& theme) {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    for (int i = -2; i <= 2; ++i) {
        const float offset = static_cast<float>(i) * 180.0f * view.zoom;
        const Vector2 start = {center.x - width * 0.55f, center.y - height * 0.42f + offset};
        const Vector2 end = {center.x + width * 0.55f, center.y + height * 0.18f + offset};
        DrawLineEx(start, end, (i == 0 ? 4.0f : 2.0f) * view.zoom,
                   Fade(i == 0 ? theme.highlight : theme.secondary,
                        (i == 0 ? 0.12f : 0.07f) * view.alpha));
    }
}

void DrawOrbitArcs(const Vector2 center, const SkillSpecView& view,
                   const BladeMasteryUIThemeProfile& theme) {
    const float radii[] = {170.0f, 290.0f, 410.0f};
    const float phase = fmodf(static_cast<float>(GetTime()) * 14.0f, 360.0f);
    for (int i = 0; i < 3; ++i) {
        const float radius = radii[i] * view.zoom;
        DrawRingLines(center, radius - 2.0f * view.zoom, radius + 2.0f * view.zoom,
                      phase + i * 45.0f, phase + 120.0f + i * 35.0f, 48,
                      Fade(i == 1 ? theme.highlight : theme.primary,
                           (i == 1 ? 0.16f : 0.10f) * view.alpha));
    }
}

void DrawBrokenPlate(const Vector2 center, const SkillSpecView& view,
                     const BladeMasteryUIThemeProfile& theme) {
    const float outerRadius = 360.0f * view.zoom;
    DrawPolyLinesEx(center, 8, outerRadius, 22.5f, 2.0f * view.zoom,
                    Fade(theme.secondary, 0.12f * view.alpha));
    DrawPolyLinesEx(center, 8, outerRadius * 0.72f, 22.5f, 1.0f * view.zoom,
                    Fade(theme.primary, 0.10f * view.alpha));

    const Vector2 crackA = {center.x - outerRadius * 0.56f, center.y - outerRadius * 0.18f};
    const Vector2 crackB = {center.x - outerRadius * 0.16f, center.y + outerRadius * 0.06f};
    const Vector2 crackC = {center.x + outerRadius * 0.22f, center.y - outerRadius * 0.12f};
    DrawLineEx(crackA, crackB, 2.0f * view.zoom,
               Fade(theme.danger, 0.16f * view.alpha));
    DrawLineEx(crackB, crackC, 2.0f * view.zoom,
               Fade(theme.secondary, 0.18f * view.alpha));
}

void DrawNodeShape(Vector2 pos, float radius, UISkillSpecRenderer::NodeType type,
                   Color fillColor, Color borderColor, float zoom, bool maxed,
                   const BladeMasteryUIThemeProfile& theme, Color accent,
                   float alpha) {
    switch (type) {
    case UISkillSpecRenderer::NodeType::Keystone:
        DrawPoly(pos, 8, radius, 22.5f, fillColor);
        DrawPolyLinesEx(pos, 8, radius, 22.5f, 3.0f * zoom, borderColor);
        if (maxed) {
            DrawPolyLinesEx(pos, 8, radius + 4.0f * zoom, 22.5f, 1.4f * zoom,
                            Fade(theme.highlight, 0.72f * alpha));
        }
        break;
    case UISkillSpecRenderer::NodeType::Trigger:
        DrawPoly(pos, 3, radius, -90.0f, fillColor);
        DrawPolyLinesEx(pos, 3, radius, -90.0f, 2.5f * zoom, borderColor);
        break;
    case UISkillSpecRenderer::NodeType::Transmuter:
        DrawPoly(pos, 4, radius, 45.0f, fillColor);
        DrawPolyLinesEx(pos, 4, radius, 45.0f, 2.6f * zoom, borderColor);
        DrawPolyLinesEx(pos, 4, radius + 5.0f * zoom, 45.0f, 1.0f * zoom,
                        Fade(accent, 0.45f * alpha));
        break;
    case UISkillSpecRenderer::NodeType::Modifier:
    case UISkillSpecRenderer::NodeType::Synergy:
        DrawPoly(pos, 6, radius, 0.0f, fillColor);
        DrawPolyLinesEx(pos, 6, radius, 0.0f, 2.5f * zoom, borderColor);
        break;
    case UISkillSpecRenderer::NodeType::Passive:
    default:
        DrawCircleV(pos, radius, fillColor);
        DrawRing(pos, radius - 2.4f * zoom, radius, 0, 360, 32, borderColor);
        break;
    }

    switch (theme.node_shell) {
    case BladeMasteryNodeShell::BeveledDiamond:
        DrawPolyLinesEx(pos, 4, radius + 6.0f * zoom, 45.0f, 1.2f * zoom,
                        Fade(theme.secondary, 0.48f * alpha));
        break;
    case BladeMasteryNodeShell::OrbitRing:
        DrawRingLines(pos, radius + 3.0f * zoom, radius + 5.0f * zoom, 0.0f,
                      360.0f, 36, Fade(accent, 0.42f * alpha));
        break;
    case BladeMasteryNodeShell::BrokenPlate:
        DrawPolyLinesEx(pos, 8, radius + 4.0f * zoom, 22.5f, 1.2f * zoom,
                        Fade(theme.secondary, 0.40f * alpha));
        DrawLineEx({pos.x - radius * 0.62f, pos.y - radius * 0.08f},
                   {pos.x - radius * 0.12f, pos.y + radius * 0.20f},
                   1.5f * zoom, Fade(theme.danger, 0.34f * alpha));
        break;
    case BladeMasteryNodeShell::None:
    default:
        break;
    }
}

} // namespace

Vector2 UISkillSpecRenderer::GetNodeScreenPos(const TalentNode& node,
                                              const SkillSpecView& view) {
    const float spacingX = 85.0f * view.zoom;
    const float spacingY = 70.0f * view.zoom;
    const float finalX = node.x * spacingX;
    const float finalY = node.y * spacingY;

    return {view.center.x + view.offset.x + finalX,
            view.center.y + view.offset.y + finalY};
}

UISkillSpecRenderer::NodeType UISkillSpecRenderer::ClassifyNodeVisual(
    const TalentNode& node, const NodeContractData* contract) {
    if (contract != nullptr) {
        switch (contract->role) {
        case SpecNodeRole::Trigger:
            return NodeType::Trigger;
        case SpecNodeRole::Synergy:
            return NodeType::Synergy;
        case SpecNodeRole::Transmuter:
            return NodeType::Transmuter;
        case SpecNodeRole::Keystone:
            return NodeType::Keystone;
        case SpecNodeRole::Passive:
        default:
            break;
        }
    }

    if (node.max_points == 1) {
        return NodeType::Keystone;
    }
    if (node.max_points <= 4) {
        return NodeType::Modifier;
    }
    return NodeType::Passive;
}

float UISkillSpecRenderer::GetNodeRadius(const TalentNode& node,
                                         const SkillSpecView& view,
                                         const NodeContractData* contract) {
    const float baseRadius = 40.0f * view.zoom;
    switch (ClassifyNodeVisual(node, contract)) {
    case NodeType::Keystone:
        return baseRadius * 1.35f;
    case NodeType::Trigger:
    case NodeType::Transmuter:
        return baseRadius * 1.1f;
    case NodeType::Synergy:
        return baseRadius;
    case NodeType::Modifier:
        return baseRadius * 0.95f;
    case NodeType::Passive:
    default:
        return baseRadius * 0.75f;
    }
}

void UISkillSpecRenderer::Draw(const SkillTreeDefinition* tree,
                               const SpecializedSkill* specialized,
                               const ActiveSkillsComponent* active,
                               const SkillData* skillData,
                               const SkillSpecView& view,
                               uint32_t hoveredNodeId,
                               const std::unordered_set<uint32_t>* excludedNodeIds) {
    DrawBackground(tree, view);
    DrawConnections(tree, specialized, view);
    DrawHub(tree, skillData, view);
    DrawNodes(tree, specialized, active, skillData, view, hoveredNodeId,
              excludedNodeIds);
}

void UISkillSpecRenderer::DrawBackground(const SkillTreeDefinition* tree,
                                         const SkillSpecView& view) {
    const BladeMasteryUIThemeProfile& theme = ResolveThemeProfile(tree);
    const Vector2 treeCenter = {view.center.x + view.offset.x,
                                view.center.y + view.offset.y};

    DrawCircleGradient(static_cast<int>(treeCenter.x), static_cast<int>(treeCenter.y),
                       520.0f * view.zoom,
                       Fade(theme.secondary, 0.10f * view.alpha),
                       Fade(BLANK, 0.0f));
    DrawCircleGradient(GetScreenWidth() / 2, GetScreenHeight() / 2,
                       static_cast<float>(std::max(GetScreenWidth(), GetScreenHeight())),
                       Fade(BLANK, 0.0f), Fade(BLACK, 0.72f * view.alpha));

    switch (theme.background_pattern) {
    case BladeMasteryBackgroundPattern::DiagonalCuts:
        DrawDiagonalCuts(treeCenter, view, theme);
        break;
    case BladeMasteryBackgroundPattern::OrbitArcs:
        DrawOrbitArcs(treeCenter, view, theme);
        break;
    case BladeMasteryBackgroundPattern::BrokenPlate:
        DrawBrokenPlate(treeCenter, view, theme);
        break;
    case BladeMasteryBackgroundPattern::None:
    default:
        break;
    }
}

void UISkillSpecRenderer::DrawHub(const SkillTreeDefinition* tree,
                                  const SkillData* skillData,
                                  const SkillSpecView& view) {
    const BladeMasteryUIThemeProfile& theme = ResolveThemeProfile(tree);
    const Vector2 centerPos = {view.center.x + view.offset.x,
                               view.center.y + view.offset.y};
    const float radius = 55.0f * view.zoom;
    const float pulseSeconds = std::max(0.2f, theme.idle_pulse_seconds);
    const float pulse = 0.5f + 0.5f * sinf(static_cast<float>(GetTime()) * (2.0f * PI / pulseSeconds));

    DrawCircleV(centerPos, radius * 1.12f, Fade(theme.secondary, 0.16f * view.alpha));
    DrawCircleV(centerPos, radius * 0.92f, Fade(BLACK, 0.95f * view.alpha));

    switch (theme.node_shell) {
    case BladeMasteryNodeShell::BeveledDiamond:
        DrawPoly(centerPos, 4, radius * 1.08f, 45.0f,
                 Fade(theme.secondary, 0.16f * view.alpha));
        DrawPolyLinesEx(centerPos, 4, radius * 1.12f, 45.0f, 3.0f * view.zoom,
                        Fade(theme.primary, 0.82f * view.alpha));
        DrawPolyLinesEx(centerPos, 4, radius * 1.30f, 45.0f, 1.2f * view.zoom,
                        Fade(theme.highlight, 0.34f * view.alpha));
        break;
    case BladeMasteryNodeShell::OrbitRing:
        DrawRing(centerPos, radius * 1.02f, radius * 1.16f, 0.0f, 360.0f, 48,
                 Fade(theme.primary, 0.52f * view.alpha));
        DrawRingLines(centerPos, radius * 1.28f, radius * 1.34f, 20.0f, 300.0f,
                      40, Fade(theme.highlight, 0.56f * view.alpha));
        break;
    case BladeMasteryNodeShell::BrokenPlate:
        DrawPoly(centerPos, 8, radius * 1.08f, 22.5f,
                 Fade(theme.secondary, 0.14f * view.alpha));
        DrawPolyLinesEx(centerPos, 8, radius * 1.12f, 22.5f, 2.8f * view.zoom,
                        Fade(theme.primary, 0.72f * view.alpha));
        DrawLineEx({centerPos.x - radius * 0.82f, centerPos.y - radius * 0.10f},
                   {centerPos.x - radius * 0.18f, centerPos.y + radius * 0.20f},
                   2.0f * view.zoom, Fade(theme.danger, 0.40f * view.alpha));
        DrawRingGradient(centerPos, radius * (1.18f + pulse * 0.02f),
                         radius * (1.28f + pulse * 0.12f),
                         Fade(theme.danger, 0.18f * view.alpha),
                         Fade(BLANK, 0.0f));
        break;
    case BladeMasteryNodeShell::None:
    default:
        DrawRing(centerPos, radius * 1.00f, radius * 1.08f, 0.0f, 360.0f, 40,
                 Fade(theme.primary, 0.52f * view.alpha));
        break;
    }

    if (skillData && skillData->icon_id != 0) {
        const Texture2D icon = AssetLoadingSystem::GetTexture(skillData->icon_id);
        if (icon.id != 0) {
            const float iconSize = radius * 1.52f;
            const Rectangle dest = {centerPos.x - iconSize / 2.0f,
                                    centerPos.y - iconSize / 2.0f,
                                    iconSize, iconSize};
            const Rectangle src = {0.0f, 0.0f, static_cast<float>(icon.width),
                                   static_cast<float>(icon.height)};
            DrawTexturePro(icon, src, dest, {0.0f, 0.0f}, 0.0f,
                           Fade(WHITE, view.alpha));
        }
    }
}

void UISkillSpecRenderer::DrawConnections(const SkillTreeDefinition* tree,
                                          const SpecializedSkill* specialized,
                                          const SkillSpecView& view) {
    if (!tree || !specialized) {
        return;
    }

    const BladeMasteryUIThemeProfile& theme = ResolveThemeProfile(tree);
    const float lineThick = 8.0f * view.zoom;
    const Color lockedColor = Fade(Color{78, 82, 96, 255}, 0.95f * view.alpha);
    const Color lockedHighlight = Fade(Color{120, 128, 146, 255}, 0.62f * view.alpha);
    const Color availableColor = Fade(theme.secondary, 0.92f * view.alpha);
    const Color activeColor = Fade(theme.primary, 0.98f * view.alpha);

    auto drawRequirementDots = [&](Vector2 p1, Vector2 p2, bool active,
                                   bool available, int requiredPoints) {
        if (requiredPoints <= 0) {
            return;
        }

        const int dotCount = std::max(1, std::min(requiredPoints, 8));
        const Vector2 delta = Vector2Subtract(p2, p1);
        const float length = Vector2Length(delta);
        if (length < 1.0f) {
            return;
        }

        const Vector2 dir = Vector2Scale(delta, 1.0f / length);
        const Vector2 mid = Vector2Lerp(p1, p2, 0.5f);
        const float spacing = std::max(12.0f * view.zoom, lineThick * 1.35f);
        const float halfSpan = (dotCount - 1) * spacing * 0.5f;

        Color dotColor = lockedHighlight;
        if (available) {
            dotColor = active ? Fade(theme.highlight, 0.98f * view.alpha)
                              : Fade(theme.primary, 0.88f * view.alpha);
        }

        const float dotRadius = std::max(2.4f * view.zoom, lineThick * 0.28f);
        for (int i = 0; i < dotCount; ++i) {
            const float offset = -halfSpan + static_cast<float>(i) * spacing;
            const Vector2 pos = Vector2Add(mid, Vector2Scale(dir, offset));
            DrawCircleV(pos, dotRadius + 1.2f * view.zoom,
                        Fade(BLACK, 0.82f * view.alpha));
            DrawCircleV(pos, dotRadius, dotColor);
        }
    };

    auto drawStyleOverlay = [&](Vector2 p1, Vector2 p2, bool active, bool available) {
        if (!active && !available) {
            return;
        }

        const Vector2 delta = Vector2Subtract(p2, p1);
        const float length = Vector2Length(delta);
        if (length < 1.0f) {
            return;
        }

        const Vector2 dir = Vector2Scale(delta, 1.0f / length);
        const Vector2 mid = Vector2Lerp(p1, p2, 0.5f);
        const Vector2 normal = {-dir.y, dir.x};
        const float strength = active ? 1.0f : 0.65f;

        switch (theme.link_style) {
        case BladeMasteryLinkStyle::SlashTrail:
            DrawLineEx(Vector2Add(mid, Vector2Scale(normal, -8.0f * view.zoom)),
                       Vector2Add(mid, Vector2Scale(normal, 8.0f * view.zoom)),
                       2.0f * view.zoom,
                       Fade(theme.highlight, 0.58f * strength * view.alpha));
            break;
        case BladeMasteryLinkStyle::ArcFlow:
            DrawRingLines(mid, 5.0f * view.zoom, 8.0f * view.zoom,
                          0.0f, 360.0f, 18,
                          Fade(theme.highlight, 0.54f * strength * view.alpha));
            break;
        case BladeMasteryLinkStyle::InwardPulse: {
            const float pulse = 0.4f + 0.6f * sinf(static_cast<float>(GetTime()) *
                                                   (2.0f * PI / std::max(0.2f, theme.idle_pulse_seconds)));
            DrawRingGradient(mid, 4.0f * view.zoom,
                             (9.0f + pulse * 8.0f) * view.zoom,
                             Fade(theme.danger, 0.18f * strength * view.alpha),
                             Fade(BLANK, 0.0f));
            break;
        }
        case BladeMasteryLinkStyle::None:
        default:
            break;
        }
    };

    auto drawLink = [&](Vector2 p1, Vector2 p2, bool active, bool available,
                        int requiredPoints) {
        if (active) {
            DrawLineEx(p1, p2, lineThick + 2.4f * view.zoom,
                       Fade(BLACK, 0.36f * view.alpha));
            DrawLineEx(p1, p2, lineThick, activeColor);
            DrawLineEx(p1, p2, lineThick * 0.32f,
                       Fade(theme.highlight, 0.74f * view.alpha));
        } else if (available) {
            DrawLineEx(p1, p2, lineThick + 1.8f * view.zoom,
                       Fade(BLACK, 0.34f * view.alpha));
            DrawLineEx(p1, p2, lineThick, availableColor);
            DrawLineEx(p1, p2, lineThick * 0.22f,
                       Fade(theme.highlight, 0.28f * view.alpha));
        } else {
            DrawLineEx(p1, p2, lineThick + 2.0f * view.zoom,
                       Fade(BLACK, 0.40f * view.alpha));
            DrawLineEx(p1, p2, lineThick, lockedColor);
            DrawLineEx(p1, p2, lineThick * 0.24f, lockedHighlight);
        }

        drawStyleOverlay(p1, p2, active, available);
        drawRequirementDots(p1, p2, active, available, requiredPoints);
    };

    for (const auto& [id, node] : tree->nodes) {
        const Vector2 targetPos = GetNodeScreenPos(node, view);
        const bool nodeAlloc = specialized->allocated_points.contains(id) &&
                               specialized->allocated_points.at(id) > 0;
        bool hasValidPrereq = false;

        for (const auto& pre : node.prerequisites) {
            const uint32_t preId = pre.node_id;
            if (!tree->nodes.contains(preId)) {
                continue;
            }

            hasValidPrereq = true;
            const Vector2 sourcePos = GetNodeScreenPos(tree->nodes.at(preId), view);
            const int prePts = specialized->allocated_points.contains(preId)
                                   ? specialized->allocated_points.at(preId)
                                   : 0;
            const int requiredPoints = pre.required_points > 0 ? pre.required_points : 1;
            const bool preAlloc = prePts >= requiredPoints;
            drawLink(sourcePos, targetPos, preAlloc && nodeAlloc, preAlloc,
                     requiredPoints);
        }

        if (!hasValidPrereq) {
            const Vector2 sourcePos = {view.center.x + view.offset.x,
                                       view.center.y + view.offset.y};
            drawLink(sourcePos, targetPos, nodeAlloc, true, 0);
        }
    }
}

void UISkillSpecRenderer::DrawNodes(
    const SkillTreeDefinition* tree, const SpecializedSkill* specialized,
    const ActiveSkillsComponent* active, const SkillData* skillData,
    const SkillSpecView& view, uint32_t hoveredNodeId,
    const std::unordered_set<uint32_t>* excludedNodeIds) {
    (void)active;
    if (!tree || !specialized) {
        return;
    }

    const BladeMasteryUIThemeProfile& theme = ResolveThemeProfile(tree);
    const uint32_t skillId = tree->skill_id;
    for (const auto& [id, node] : tree->nodes) {
        const NodeContractData* contract = nullptr;
        if (skillId != INVALID_SKILL_ID) {
            contract = SkillRegistry::Get().GetNodeContract(skillId, id);
        }

        const Vector2 pos = GetNodeScreenPos(node, view);
        const NodeType type = ClassifyNodeVisual(node, contract);
        const float baseRadius = GetNodeRadius(node, view, contract);
        const bool isHovered = (id == hoveredNodeId);
        const float radius = isHovered ? baseRadius * 1.10f : baseRadius;

        const int currentPts = specialized->allocated_points.contains(id)
                                   ? specialized->allocated_points.at(id)
                                   : 0;
        const bool isMaxed = currentPts >= node.max_points;
        const bool isAllocated = currentPts > 0;
        const bool isExcluded = excludedNodeIds && excludedNodeIds->contains(id);
        const bool canUnlock = IsPrerequisiteSatisfiedOr(node, tree, specialized);

        Color edgeColor = canUnlock ? theme.primary : Color{64, 68, 78, 255};
        if (isAllocated) {
            edgeColor = isMaxed ? theme.highlight : theme.primary;
        }
        if (isExcluded && !isAllocated) {
            edgeColor = theme.danger;
        }

        const Color accent = GetLocalNodeAccent(skillData, type, theme);
        const Color borderColor = Fade(edgeColor, view.alpha);
        Color fillColor = Fade(BLACK, 0.90f * view.alpha);
        if (isAllocated) {
            fillColor = Fade(theme.secondary, 0.58f * view.alpha);
        } else if (canUnlock) {
            fillColor = Fade(theme.secondary, 0.28f * view.alpha);
        }

        if (isAllocated) {
            DrawRingGradient(pos, radius + 2.0f * view.zoom,
                             radius + (isMaxed ? 11.0f : 8.0f) * view.zoom,
                             Fade(edgeColor, 0.44f * view.alpha),
                             Fade(BLANK, 0.0f));
        } else if (canUnlock) {
            const float pulse = 0.5f +
                                0.5f * sinf(static_cast<float>(GetTime()) *
                                            (2.0f * PI / std::max(0.2f, theme.idle_pulse_seconds)));
            DrawRingGradient(pos, radius + 2.0f * view.zoom,
                             radius + (5.0f + pulse * 4.0f) * view.zoom,
                             Fade(theme.primary, (0.16f + pulse * 0.10f) * view.alpha),
                             Fade(BLANK, 0.0f));
        }

        DrawNodeShape(pos, radius, type, fillColor, borderColor, view.zoom,
                      isMaxed, theme, accent, view.alpha);

        if (isAllocated && !isMaxed) {
            const float pct = static_cast<float>(currentPts) /
                              static_cast<float>(std::max(1, node.max_points));
            const float innerR = radius * 0.82f * std::clamp(pct, 0.25f, 1.0f);
            const Color fill = Fade(accent, 0.34f * view.alpha);
            switch (type) {
            case NodeType::Keystone:
                DrawPoly(pos, 8, innerR, 22.5f, fill);
                break;
            case NodeType::Trigger:
                DrawPoly(pos, 3, innerR, -90.0f, fill);
                break;
            case NodeType::Transmuter:
                DrawPoly(pos, 4, innerR, 45.0f, fill);
                break;
            case NodeType::Modifier:
            case NodeType::Synergy:
                DrawPoly(pos, 6, innerR, 0.0f, fill);
                break;
            case NodeType::Passive:
            default:
                DrawCircleV(pos, innerR, fill);
                break;
            }
        } else if (isMaxed) {
            const float innerR = radius * 0.82f;
            const Color fill = Fade(theme.highlight, 0.24f * view.alpha);
            switch (type) {
            case NodeType::Keystone:
                DrawPoly(pos, 8, innerR, 22.5f, fill);
                break;
            case NodeType::Trigger:
                DrawPoly(pos, 3, innerR, -90.0f, fill);
                break;
            case NodeType::Transmuter:
                DrawPoly(pos, 4, innerR, 45.0f, fill);
                break;
            case NodeType::Modifier:
            case NodeType::Synergy:
                DrawPoly(pos, 6, innerR, 0.0f, fill);
                break;
            case NodeType::Passive:
            default:
                DrawCircleV(pos, innerR, fill);
                break;
            }
        }

        if (node.icon_id > 0) {
            const Texture2D icon = AssetLoadingSystem::GetTexture(node.icon_id);
            if (icon.id != 0) {
                const float iconSize = radius * 1.34f;
                const Rectangle dest = {pos.x - iconSize / 2.0f,
                                        pos.y - iconSize / 2.0f,
                                        iconSize, iconSize};
                const Rectangle src = {0.0f, 0.0f,
                                       static_cast<float>(icon.width),
                                       static_cast<float>(icon.height)};
                DrawTexturePro(icon, src, dest, {0.0f, 0.0f}, 0.0f,
                               Fade(WHITE, (isAllocated || canUnlock)
                                               ? view.alpha
                                               : 0.52f * view.alpha));
            }
        } else if (isAllocated) {
            DrawCircleV(pos, radius * 0.26f,
                        Fade(accent, 0.80f * view.alpha));
        }

        if (isHovered || isAllocated) {
            const int fontSize = static_cast<int>(20 * view.zoom);
            const char* text = TextFormat("%d/%d", currentPts, node.max_points);
            const Vector2 textSize = MeasureTextEx(UISystem::GetFont(), text,
                                                   static_cast<float>(fontSize), 1.0f);

            const float padding = 4.0f;
            const Rectangle pillHitbox = {
                pos.x - textSize.x / 2.0f - padding,
                pos.y + radius + 14.0f * view.zoom - padding,
                textSize.x + padding * 2.0f,
                textSize.y + padding * 2.0f,
            };

            DrawRectangleRounded(pillHitbox, 0.5f, 4,
                                 Fade(BLACK, 0.74f * view.alpha));
            DrawRectangleRoundedLinesEx(
                pillHitbox, 0.5f, 4, 1.0f,
                Fade(isAllocated ? accent : borderColor, 0.55f * view.alpha));

            const Color textColor = isAllocated
                                        ? (isMaxed ? theme.highlight : WHITE)
                                        : (canUnlock ? Fade(WHITE, 0.92f)
                                                     : Fade(WHITE, 0.42f));
            DrawTextEx(UISystem::GetFont(), text,
                       {pillHitbox.x + padding, pillHitbox.y + padding},
                       static_cast<float>(fontSize), 1.0f,
                       Fade(textColor, view.alpha));
        }

        if (isExcluded && !isAllocated) {
            const float crossThickness = 2.0f * view.zoom;
            DrawLineEx({pos.x - radius * 0.75f, pos.y - radius * 0.75f},
                       {pos.x + radius * 0.75f, pos.y + radius * 0.75f},
                       crossThickness, Fade(theme.danger, 0.86f * view.alpha));
            DrawLineEx({pos.x - radius * 0.75f, pos.y + radius * 0.75f},
                       {pos.x + radius * 0.75f, pos.y - radius * 0.75f},
                       crossThickness, Fade(theme.danger, 0.86f * view.alpha));
        }
    }
}

} // namespace NoMoreDay
