#include "game/application/ui/MonsterHealthBarController.hpp"

#include "game/application/ui/UiResourceIds.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/data/MonsterAffixRegistry.hpp"
#include "engine/render/CoordSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace NoMoreDay::ui {

namespace {

inline constexpr UiId kMonsterHealthBarsRootNode =
    static_cast<UiId>(0x93A11D2Eu); // hashed "ui_monster_health_bars"

// Palette (ported verbatim from the legacy MonsterHealthBarSystem).
inline constexpr UiColor kOverheadBgColor{20, 20, 20, 180};
inline constexpr UiColor kOverheadBorderColor{0, 0, 0, 200};
inline constexpr UiColor kOverheadLowHpColor{255, 40, 40, 255};
inline constexpr UiColor kOverheadNormalColor{200, 30, 30, 255};
inline constexpr UiColor kOverheadRareColor{255, 180, 0, 255};
inline constexpr UiColor kTargetBgColor{10, 10, 10, 230};
inline constexpr UiColor kTargetBorderColor{50, 50, 50, 255};
inline constexpr UiColor kTargetBarBgColor{30, 0, 0, 255};
inline constexpr UiColor kTargetBarNormalColor{180, 20, 20, 255};
inline constexpr UiColor kTargetNameColor{255, 255, 255, 255};
inline constexpr UiColor kTargetBarColorChampion{135, 206, 235, 255}; // SKYBLUE
inline constexpr UiColor kTargetBarColorElite{255, 215, 0, 255};      // GOLD
inline constexpr UiColor kTargetBarColorBoss{255, 165, 0, 255};       // ORANGE
inline constexpr UiColor kTargetBarColorNemesis{255, 0, 0, 255};      // RED
inline constexpr float kHoverPickRadius = 40.0f;

// Screen-space metrics of the top-center target widget (legacy layout).
inline constexpr float kTargetWidth = 400.0f;
inline constexpr float kTargetHeight = 40.0f;
inline constexpr float kTargetTop = 50.0f;

// Returns the rarity bar/name color for the given EnemyRarityComponent::Rarity
// value. Default (NORMAL / unknown) is the normal red.
UiColor RarityBarColor(std::uint8_t rarity) {
    switch (rarity) {
        case 1: return kTargetBarColorChampion; // CHAMPION
        case 2: return kTargetBarColorElite;    // ELITE
        case 3: return kTargetBarColorBoss;     // BOSS
        case 4: return kTargetBarColorNemesis;  // NEMESIS
        default: return kTargetBarNormalColor;
    }
}

} // namespace

MonsterHealthBarController::MonsterHealthBarController(UiRuntime& runtime)
    : m_runtime(runtime) {
    UiNodeDesc desc;
    desc.id = kMonsterHealthBarsRootNode;
    desc.parent = kRootUiId;
    desc.layout.kind = UiLayoutKind::Overlay;
    desc.visible = true;
    desc.hitTestVisible = false;
    desc.capturePointer = false;
    desc.focusable = false;
    desc.captureKeyboard = false;
    desc.acceptsText = false;
    desc.modal = false;
    desc.zIndex = static_cast<std::int32_t>(UiDrawLayer::Hud);
    desc.customPainter = kInvalidUiResourceId;
    if (m_runtime.CreateNode(desc)) {
        m_rootNodeId = desc.id;
    } else {
        m_rootNodeId = kInvalidUiId;
    }
}

void MonsterHealthBarController::EnterGameplay() {
    m_target = TargetData{};
}

void MonsterHealthBarController::LeaveGameplay() {
    m_target = TargetData{};
}

