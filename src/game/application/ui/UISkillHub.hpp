#pragma once
#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"
#include "game/foundation/data/BladeMasteryData.hpp"

#include <array>
#include <cstdint>

namespace NoMoreDay {

namespace ui {
class GameUiHost; // fwd: composition-root back-pointer (message box + intent sink).
struct UiInputFrame;
} // namespace ui

// Instance panel for the skill hub (skill specialization) UI.
//
// R8 (UI remediation, design §3.1/§3.4): the hub is a snapshot/intent
// surface. The interaction phase (UpdateInput) reads the snapshot skill-hub
// segment (mastery cards / specialized slots / attunement / debug override),
// runs the hit tests against the UiInputFrame pointer and enqueues
// gameplay-writing intents (SkillSelectMastery / SkillSetAttunement /
// SkillSetDebugUnlock / SkillAssign / SkillUnassign) through the host
// composition root; it never touches the registry. The render phase paints
// through the draw list (Paint -> custom painter -> PaintCanvas): raylib draw
// calls are confined to the registered backend painter, and its read-only
// data comes from the paint state captured by UpdateInput (frame-scoped
// snapshot pointer, alpha, mastery selection, attunement, slot cache).
// UI-local session state (selected mastery skill id, hover routing) stays in
// the controller / host channels, exactly like the other R4-R7 panels.
class UISkillHub {
public:
    UISkillHub() = default;

    UISkillHub(const UISkillHub&) = delete;
    UISkillHub& operator=(const UISkillHub&) = delete;

    // R8: bind the composition root (GameUiHost) for the intent sink and the
    // hover channel; may be null in headless tests (interactions degrade to
    // UI-local session state, matching the other panels).
    void SetHost(ui::GameUiHost* uiHost) noexcept { m_uiHost = uiHost; }

    // R8: interaction phase. Reads the snapshot skill-hub segment, resolves
    // the hover/tooltip channel and enqueues skill intents; fills the paint
    // state consumed by PaintCanvas. `alpha` is the animated panel alpha
    // supplied by the owning SkillTreeController.
    void UpdateInput(const ui::GameUiSnapshot& snapshot,
                     const ui::UiInputFrame& input, float alpha);

    // R8: render-phase paint. Emits the hub custom command (Panels layer)
    // through the draw list; the backend invokes PaintCanvas via the
    // registered painter.
    void Paint(ui::UiDrawList& drawList, const ui::UiViewport& viewport,
               const ui::GameUiSnapshot& snapshot, float alpha);

    // R8: canvas draw (registered custom-painter target). Draws the hub
    // chrome with raylib primitives; read-only over the paint state. Public
    // so tech tests can smoke it without the backend dispatch.
    void PaintCanvas(ui::UiRect nativeBounds);

    // R8: selection round-trip. The interaction phase writes the clicked
    // mastery here so the controller can switch to the talent tree view (was
    // the State.selectedSkillId round-trip).
    [[nodiscard]] uint32_t SelectedSkillId() const noexcept {
        return m_selectedSkillId;
    }

    void ResetSelection() noexcept { m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID; }

private:
    // R8: read-only render data captured by UpdateInput (frame-scoped). The
    // painter never re-queries gameplay.
    struct HubPaintState {
        const ui::GameUiSnapshot* snapshot = nullptr;
        float alpha = 0.0f;
        bool hasBladeProfession = false;
        bool debugUnlockEnabled = false;
        std::uint8_t selectedMastery = 0; // BladeMasteryId
        std::uint8_t heavenlyAttunement = 0; // BladeAttunement
        int playerLevel = 1;
        std::array<ui::GameUiSpecializedSlotView, 5> slots;
        std::vector<ui::GameUiMasteryCardView> cards;
        std::vector<std::uint32_t> lockedSignatureSkills;
    } m_paint{};

    ui::GameUiHost* m_uiHost = nullptr;
    uint32_t m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID;
};

// R8: backend painter callback (registered by GameUiHost with
// kSkillHubPainterResourceId). userData points to the UISkillHub instance.
void SkillHubPaintCallback(void* userData, ui::UiRect nativeBounds);

} // namespace NoMoreDay
