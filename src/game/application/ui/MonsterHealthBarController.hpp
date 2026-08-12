#pragma once

#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace NoMoreDay::ui {

// Instance controller for the monster health bars / target widget.
//
// R5 migration: paint is snapshot-only. Update(const GameUiSnapshot&) reads the
// monster view-model from the frame snapshot (never the ECS registry): it culls
// enemies, picks the hovered target and batches the overhead damage bars into
// fixed controller-owned arrays (zero per-frame allocation). Paint emits
// draw-list commands (Hud layer) in the world-to-logical transformed space.
//
// Owned by GameUiHost; the host extracts the camera transform (target/offset/
// zoom) and the raw mouse position at the legacy world-pass call position and
// forwards them as plain data. Paint is invoked by the host during
// PrepareRender.
class MonsterHealthBarController {
public:
    explicit MonsterHealthBarController(UiRuntime& runtime);

    MonsterHealthBarController(const MonsterHealthBarController&) = delete;
    MonsterHealthBarController& operator=(const MonsterHealthBarController&) = delete;

    // World-space data pass (legacy Render position, inside Mode2D): culls
    // enemies, picks the hovered target and batches the overhead damage bars
    // from the snapshot. camera transform is passed as plain data (no raylib
    // types on the paint path).
    void Update(const GameUiSnapshot& snapshot, float camTargetX,
                float camTargetY, float camOffsetX, float camOffsetY,
                float camZoom, float mousePixelX, float mousePixelY,
                int screenPixelWidth, int screenPixelHeight);

    // Screen-space paint (called by the host during PrepareRender): emits the
    // overhead bars and the top-center target widget for the hovered entity
    // into the draw list (Hud layer). Snapshot-only, zero allocation.
    void Paint(UiDrawList& drawList, const UiViewport& viewport) const;

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
    // Fixed-capacity overhead bar batch (bounded by the monster cap; a full
    // screen of damaged monsters stays well below this).
    struct BarCmd {
        float worldX = 0.0f;
        float worldY = 0.0f; // Top edge (y + yOffset already applied).
        float width = 0.0f;
        float height = 0.0f;
        float hpPercent = 0.0f;
        bool isRare = false;
    };
    static constexpr std::size_t kMaxBars = 256;
    std::array<BarCmd, kMaxBars> m_bars{};
    std::size_t m_barCount = 0;

    // Hovered target display data (copied from the snapshot, domain id based).
    struct TargetData {
        bool hasTarget = false;
        std::uint32_t domainId = 0;
        float current = 0.0f;
        float max = 1.0f;
        std::uint8_t rarity = 0; // EnemyRarityComponent::Rarity
        std::uint8_t raceType = 0; // EnemyRace::Type
        std::array<std::uint8_t, 4> affixTypes{};
        std::uint8_t affixCount = 0;
        float worldX = 0.0f;
        float worldY = 0.0f;
    };
    TargetData m_target{};

    // Camera transform retained from Update for the world->logical mapping in
    // Paint (plain data; raylib types never appear on the paint path).
    float m_camTargetX = 0.0f;
    float m_camTargetY = 0.0f;
    float m_camOffsetX = 0.0f;
    float m_camOffsetY = 0.0f;
    float m_camZoom = 1.0f;

    UiRuntime& m_runtime;
    UiId m_rootNodeId = kInvalidUiId;
};

} // namespace NoMoreDay::ui
