#include "game/application/ui/UISkillTalentTree.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UISkillHub.hpp"
#include "game/application/ui/UISkillSpecRenderer.hpp"
#include "game/application/ui/TooltipController.hpp"
#include "game/application/ui/BladeMasteryUITheme.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/contracts/impl/CombatAntiMeta.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "core/logging/Logger.hpp"
#include "raymath.h"
#include <array>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <vector>

namespace NoMoreDay {

namespace {

constexpr float kNodeSpacingX = 85.0f;
constexpr float kNodeSpacingY = 70.0f;
constexpr float kTooltipBaseHeight = 280.0f;
constexpr float kTooltipDescTop = 108.0f;
constexpr float kTooltipDescMinHeight = 56.0f;
constexpr float kTooltipFooterLineHeight = 24.0f;
constexpr float kTooltipFooterTopPad = 12.0f;
constexpr float kTooltipFooterBottomPad = 16.0f;
constexpr float kTooltipFooterGap = 12.0f;

struct TooltipBadgeSpec {
    std::string text;
    Color fill;
    Color outline;
};

struct TooltipKeywordSpec {
    const char* text;
    Color color;
};

struct TreeFeedbackState {
    Color ringColor;
    Color accentColor;
    float ringThickness = 0.0f;
    float ringRadiusScale = 1.0f;
    bool drawAccent = false;
};

SkillTreeUI::Vec2 ScreenToTreeCoords(const Vector2& mousePixelPos,
                                     const SkillSpecView& view) {
    return {
        (mousePixelPos.x - view.center.x - view.offset.x) / (kNodeSpacingX * view.zoom),
        (mousePixelPos.y - view.center.y - view.offset.y) / (kNodeSpacingY * view.zoom),
    };
}

const char* NodeRoleToText(SpecNodeRole role) {
    switch (role) {
    case SpecNodeRole::Keystone: return "核心";
    case SpecNodeRole::Trigger: return "触发";
    case SpecNodeRole::Synergy: return "联动";
    case SpecNodeRole::Transmuter: return "转化";
    default: return "被动";
    }
}

const char* ScopePolicyToText(ScopePolicy scope) {
    switch (scope) {
    case ScopePolicy::GlobalAlways: return "全局常驻";
    case ScopePolicy::GlobalWhileBuffActive: return "增益期间全局";
    default: return "仅本技能";
    }
}

bool IsPrerequisiteSatisfiedOr(const TalentNode& node, const SkillTreeDefinition& tree,
                               const SpecializedSkill& specialized) {
    if (node.prerequisites.empty()) {
        return true;
    }

    bool hasValidPrereq = false;
    for (const auto& pre : node.prerequisites) {
        const uint32_t preId = pre.node_id;
        if (preId == 0 || !tree.nodes.contains(preId)) {
            continue;
        }
        hasValidPrereq = true;
        int prePts = specialized.allocated_points.contains(preId) ? specialized.allocated_points.at(preId) : 0;
        const int requiredPoints = pre.required_points > 0 ? pre.required_points : 1;
        if (prePts >= requiredPoints) {
            return true;
        }
    }
    return !hasValidPrereq;
}

TooltipBadgeSpec BuildRoleBadge(const NodeContractData& nodeContract) {
    return {
        TextFormat("定位 %s", NodeRoleToText(nodeContract.role)),
        Fade(ORANGE, 0.18f),
        Fade(ORANGE, 0.90f),
    };
}

TooltipBadgeSpec DrawRoleBadge(const NodeContractData& nodeContract) {
    return BuildRoleBadge(nodeContract);
}

TooltipBadgeSpec BuildScopeBadge(const NodeContractData& nodeContract) {
    return {
        TextFormat("范围 %s", ScopePolicyToText(nodeContract.scope_policy)),
        Fade(SKYBLUE, 0.18f),
        Fade(SKYBLUE, 0.90f),
    };
}

TooltipBadgeSpec DrawScopeBadge(const NodeContractData& nodeContract) {
    return BuildScopeBadge(nodeContract);
}

TooltipBadgeSpec BuildExclusionBadge(const NodeContractData& nodeContract) {
    return {
        TextFormat("互斥 G%u", static_cast<unsigned int>(nodeContract.keystone_exclusion_group)),
        Fade(RED, 0.18f),
        Fade(RED, 0.92f),
    };
}

Color ResolveTooltipBadgeTextColor(const TooltipBadgeSpec& badge) {
    (void)badge;
    return {244, 238, 228, 255};
}

Color ResolveTooltipTitleColor(const BladeMasteryUIThemeProfile& masteryTheme) {
    return {
        static_cast<unsigned char>(std::max<int>(masteryTheme.highlight.r, 232)),
        static_cast<unsigned char>(std::max<int>(masteryTheme.highlight.g, 228)),
        static_cast<unsigned char>(std::max<int>(masteryTheme.highlight.b, 220)),
        255,
    };
}

void DrawTooltipBadgeChip(const Font& font, const TooltipBadgeSpec& badge, float x, float y,
                          float fontSize, float alpha, float scale) {
    const float sFont = fontSize * scale;
    const float sSpacing = 1.0f * scale;
    const float paddingX = 12.0f;
    const float chipH = 28.0f;
    const float textW = IsFontValid(font)
        ? MeasureTextEx(font, badge.text.c_str(), sFont, sSpacing).x / scale
        : static_cast<float>(MeasureText(badge.text.c_str(), static_cast<int>(sFont))) / scale;
    const Rectangle chip = {x, y, textW + paddingX * 2.0f, chipH};
    DrawRectangleRounded({chip.x * scale, chip.y * scale, chip.width * scale, chip.height * scale},
                         0.24f, 8, Fade(badge.fill, alpha));
    DrawRectangleRoundedLinesEx(
        {chip.x * scale, chip.y * scale, chip.width * scale, chip.height * scale},
        0.24f, 8, 1.0f, Fade(badge.outline, alpha));
    const Color textColor = ResolveTooltipBadgeTextColor(badge);
    UISystem::DrawTextUI(badge.text.c_str(), x + paddingX, y + 4.0f, fontSize,
                         textColor, alpha);
}

void DrawTooltipHeader(const BladeMasteryUIThemeProfile& masteryTheme, const TalentNode& hoveredNode,
                       float x, float y, float width, float alpha, float scale) {
    const float headerH = 52.0f;
    DrawRectangleRec({x * scale, y * scale, width * scale, headerH * scale},
                     Fade(masteryTheme.primary, 0.16f * alpha));
    DrawRectangleRec({(x + 8.0f) * scale, (y + headerH - 6.0f) * scale,
                      (width - 16.0f) * scale, 2.0f * scale},
                     Fade(masteryTheme.highlight, 0.92f * alpha));
    DrawRectangleRec({(x + 16.0f) * scale, (y + 10.0f) * scale,
                      52.0f * scale, 4.0f * scale},
                     Fade(masteryTheme.secondary, 0.72f * alpha));
    const Color titleColor = ResolveTooltipTitleColor(masteryTheme);
    UISystem::DrawTextUI(hoveredNode.name_key.c_str(), x + 20.0f, y + 16.0f, 28.0f,
                         titleColor, alpha);
}

int MatchTooltipKeyword(const char* text, Color& keywordColor, int& keywordBytes) {
    static constexpr std::array<TooltipKeywordSpec, 14> kTooltipKeywords = {{
        {"冷却", GOLD},
        {"重置", GOLD},
        {"暴击", ORANGE},
        {"剑意", SKYBLUE},
        {"剑步", SKYBLUE},
        {"触发", GREEN},
        {"联动", GREEN},
        {"转化", VIOLET},
        {"全局", YELLOW},
        {"增益", LIME},
        {"护盾", BLUE},
        {"位移", PINK},
        {"代价", RED},
        {"收益", GREEN},
    }};

    int bestBytes = 0;
    Color bestColor = WHITE;
    for (const auto& keyword : kTooltipKeywords) {
        const int byteCount = static_cast<int>(std::char_traits<char>::length(keyword.text));
        if (byteCount <= bestBytes) {
            continue;
        }
        if (std::char_traits<char>::compare(text, keyword.text, byteCount) == 0) {
            bestBytes = byteCount;
            bestColor = keyword.color;
        }
    }

    keywordColor = bestColor;
    keywordBytes = bestBytes;
    return bestBytes;
}

void FlushTooltipLine(const Font& font,
                      const std::vector<std::pair<std::string, Color>>& segments,
                      float x, float y, float fontSize, float alpha, float scale) {
    if (segments.empty()) {
        return;
    }

    const float sFont = fontSize * scale;
    const float sSpacing = 1.0f * scale;
    float cursorX = x * scale;
    const float drawY = y * scale;

    for (const auto& segment : segments) {
        if (segment.first.empty()) {
            continue;
        }
        const Color color = Fade(segment.second, alpha);
        if (IsFontValid(font)) {
            DrawTextEx(font, segment.first.c_str(), {cursorX, drawY}, sFont, sSpacing, color);
            cursorX += MeasureTextEx(font, segment.first.c_str(), sFont, sSpacing).x;
        } else {
            DrawText(segment.first.c_str(), static_cast<int>(cursorX), static_cast<int>(drawY),
                     static_cast<int>(sFont), color);
            cursorX += static_cast<float>(MeasureText(segment.first.c_str(), static_cast<int>(sFont)));
        }
    }
}

void DrawKeywordHighlights(const Font& font, const char* text, float x, float y, float width,
                           float height, float fontSize, Color baseColor, float alpha,
                           float scale) {
    if (!text || text[0] == '\0' || width <= 0.0f || height <= 0.0f) {
        return;
    }

    const float sFont = fontSize * scale;
    const float sSpacing = 1.0f * scale;
    const float maxWidth = width * scale;
    const float maxY = (y + height) * scale;
    const float lineHeight = (fontSize + 6.0f) * scale;

    std::vector<std::pair<std::string, Color>> lineSegments;
    float lineWidth = 0.0f;
    float drawY = y;
    const char* ptr = text;

    auto measureSegment = [&](const std::string& segment) {
        if (segment.empty()) {
            return 0.0f;
        }
        if (IsFontValid(font)) {
            return MeasureTextEx(font, segment.c_str(), sFont, sSpacing).x;
        }
        return static_cast<float>(MeasureText(segment.c_str(), static_cast<int>(sFont)));
    };

    while (*ptr != '\0') {
        int bytes = 0;
        const int cp = GetCodepoint(ptr, &bytes);
        if (bytes <= 0) {
            break;
        }

        if (cp == '\r') {
            ptr += bytes;
            continue;
        }
        if (cp == '\n') {
            if (drawY * scale + lineHeight > maxY) {
                break;
            }
            FlushTooltipLine(font, lineSegments, x, drawY, fontSize, alpha, scale);
            lineSegments.clear();
            lineWidth = 0.0f;
            drawY += fontSize + 6.0f;
            ptr += bytes;
            continue;
        }

        Color segmentColor = baseColor;
        int keywordBytes = 0;
        std::string segmentText;
        if (MatchTooltipKeyword(ptr, segmentColor, keywordBytes) > 0) {
            segmentText.assign(ptr, ptr + keywordBytes);
            bytes = keywordBytes;
        } else {
            int glyphBytes = 0;
            const char* glyphPtr = CodepointToUTF8(cp, &glyphBytes);
            segmentText.assign(glyphPtr, glyphBytes);
        }

        const float segmentWidth = measureSegment(segmentText);
        if (!lineSegments.empty() && lineWidth + segmentWidth > maxWidth) {
            if (drawY * scale + lineHeight > maxY) {
                break;
            }
            FlushTooltipLine(font, lineSegments, x, drawY, fontSize, alpha, scale);
            lineSegments.clear();
            lineWidth = 0.0f;
            drawY += fontSize + 6.0f;
            if (drawY * scale + lineHeight > maxY) {
                break;
            }
        }

        lineSegments.emplace_back(segmentText, segmentColor);
        lineWidth += segmentWidth;
        ptr += bytes;
    }

    if (!lineSegments.empty() && drawY * scale + lineHeight <= maxY) {
        FlushTooltipLine(font, lineSegments, x, drawY, fontSize, alpha, scale);
    }
}

const char* StatTypeToLabel(StatType type) {
    switch (type) {
    case StatType::CooldownReduction: return "冷却缩减";
    case StatType::ResourceCostReduction: return "资源消耗降低";
    case StatType::AreaScale: return "技能范围";
    case StatType::ProjectileSpeed: return "投射物速度";
    case StatType::DurationScale: return "持续时间";
    case StatType::GlobalDamageReduction: return "伤害减免";
    case StatType::CritChance: return "暴击率";
    case StatType::CritDamage: return "暴击伤害";
    case StatType::AttackSpeed: return "攻击速度";
    case StatType::CastSpeed: return "施法速度";
    case StatType::MoveSpeed: return "移动速度";
    case StatType::MaxHealth: return "最大生命值";
    case StatType::MaxMana: return "最大法力值";
    case StatType::MaxBarrier: return "最大护盾";
    case StatType::PhysicalDamage: return "物理伤害";
    case StatType::FireDamage: return "火焰伤害";
    case StatType::ColdDamage: return "冰霜伤害";
    case StatType::LightningDamage: return "闪电伤害";
    case StatType::PoisonDamage: return "毒素伤害";
    case StatType::ShadowDamage: return "暗影伤害";
    default: return nullptr;
    }
}

int GetLinePriority(StatType type) {
    switch (type) {
    case StatType::PhysicalDamage:
    case StatType::FireDamage:
    case StatType::ColdDamage:
    case StatType::LightningDamage:
    case StatType::PoisonDamage:
    case StatType::ShadowDamage:
        return 0;
    case StatType::DurationScale:
        return 1;
    case StatType::AttackSpeed:
    case StatType::CastSpeed:
        return 2;
    case StatType::AreaScale:
        return 3;
    case StatType::CooldownReduction:
    case StatType::ResourceCostReduction:
        return 4;
    default:
        return 5;
    }
}

std::vector<std::pair<std::string, Color>> BuildNodeQuantitativeLines(
    const TalentNode& node,
    const SpecializedSkill& specialized,
    uint32_t hoveredNodeId) {
    
    struct LineInfo {
        std::string text;
        Color color;
        int priority;
    };
    std::vector<LineInfo> rawLines;
    
    int currentPts = 0;
    if (specialized.allocated_points.contains(hoveredNodeId)) {
        currentPts = specialized.allocated_points.at(hoveredNodeId);
    }
    
    // Preview points:
    // If uninvested (0 points), show 1pt preview as requested.
    // If already invested, show CURRENT points total.
    int displayPoints = currentPts > 0 ? currentPts : 1;
    
    // 1. auto-generated lines from stable stat_modifiers
    for (const auto& mod : node.stat_modifiers) {
        float totalVal = mod.value * displayPoints;
        const char* label = StatTypeToLabel(mod.type);
        if (!label) continue;
        
        std::string sign = totalVal >= 0 ? "+" : "";
        std::string text;
        if (mod.mode == ModifierMode::PercentAdd || mod.mode == ModifierMode::PercentMult) {
             text = TextFormat("%s%.0f%% %s", sign.c_str(), totalVal, label);
        } else {
             text = TextFormat("%s%.1f %s", sign.c_str(), totalVal, label);
        }
        rawLines.push_back({text, SKYBLUE, GetLinePriority(mod.type)});
    }
    
    // 2. auto-generated lines from damage_modifiers
    for (const auto& mod : node.damage_modifiers) {
         float totalVal = mod.value * displayPoints;
         const char* locLabel = "伤害";
         if (HasTag(mod.source_tag, Tag::Physical)) locLabel = "物理伤害";
         else if (HasTag(mod.source_tag, Tag::Fire)) locLabel = "火焰伤害";
         else if (HasTag(mod.source_tag, Tag::Cold)) locLabel = "冰霜伤害";
         else if (HasTag(mod.source_tag, Tag::Lightning)) locLabel = "闪电伤害";
         else if (HasTag(mod.source_tag, Tag::Poison)) locLabel = "毒素伤害";
         else if (HasTag(mod.source_tag, Tag::Shadow)) locLabel = "暗影伤害";

         std::string sign = totalVal >= 0 ? "+" : "";
         std::string text;
         if (mod.type == ModifierType::Increased || mod.type == ModifierType::More) {
               text = TextFormat("%s%.0f%% %s", sign.c_str(), totalVal, locLabel);
         } else {
              text = TextFormat("%s%.1f %s", sign.c_str(), totalVal, locLabel);
         }
         rawLines.push_back({text, ORANGE, 0}); // Damage is priority 0
    }

    // 3. append explicit node.display_lines metadata-driven entries
    for (const auto& dline : node.display_lines) {
        float val = dline.base_value + dline.per_point * displayPoints;
        std::string sign = val >= 0 ? "+" : "";
        std::string text;
        if (dline.is_percent) {
            text = TextFormat("%s%.0f%% %s%s", sign.c_str(), val, dline.label.c_str(), dline.suffix.c_str());
        } else {
            text = TextFormat("%s%.1f %s%s", sign.c_str(), val, dline.label.c_str(), dline.suffix.c_str());
        }
        
        // Priority mapping for explicit lines (heuristic based on label)
        int priority = 5;
        if (dline.label.find("伤害") != std::string::npos) priority = 0;
        else if (dline.label.find("持续") != std::string::npos) priority = 1;
        else if (dline.label.find("频率") != std::string::npos || dline.label.find("速度") != std::string::npos) priority = 2;
        else if (dline.label.find("范围") != std::string::npos) priority = 3;
        else if (dline.label.find("消耗") != std::string::npos || dline.label.find("冷却") != std::string::npos) priority = 4;

        rawLines.push_back({text, GOLD, priority});
    }

    // Dedupe equivalent lines by label/text
    std::unordered_set<std::string> seen;
    auto it = std::remove_if(rawLines.begin(), rawLines.end(), [&](const auto& info) {
        if (seen.contains(info.text)) return true;
        seen.insert(info.text);
        return false;
    });
    rawLines.erase(it, rawLines.end());

    // Sort by priority
    std::stable_sort(rawLines.begin(), rawLines.end(), [](const auto& a, const auto& b) {
        return a.priority < b.priority;
    });

    std::vector<std::pair<std::string, Color>> result;
    for (const auto& info : rawLines) {
        result.emplace_back(info.text, info.color);
    }
    return result;
}

TreeFeedbackState BuildTreeFeedbackState(bool hoveredNodeExcluded,
                                         const BladeMasteryUIThemeProfile& masteryTheme,
                                         float alpha) {
    TreeFeedbackState state;
    state.ringColor = hoveredNodeExcluded ? masteryTheme.danger : masteryTheme.highlight;
    state.accentColor = hoveredNodeExcluded ? masteryTheme.danger : masteryTheme.secondary;
    state.ringThickness = hoveredNodeExcluded ? 3.0f : 2.0f;
    state.ringRadiusScale = hoveredNodeExcluded ? 1.26f : 1.18f;
    state.drawAccent = alpha > 0.0f;
    return state;
}

void DrawWrappedTextUI(const Font& font, const char* text, float x, float y, float width, float height,
                       float fontSize, Color color, float alpha, float scale) {
    if (!text || text[0] == '\0' || width <= 0.0f || height <= 0.0f) {
        return;
    }

    const float sX = x * scale;
    const float sY = y * scale;
    const float sWidth = width * scale;
    const float sMaxY = (y + height) * scale;
    const float sFont = fontSize * scale;
    const float sSpacing = 1.0f * scale;
    const float sLineH = sFont + 4.0f * scale;

    std::vector<std::string> lines;
    std::string currentLine;
    const char* ptr = text;

    while (*ptr != '\0') {
        int bytes = 0;
        int cp = GetCodepoint(ptr, &bytes);
        if (bytes <= 0) {
            break;
        }

        if (cp == '\r') {
            ptr += bytes;
            continue;
        }
        if (cp == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
            ptr += bytes;
            continue;
        }

        int glyphBytes = 0;
        const char* glyphPtr = CodepointToUTF8(cp, &glyphBytes);
        std::string glyph(glyphPtr, glyphBytes);
        std::string testLine = currentLine + glyph;

        const float testW = IsFontValid(font)
            ? MeasureTextEx(font, testLine.c_str(), sFont, sSpacing).x
            : static_cast<float>(MeasureText(testLine.c_str(), static_cast<int>(sFont)));

        if (testW <= sWidth || currentLine.empty()) {
            currentLine = std::move(testLine);
        } else {
            lines.push_back(currentLine);
            currentLine = std::move(glyph);
        }

        ptr += bytes;
    }

    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }

