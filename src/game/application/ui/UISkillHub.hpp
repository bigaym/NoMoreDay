#pragma once
#include <entt/entt.hpp>
#include "game/foundation/data/BladeMasteryData.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "raylib.h"

namespace NoMoreDay {

namespace ui {
class GameUiHost; // fwd: composition-root back-pointer (message box).
}

// Instance panel for the skill hub (skill specialization) UI.
//
// U7 cleanup: converted from a static class to an instance type so the panel
// keeps no static mutable state. U8 final narrowing: the panel reads the
// controller-supplied alpha (was State.skillTreeAlpha), writes its mastery
// selection into its own instance member (was State.selectedSkillId) and
// routes the "requirement not met" message box through the host channel
// (was the State.showMessageBox write).
class UISkillHub {
public:
    UISkillHub() = default;

    UISkillHub(const UISkillHub&) = delete;
    UISkillHub& operator=(const UISkillHub&) = delete;

    // U8: bind the composition root (GameUiHost) for the message-box
    // channel; may be null in headless tests.
    void SetHost(ui::GameUiHost* uiHost) noexcept { m_uiHost = uiHost; }

    // Draws the hub panel. `alpha` is the animated panel alpha supplied by
    // the owning SkillTreeController (was State.skillTreeAlpha).
    void Draw(entt::registry& registry, entt::entity player,
              float alpha = 1.0f);

    // Selects the given mastery; on failure routes a message box through the
    // host channel (was the State.showMessageBox write).
    bool TrySelectMastery(entt::registry& registry, entt::entity player,
                          BladeMasteryId masteryId);

    // U8: selection round-trip. Draw writes the clicked mastery here so the
    // controller can switch to the talent tree view (was State.selectedSkillId).
    [[nodiscard]] uint32_t SelectedSkillId() const noexcept {
        return m_selectedSkillId;
    }

    void ResetSelection() noexcept { m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID; }

private:
    ui::GameUiHost* m_uiHost = nullptr;
    uint32_t m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID;
};

} // namespace NoMoreDay