void MonsterHealthBarController::Update(const GameUiSnapshot& snapshot,
                                        float camTargetX, float camTargetY,
                                        float camOffsetX, float camOffsetY,
                                        float camZoom, float mousePixelX,
                                        float mousePixelY,
                                        int screenPixelWidth,
                                        int screenPixelHeight) {
    m_camTargetX = camTargetX;
    m_camTargetY = camTargetY;
    m_camOffsetX = camOffsetX;
    m_camOffsetY = camOffsetY;
    m_camZoom = camZoom;
    if (camZoom <= 0.0f) {
        camZoom = 1.0f;
    }

    m_barCount = 0;
    m_target = TargetData{};

    if (!snapshot.player.hasPlayer || snapshot.monsters.empty()) {
        return;
    }

    // R1: use CoordSystem instead of hand-rolling raylib camera math.
    NoMoreDay::render::coord::Camera2DTransform cam;
    cam.target = {camTargetX, camTargetY};
    cam.offset = {camOffsetX, camOffsetY};
    cam.zoom = camZoom > 0.0f ? camZoom : 1.0f;
    const Vector2 mouseWorld = NoMoreDay::render::coord::ScenePixelToWorld(
        cam, Vector2{mousePixelX, mousePixelY});
    const float mouseWorldX = mouseWorld.x;
    const float mouseWorldY = mouseWorld.y;

    // Viewport culling bounds in world space (+100 padding, legacy).
    const Vector2 viewMin = NoMoreDay::render::coord::ScenePixelToWorld(
        cam, Vector2{0.0f, 0.0f});
    const Vector2 viewMax = NoMoreDay::render::coord::ScenePixelToWorld(
        cam, Vector2{static_cast<float>(screenPixelWidth),
                     static_cast<float>(screenPixelHeight)});
    const float viewMinX = viewMin.x - 100.0f;
    const float viewMinY = viewMin.y - 100.0f;
    const float viewMaxX = viewMax.x + 100.0f;
    const float viewMaxY = viewMax.y + 100.0f;

    float closestDistSq = 3.4028235e38f;
    for (const GameUiMonsterHealthView& monster : snapshot.monsters) {
        const float mx = monster.worldX;
        const float my = monster.worldY;

        // Culling (world space).
        if (mx < viewMinX || mx > viewMaxX || my < viewMinY || my > viewMaxY) {
            continue;
        }
        if (monster.current <= 0.0f) {
            continue;
        }

        // Hover pick (closest under cursor, legacy pick radius logic).
        const float dx = mx - mouseWorldX;
        const float dy = my - mouseWorldY;
        const float distSq = dx * dx + dy * dy;
        const float pickRadius =
            (monster.radius > 0.0f ? monster.radius + 10.0f
                                   : kHoverPickRadius);
        const float pickRadiusSq = pickRadius * pickRadius;
        if (distSq < pickRadiusSq && distSq < closestDistSq) {
            closestDistSq = distSq;
            m_target.hasTarget = true;
            m_target.domainId = monster.domainId;
            m_target.current = monster.current;
            m_target.max = monster.max > 0.0f ? monster.max : 1.0f;
            m_target.rarity = monster.rarity;
            m_target.raceType = monster.raceType;
            m_target.affixCount = monster.affixCount;
            for (std::uint8_t i = 0; i < monster.affixCount && i < 4; ++i) {
                m_target.affixTypes[i] = monster.affixTypes[i];
            }
            m_target.worldX = mx;
            m_target.worldY = my;
        }

        // Overhead bar: only when damaged (legacy optimization).
        if (monster.current >= monster.max - 0.1f) {
            continue;
        }
        if (m_barCount >= m_bars.size()) {
            break; // Capped; overflow is bounded by the monster cap.
        }

        const float hpPercent =
            std::clamp(monster.current / (monster.max > 0.0f ? monster.max
                                                             : 1.0f),
                       0.0f, 1.0f);
        BarCmd& cmd = m_bars[m_barCount++];
        cmd.worldX = mx - 20.0f;
        cmd.worldY = my - 25.0f;
        cmd.width = 40.0f;
        cmd.height = 4.0f;
        cmd.hpPercent = hpPercent;
        cmd.isRare = monster.rarity > 0; // EnemyRarityComponent::NORMAL == 0
    }
}