    float drawY = sY;
    const Color finalColor = Fade(color, alpha);
    for (const auto& line : lines) {
        if (drawY + sLineH > sMaxY) {
            break;
        }
        if (IsFontValid(font)) {
            DrawTextEx(font, line.c_str(), {sX, drawY}, sFont, sSpacing, finalColor);
        } else {
            DrawText(line.c_str(), static_cast<int>(sX), static_cast<int>(drawY),
                     static_cast<int>(sFont), finalColor);
        }
        drawY += sLineH;
    }
}

} // namespace

SkillTreeUI::TooltipLayoutMetrics SkillTreeUI::ComputeTooltipLayoutMetrics(
    float tooltipHeight, std::size_t quantitativeLineCount, std::size_t footerLineCount) {
    TooltipLayoutMetrics metrics;
    metrics.footerGap = kTooltipFooterGap;
    metrics.descriptionTop = kTooltipDescTop;

    metrics.quantitativeHeight = quantitativeLineCount == 0 ? 0.0f
        : (kTooltipFooterTopPad + kTooltipFooterLineHeight * static_cast<float>(quantitativeLineCount));

    metrics.footerHeight = footerLineCount == 0 ? 0.0f
        : (kTooltipFooterTopPad + kTooltipFooterLineHeight * static_cast<float>(footerLineCount));

    const float minimumHeight = metrics.descriptionTop + kTooltipDescMinHeight +
        metrics.quantitativeHeight + metrics.footerGap + metrics.footerHeight + kTooltipFooterBottomPad;

    metrics.tooltipHeight = std::max(tooltipHeight, minimumHeight);
    metrics.footerTop = metrics.tooltipHeight - kTooltipFooterBottomPad - metrics.footerHeight;
    metrics.quantitativeTop = metrics.footerTop - metrics.quantitativeHeight;
    metrics.descriptionHeight = std::max(0.0f,
        metrics.quantitativeTop - metrics.footerGap - metrics.descriptionTop);
    metrics.descriptionBottom = metrics.descriptionTop + metrics.descriptionHeight;
    return metrics;
}

