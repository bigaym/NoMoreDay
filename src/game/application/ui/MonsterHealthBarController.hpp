#pragma once

#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"

#include <entt/entt.hpp>

#include "raylib.h"

namespace NoMoreDay::ui {

// Instance controller for the monster health bars / target widget.
//
// Ports the legacy static system MonsterHealthBarSystem into a hostable
// instance: the controller owns a UiRuntime root node (created in the ctor)
// and renders with the exact same visual output as the original system. It
// holds no static mutable UI state; the frame-scoped hovered target is an
// instance member reset by EnterGameplay/LeaveGameplay and per-frame by
// Render.
//
// Owned by GameUiHost; the host drives EnterGameplay/LeaveGameplay around
// gameplay sessions and calls Render (world pass, inside Mode2D) and RenderUI
// (screen pass) at the original legacy call positions.
class MonsterHealthBarController {
public:
    explicit MonsterHealthBarController(UiRuntime& runtime);

    MonsterHealthBarController(const MonsterHealthBarController&) = delete;
    MonsterHealthBarController& operator=(const MonsterHealthBarController&) = delete;

    // World-space pass: culls enemies, picks the hovered target and batches
    // overhead damage bars. Runs inside Mode2D (legacy position preserved).
    void Render(entt::registry& registry, const Camera2D& camera);

    // Screen-space pass: top-center target widget for the hovered entity.
    // Runs after the scene composite, outside Mode2D.
    void RenderUI(entt::registry& registry);

    // Clears session-scoped state (hovered target) when a gameplay session
    // begins. Idempotent.
    void EnterGameplay();

    // Clears session-scoped state (hovered target) when a gameplay session
    // ends. Idempotent.
    void LeaveGameplay();

    // Runtime node id of the health-bars root (kInvalidUiId if the node could
    // not be created, e.g. a duplicate id already exists in the runtime).
    [[nodiscard]] UiId NodeId() const noexcept { return m_rootNodeId; }

private:
    void DrawTargetWidget(entt::registry& registry, entt::entity entity);

    UiRuntime& m_runtime;
    UiId m_rootNodeId = kInvalidUiId;
    // Entity under the mouse cursor for the current frame; set by Render and
    // consumed by RenderUI. Never retained across frames.
    entt::entity m_hoveredEntity = entt::null;
};

} // namespace NoMoreDay::ui