void MonsterHealthBarController::Paint(UiDrawList& drawList,
                                       const UiViewport& viewport) const {
    // R1: world -> scene pixel via CoordSystem, then scene pixel -> logical
    // via UiViewport. No raw camera formula on the paint path.
    NoMoreDay::render::coord::Camera2DTransform cam;
    cam.target = {m_camTargetX, m_camTargetY};
    cam.offset = {m_camOffsetX, m_camOffsetY};
    cam.zoom = m_camZoom > 0.0f ? m_camZoom : 1.0f;

    // Overhead bars (world -> logical via the retained camera transform).
    for (std::size_t i = 0; i < m_barCount; ++i) {
        const BarCmd& cmd = m_bars[i];
        const Vector2 screen = NoMoreDay::render::coord::WorldToScenePixel(
            cam, Vector2{cmd.worldX, cmd.worldY});
        const UiVec2 origin = viewport.ToLogical(UiVec2{screen.x, screen.y});
        const float scale = viewport.Scale();
        const UiRect bg{origin, {cmd.width * scale, cmd.height * scale}};
        const UiRect fg{bg.origin,
                        {bg.size.x * cmd.hpPercent, bg.size.y}};

        drawList.FillRect(UiDrawLayer::Hud, kMonsterHealthBarsRootNode, bg,
                          kOverheadBgColor);
        drawList.FillRect(UiDrawLayer::Hud, kMonsterHealthBarsRootNode, fg,
                          cmd.isRare ? kOverheadRareColor
                                     : (cmd.hpPercent < 0.25f
                                            ? kOverheadLowHpColor
                                            : kOverheadNormalColor));
        if (cmd.isRare) {
            drawList.StrokeRect(UiDrawLayer::Hud, kMonsterHealthBarsRootNode,
                                bg, kOverheadBorderColor, 1.0f);
        }
    }

    // Top-center target widget for the hovered entity.
    if (!m_target.hasTarget) {
        return;
    }

    const UiVec2 viewSize = viewport.LogicalSize();
    const float x = (viewSize.x - kTargetWidth) * 0.5f;
    const float y = kTargetTop;

    // 1. Background frame.
    const UiRect bgRect{{x, y}, {kTargetWidth, kTargetHeight}};
    drawList.FillRect(UiDrawLayer::Hud, kMonsterHealthBarsRootNode, bgRect,
                      kTargetBgColor);
    drawList.StrokeRect(UiDrawLayer::Hud, kMonsterHealthBarsRootNode, bgRect,
                        kTargetBorderColor, 2.0f);

    // 2. Health bar.
    constexpr float margin = 4.0f;
    const UiRect barBg{{x + margin, y + margin},
                       {kTargetWidth - margin * 2.0f,
                        kTargetHeight - margin * 2.0f}};
    drawList.FillRect(UiDrawLayer::Hud, kMonsterHealthBarsRootNode, barBg,
                      kTargetBarBgColor);

    const float hpPercent = std::clamp(m_target.current / m_target.max, 0.0f,
                                       1.0f);
    const UiColor barColor = RarityBarColor(m_target.rarity);
    const UiRect barFg{barBg.origin, {barBg.size.x * hpPercent, barBg.size.y}};
    drawList.FillRect(UiDrawLayer::Hud, kMonsterHealthBarsRootNode, barFg,
                      barColor);

    // 3. Name (above the bar; race display name from the static table).
    std::string_view name = "Enemy";
    if (static_cast<std::size_t>(m_target.raceType) < kRaceData.size()) {
        name = kRaceData[static_cast<std::size_t>(m_target.raceType)].name;
    }
    drawList.Text(UiDrawLayer::Hud, kMonsterHealthBarsRootNode, name,
                  {x + kTargetWidth * 0.5f, y - 25.0f}, 18.0f,
                  m_target.rarity > 0 ? RarityBarColor(m_target.rarity)
                                      : kTargetNameColor,
                  kGlobalFontResourceId, UiTextAlign::Center);

    // HP text (inside the bar).
    char hpText[48];
    std::snprintf(hpText, sizeof(hpText), "%.0f / %.0f", m_target.current,
                  m_target.max);
    drawList.Text(UiDrawLayer::Hud, kMonsterHealthBarsRootNode, hpText,
                  {x + kTargetWidth * 0.5f, y + kTargetHeight * 0.5f - 8.0f},
                  16.0f, kTargetNameColor, kGlobalFontResourceId,
                  UiTextAlign::Center);

    // 4. Affix labels (below the bar; static table names).
    float labelX = x;
    const float labelY = y + kTargetHeight + 8.0f;
    for (std::uint8_t i = 0; i < m_target.affixCount; ++i) {
        const auto& def =
            MonsterAffixRegistry::GetAffixDef(
                static_cast<MonsterAffixType>(m_target.affixTypes[i]));
        char label[40];
        std::snprintf(label, sizeof(label), "[%s]", def.name);
        const UiColor tint{def.tintR, def.tintG, def.tintB, 255};
        drawList.Text(UiDrawLayer::Hud, kMonsterHealthBarsRootNode, label,
                      {labelX, labelY}, 14.0f, tint, kGlobalFontResourceId);
        labelX += 14.0f * static_cast<float>(std::strlen(label)) * 0.55f +
                  5.0f; // Approx advance (no raylib measure on paint path).
    }
}

} // namespace NoMoreDay::ui