void SkillTreeUI::Draw(entt::registry& registry, entt::entity player,
                       uint32_t skillId, float alpha) {
    if (alpha <= 0.0f) return;

    auto& skillRegistry = SkillRegistry::Get();
    const auto* skillData = skillRegistry.GetSkill(skillId);
    auto* mutableTree = skillRegistry.GetMutableSkillTree(skillId);
    const auto* tree = mutableTree;
    if (!skillData || !tree) return;
    const BladeMasteryUIThemeProfile& masteryTheme =
        GetBladeMasteryUIThemeProfile(tree->mastery_id);

    auto* active = registry.try_get<ActiveSkillsComponent>(player);
    if (!active) return;

    const SpecializedSkill* specialized = nullptr;
    for (const auto& s : active->specialized_slots) {
        if (s.skill_id == skillId) {
            specialized = &s;
            break;
        }
    }
    if (!specialized) return;

    // --- Metrics (ALL IN LOGIC UNITS 2560x1440) ---
    float logicW = UI_REF_WIDTH;
    float logicH = UI_REF_HEIGHT;
    float panelW = 1800.0f; 
    float panelH = 1100.0f;
    float startX = (logicW - panelW) / 2.0f;
    float startY = (logicH - panelH) / 2.0f;

    Vector2 mouseLogicPos = UISystem::GetMousePositionLogic();
    float scale = UISystem::GetScaleFactor();

    // Draw Background
    DrawRectangleRec({startX * scale, startY * scale, panelW * scale, panelH * scale},
                     Fade(masteryTheme.secondary, 0.12f * alpha));
    DrawRectangleRec({(startX + 6.0f) * scale, (startY + 6.0f) * scale,
                      (panelW - 12.0f) * scale, (panelH - 12.0f) * scale},
                     Fade(BLACK, 0.94f * alpha));
    DrawRectangleLinesEx({startX * scale, startY * scale, panelW * scale, panelH * scale},
                         2.0f, Fade(masteryTheme.primary, alpha));
    DrawRectangleLinesEx({(startX + 8.0f) * scale, (startY + 8.0f) * scale,
                          (panelW - 16.0f) * scale, (panelH - 16.0f) * scale},
                         1.0f, Fade(masteryTheme.secondary, 0.8f * alpha));

    switch (masteryTheme.background_pattern) {
    case BladeMasteryBackgroundPattern::DiagonalCuts:
        DrawLineEx({(startX + 36.0f) * scale, (startY + 108.0f) * scale},
                   {(startX + 320.0f) * scale, (startY + 34.0f) * scale},
                   2.0f * scale, Fade(masteryTheme.highlight, 0.18f * alpha));
        break;
    case BladeMasteryBackgroundPattern::OrbitArcs:
        DrawRingLines({(startX + panelW - 150.0f) * scale, (startY + 82.0f) * scale},
                      18.0f * scale, 40.0f * scale, 210.0f, 30.0f, 24,
                      Fade(masteryTheme.highlight, 0.20f * alpha));
        break;
    case BladeMasteryBackgroundPattern::BrokenPlate:
        DrawLineEx({(startX + 42.0f) * scale, (startY + 70.0f) * scale},
                   {(startX + 164.0f) * scale, (startY + 104.0f) * scale},
                   2.0f * scale, Fade(masteryTheme.danger, 0.16f * alpha));
        break;
    case BladeMasteryBackgroundPattern::None:
    default:
        break;
    }

    // Reset view if skill changed
    if (skillId != m_lastSkillId) {
        m_viewOffset = { 0, 0 }; // Center (0,0)
        m_viewZoom = 1.0f;
        m_lastSkillId = skillId;
        m_layoutEditMode = false;
        m_draggingNodeId = 0;
    }

    // --- Panning & Zooming Interaction ---
    Rectangle viewBoundsLogic = { startX + 20, startY + 120, panelW - 40, panelH - 140 };
    bool mouseInView = CheckCollisionPointRec(mouseLogicPos, viewBoundsLogic);

    if (mouseInView) {
        // Panning with Right Mouse Button
        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            m_viewOffset.x += (mouseLogicPos.x - m_lastMouseLogicPos.x);
            m_viewOffset.y += (mouseLogicPos.y - m_lastMouseLogicPos.y);
        }

        // Zooming with Mouse Wheel
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            m_viewZoom += wheel * 0.1f;
            m_viewZoom = std::clamp(m_viewZoom, 0.4f, 2.5f);
        }
    }
    m_lastMouseLogicPos.x = mouseLogicPos.x;
    m_lastMouseLogicPos.y = mouseLogicPos.y;

    // Header & Points
    UISystem::DrawTextUI(TextFormat("%s - 专精天赋", skillData->name_key.c_str()),
                         startX + 40, startY + 30, 40, masteryTheme.highlight, alpha);
    UISystem::DrawTextUI(TextFormat("可用点数: %d", active->available_talent_points),
                         startX + 40, startY + 80, 24, masteryTheme.primary, alpha);
    
    Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);

    // Reset Button
    Rectangle resetRectLogic = {startX + 250, startY + 75, 120, 40};
    bool resetHover = CheckCollisionPointRec(mouseLogicPos, resetRectLogic);
    
    UIRenderer::DrawButton(UISystem::GetFont(), rectTex, resetRectLogic, "重置天赋",
                           20, WHITE,
                           resetHover ? masteryTheme.danger : masteryTheme.secondary,
                           resetHover, resetHover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);

    if (resetHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        SkillSystem::ResetTalents(registry, player, skillId);
    }

    Rectangle editRectLogic = {startX + 390, startY + 75, 140, 40};
    bool editHover = CheckCollisionPointRec(mouseLogicPos, editRectLogic);
    const bool editPressed = editHover && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    UIRenderer::DrawButton(UISystem::GetFont(), rectTex, editRectLogic,
                           m_layoutEditMode ? "保存布局" : "编辑布局",
                            20, WHITE,
                            m_layoutEditMode ? masteryTheme.primary
                                                          : masteryTheme.secondary,
                            editHover, editPressed, alpha);

    if (editHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (m_layoutEditMode) {
            const bool saved = skillRegistry.SaveSkillTreeLayout(skillId);
            LOG_INFO("Skill tree layout save for skill {}: {}", skillId,
                     saved ? "success" : "failed");
            m_draggingNodeId = 0;
        }
        m_layoutEditMode = !m_layoutEditMode;
    }

    UISystem::DrawTextUI(
        m_layoutEditMode
            ? "编辑布局: 左键拖动节点, 再点[保存布局]写回JSON"
            : "右键拖拽平移, 滚轮缩放",
        startX + panelW - 520, startY + 85, 20, masteryTheme.secondary,
        alpha * 0.7f);
    
    // Back Button
    Rectangle backRectLogic = {startX + panelW - 150, startY + 30, 120, 50};
    bool backHover = CheckCollisionPointRec(mouseLogicPos, backRectLogic);
    
    UIRenderer::DrawButton(UISystem::GetFont(), rectTex, backRectLogic, "返回", 22,
                           WHITE, masteryTheme.primary, backHover,
                           backHover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);

    if (backHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID;
        return;
    }

    // SkillSpecView view setup
    SkillSpecView view;
    float centerX = startX + panelW / 2.0f;
    float centerY = startY + panelH / 2.0f;
    
    view.center = { centerX * scale, centerY * scale };
    view.offset = { m_viewOffset.x * scale, m_viewOffset.y * scale };
    view.zoom = m_viewZoom * scale;
    view.alpha = alpha;  

    // --- Interaction Logic (Pre-Calculation) ---
    Vector2 mousePixelPos = GetMousePosition(); // Raylib raw mouse pos
    uint32_t targetNodeId = 0;
    uint32_t hoveredNodeId = 0;
    const TalentNode* hoveredNode = nullptr;
    std::unordered_set<uint32_t> excludedNodeIds;
    for (const auto& [id, node] : tree->nodes) {
        (void)node;
        if (SkillSystem::IsNodeExcludedByMutualKeystone(registry, player, skillId, id)) {
            excludedNodeIds.insert(id);
        }
    }

    const Vector2 hubCenter = { view.center.x + view.offset.x, view.center.y + view.offset.y };
    const float hubHoverRadius = 55.0f * view.zoom * 1.25f;
    const Vector2 hubCenterLogic = { centerX + m_viewOffset.x, centerY + m_viewOffset.y };
    const float hubHoverRadiusLogic = 55.0f * m_viewZoom * 1.25f;
    const bool hubHovered = mouseInView &&
                            (CheckCollisionPointCircle(mousePixelPos, hubCenter, hubHoverRadius) ||
                             CheckCollisionPointCircle(mouseLogicPos, hubCenterLogic, hubHoverRadiusLogic));
    if (hubHovered) {
        // U8: the hovered-skill channel routes through the bound tooltip
        // controller (was the State.hoveredSkillId write).
        if (m_tooltip != nullptr) {
            m_tooltip->SetHoveredSkill(skillId);
        }
    }

    for (const auto& [id, node] : tree->nodes) {
        const NodeContractData* nodeContract =
            SkillRegistry::Get().GetNodeContract(skillId, id);
        const auto nodeVisual =
            UISkillSpecRenderer::ClassifyNodeVisual(node, nodeContract);
        Vector2 pos = UISkillSpecRenderer::GetNodeScreenPos(node, view);
        float r = UISkillSpecRenderer::GetNodeRadius(node, view, nodeContract);
        (void)nodeVisual;

        if (CheckCollisionPointCircle(mousePixelPos, pos, r) && mouseInView) {
            hoveredNodeId = id;
            hoveredNode = &node;

            if (m_layoutEditMode && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                m_draggingNodeId = id;
                const Vec2 mouseTreePos = ScreenToTreeCoords(mousePixelPos, view);
                m_dragNodeOffset = {
                    node.x - mouseTreePos.x,
                    node.y - mouseTreePos.y,
                };
            } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                // Logic check
                int currentPts = specialized->allocated_points.contains(id) ? specialized->allocated_points.at(id) : 0;
                bool isMaxed = currentPts >= node.max_points;
                
                // Prereq check
                bool canUnlock = IsPrerequisiteSatisfiedOr(node, *tree, *specialized);
                const bool canSwapExcluded = excludedNodeIds.contains(id);
                
                if (canUnlock && !isMaxed &&
                    (active->available_talent_points > 0 || canSwapExcluded)) {
                     targetNodeId = id;
                }
            }
            // Only handle one hover
            break; 
        }
    }

    if (m_layoutEditMode && m_draggingNodeId != 0) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && mutableTree != nullptr &&
            mutableTree->nodes.contains(m_draggingNodeId)) {
            const Vec2 mouseTreePos = ScreenToTreeCoords(mousePixelPos, view);
            auto& dragged = mutableTree->nodes.at(m_draggingNodeId);
            dragged.x = mouseTreePos.x + m_dragNodeOffset.x;
            dragged.y = mouseTreePos.y + m_dragNodeOffset.y;
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            m_draggingNodeId = 0;
        }
    }

    // --- Scissor Mode & Render Content ---
    BeginScissorMode((int)(viewBoundsLogic.x * scale), (int)(viewBoundsLogic.y * scale), 
                     (int)(viewBoundsLogic.width * scale), (int)(viewBoundsLogic.height * scale));

    UISkillSpecRenderer::Draw(tree, specialized, active, skillData, view, hoveredNodeId, &excludedNodeIds);

    if (hoveredNode != nullptr) {
        const NodeContractData* nodeContract = SkillRegistry::Get().GetNodeContract(skillId, hoveredNodeId);
        const TreeFeedbackState feedbackState =
            BuildTreeFeedbackState(excludedNodeIds.contains(hoveredNodeId), masteryTheme, alpha);
        const float nodeRadius = UISkillSpecRenderer::GetNodeRadius(*hoveredNode, view, nodeContract);
        const Vector2 nodeCenter = UISkillSpecRenderer::GetNodeScreenPos(*hoveredNode, view);
        DrawRingLines(nodeCenter, nodeRadius * feedbackState.ringRadiusScale,
                      nodeRadius * feedbackState.ringRadiusScale + feedbackState.ringThickness,
                      0.0f, 360.0f, 32, Fade(feedbackState.ringColor, alpha * 0.95f));
        if (feedbackState.drawAccent) {
            DrawCircleLinesV(nodeCenter, nodeRadius * 0.68f,
                             Fade(feedbackState.accentColor, alpha * 0.75f));
        }
    }

    EndScissorMode();

    // --- Tooltip & Actions ---
    if (hoveredNode) {
        const NodeContractData* nodeContract = SkillRegistry::Get().GetNodeContract(skillId, hoveredNodeId);
        // Tooltip
        float tx = mouseLogicPos.x + 30;
        float ty = mouseLogicPos.y + 30;
        float tw = 440;
        float th = kTooltipBaseHeight;

        std::vector<TooltipBadgeSpec> badges;
        if (nodeContract) {
            badges.push_back(DrawRoleBadge(*nodeContract));
            badges.push_back(DrawScopeBadge(*nodeContract));
            if (nodeContract->keystone_exclusion_group != 0) {
                badges.push_back(BuildExclusionBadge(*nodeContract));
            }
        }

        const auto quantitativeLines = BuildNodeQuantitativeLines(*hoveredNode, *specialized, hoveredNodeId);

        std::vector<std::pair<std::string, Color>> footerLines;
        if (nodeContract && nodeContract->cost_affix != CostAffixPreset::None) {
            const auto& costAffix = CombatAntiMeta::GetCostAffixConfig(nodeContract->cost_affix);
            footerLines.emplace_back(TextFormat("代价词缀: %s", costAffix.display_name), PINK);
            if (costAffix.reward_text && costAffix.reward_text[0] != '\0') {
                footerLines.emplace_back(TextFormat("收益: %s", costAffix.reward_text), GREEN);
            }
            if (costAffix.penalty_text && costAffix.penalty_text[0] != '\0') {
                footerLines.emplace_back(TextFormat("代价: %s", costAffix.penalty_text), RED);
            }
        }

        if (excludedNodeIds.contains(hoveredNodeId)) {
            footerLines.emplace_back("当前被同组核心互斥，点击会替换当前核心", RED);
        }

        const TooltipLayoutMetrics tooltipLayout =
            ComputeTooltipLayoutMetrics(th, quantitativeLines.size(), footerLines.size());
        th = tooltipLayout.tooltipHeight;

        if (tx + tw > logicW) {
            tx -= (tw + 60);
        }
        if (ty + th > logicH) {
            ty = std::max(20.0f, logicH - th - 20.0f);
        }

        // Draw Tooltip Box
        DrawRectangleRec({tx * scale, ty * scale, tw * scale, th * scale},
                         Fade(masteryTheme.secondary, 0.14f * alpha));
        DrawRectangleRec({(tx + 4.0f) * scale, (ty + 4.0f) * scale,
                          (tw - 8.0f) * scale, (th - 8.0f) * scale},
                         Fade(BLACK, 0.95f * alpha));
        DrawRectangleLinesEx({tx * scale, ty * scale, tw * scale, th * scale}, 1.0f,
                             Fade(masteryTheme.primary, alpha));

        DrawTooltipHeader(masteryTheme, *hoveredNode, tx, ty + 2.0f, tw, alpha, scale);

        float badgeX = tx + 20.0f;
        const float badgeY = ty + 66.0f;
        for (const auto& badge : badges) {
            DrawTooltipBadgeChip(UISystem::GetFont(), badge, badgeX, badgeY, 18.0f,
                                 alpha * 0.95f, scale);
            const float badgeWidth = IsFontValid(UISystem::GetFont())
                ? MeasureTextEx(UISystem::GetFont(), badge.text.c_str(), 18.0f * scale, 1.0f * scale).x / scale
                : static_cast<float>(MeasureText(badge.text.c_str(), static_cast<int>(18.0f * scale))) / scale;
            badgeX += badgeWidth + 32.0f;
        }

        const float descX = tx + 20.0f;
        const float descY = ty + tooltipLayout.descriptionTop;
        const float descW = tw - 40.0f;
        const float descH = tooltipLayout.descriptionHeight;
        DrawKeywordHighlights(UISystem::GetFont(), hoveredNode->desc_key.c_str(),
                               descX, descY, descW, descH, 20.0f, WHITE,
                               alpha * 0.96f, scale);

        const float lineFont = 22.0f;
        if (!quantitativeLines.empty()) {
            float lineY = ty + tooltipLayout.quantitativeTop + kTooltipFooterTopPad;
            for (const auto& line : quantitativeLines) {
                UISystem::DrawTextUI(line.first.c_str(), tx + 20.0f, lineY, lineFont, line.second, alpha * 0.92f);
                lineY += kTooltipFooterLineHeight;
            }
        }

        if (!footerLines.empty()) {
            float lineY = ty + tooltipLayout.footerTop + kTooltipFooterTopPad;
            for (const auto& line : footerLines) {
                UISystem::DrawTextUI(line.first.c_str(), tx + 20.0f, lineY, lineFont, line.second, alpha * 0.9f);
                lineY += kTooltipFooterLineHeight;
            }
        }
    }

    if (!m_layoutEditMode && targetNodeId != 0) {
        SkillSystem::AddTalentPoint(registry, player, skillId, targetNodeId);
    }
}

} // namespace NoMoreDay
