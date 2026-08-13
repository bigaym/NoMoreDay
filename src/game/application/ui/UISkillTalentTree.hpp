#pragma once
#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"
#include "game/foundation/components/SkillDefs.hpp" // INVALID_SKILL_ID

#include <cstddef>
#include <cstdint>
#include <unordered_set>

// Forward declarations
namespace NoMoreDay {

namespace ui {
class TooltipController; // fwd: hovered-skill channel (U8).
class GameUiHost;        // R8: intent sink (talent reset / allocate).
struct UiInputFrame;
} // namespace ui

// Talent tree panel widget.
//
// R8 (UI remediation, design §3.1/§3.4): the panel is a snapshot/intent
// surface. The interaction phase (UpdateInput) reads the snapshot skill-tree
// segment (specialized slot + allocated points + talent budget + mutual
// keystone exclusions), runs pan/zoom/hover and enqueues gameplay-writing
// intents (SkillResetTalents / SkillAllocateTalentPoint) through the host
// composition root; it never touches the registry. The render phase paints
// through the draw list (Paint -> custom painter -> PaintCanvas): the raylib
// drawing (background chrome, scissored skill-spec canvas, hover feedback,
// node tooltip) is confined to the registered backend painter and reads the
// paint state captured by UpdateInput (frame-scoped snapshot pointer, alpha,
// skillId, hovered node, SkillSpecView, exclusions).
//
// U7 cleanup: the legacy static mutable state (s_viewOffset, s_viewZoom,
// s_lastSkillId, s_lastMouseLogicPos, s_layoutEditMode, s_draggingNodeId,
// s_dragNodeOffset) was migrated to instance members so the UI no longer
// keeps any static mutable rendering state (design invariant 4). The
// ComputeTooltipLayoutMetrics pure helper stays static: it is stateless and
// side-effect free. U8 final narrowing: the panel reads its alpha from the
// caller (was State.skillTreeAlpha), uses UISystem scale helpers (was
// State.scaleFactor) and routes the hovered node through the bound tooltip
// controller (was State.hoveredSkillId).
class SkillTreeUI {
public:
    struct Vec2 { float x, y; };
    struct TooltipLayoutMetrics {
        float tooltipHeight = 0.0f;
        float descriptionTop = 0.0f;
        float descriptionHeight = 0.0f;
        float descriptionBottom = 0.0f;
        float quantitativeTop = 0.0f;
        float quantitativeHeight = 0.0f;
        float footerTop = 0.0f;
        float footerHeight = 0.0f;
        float footerGap = 0.0f;
    };

    SkillTreeUI() = default;
    SkillTreeUI(const SkillTreeUI&) = delete;
    SkillTreeUI& operator=(const SkillTreeUI&) = delete;

    // U8: bind the tooltip controller for the hovered-node channel (may be
    // null in headless tests). Called by SkillTreeController.
    void SetTooltip(ui::TooltipController* tooltip) noexcept { m_tooltip = tooltip; }

    // R8: interaction phase. Reads the snapshot skill-tree segment, runs the
    // pan/zoom/hover/reset/edit/node-click hit tests and enqueues
    // gameplay-writing intents through `uiHost` (may be null headless);
    // captures the paint state consumed by PaintCanvas.
    void UpdateInput(const ui::GameUiSnapshot& snapshot,
                     const ui::UiInputFrame& input, uint32_t skillId,
                     ui::GameUiHost* uiHost, float alpha);

    // R8: render-phase paint. Emits the tree custom command (Panels layer)
    // through the draw list; the backend invokes PaintCanvas via the
    // registered painter.
    void Paint(ui::UiDrawList& drawList, const ui::UiViewport& viewport,
               uint32_t skillId, float alpha);

    // R8: canvas draw (registered custom-painter target). Draws the panel
    // chrome + scissored spec canvas + hover feedback + node tooltip with
    // raylib primitives; read-only over the paint state. Public so tech tests
    // can smoke it without the backend dispatch.
    void PaintCanvas(ui::UiRect nativeBounds);

    // U8: the back button writes this so the controller can return to the
    // hub view (was State.selectedSkillId = INVALID_SKILL_ID).
    [[nodiscard]] uint32_t SelectedSkillId() const noexcept {
        return m_selectedSkillId;
    }

    static TooltipLayoutMetrics ComputeTooltipLayoutMetrics(float tooltipHeight,
                                                            std::size_t quantitativeLineCount,
                                                            std::size_t footerLineCount);

private:
    Vec2 m_viewOffset = { 0, 0 };
    float m_viewZoom = 1.0f;
    uint32_t m_lastSkillId = 0;
    Vec2 m_lastMouseLogicPos = { 0, 0 };
    bool m_layoutEditMode = false;
    uint32_t m_draggingNodeId = 0;
    Vec2 m_dragNodeOffset = { 0, 0 };
    bool m_layoutDirty = false; // Edit-mode save gate: set when a drag moved a node.
    ui::TooltipController* m_tooltip = nullptr; // Hover channel (U8).
    uint32_t m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID; // View reset (U8).

    // R8: read-only render data captured by UpdateInput (frame-scoped). The
    // painter never re-queries gameplay.
    struct TreePaintState {
        const ui::GameUiSnapshot* snapshot = nullptr;
        uint32_t skillId = NoMoreDay::INVALID_SKILL_ID;
        float alpha = 0.0f;
        uint32_t hoveredNodeId = 0;
        std::unordered_set<uint32_t> excludedNodeIds;
    } m_paint{};
};

// R8: backend painter callback (registered by GameUiHost with
// kSkillTreePainterResourceId). userData points to the SkillTreeUI instance.
void SkillTreePaintCallback(void* userData, ui::UiRect nativeBounds);

} // namespace NoMoreDay
