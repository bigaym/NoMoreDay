#include "game/application/ui/MinimapController.hpp"

#include "core/logging/Logger.hpp"
#include "core/utils/FmtBuffer.hpp"
#include "engine/render/GPUData.hpp" // components::Colors::MAP_AFFIX_*
#include "game/application/ui/UICommon.hpp"
#include "game/application/ui/UiResourceIds.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/WorldState.hpp"
#include "game/foundation/data/AffixMapping.hpp"
#include "game/foundation/data/BiomeTypes.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/world/EnemyConstants.hpp"
#include "game/systems/world/FogOfWarSystem.hpp"
#include "game/systems/world/LevelManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace NoMoreDay::ui {

namespace {
// Node id: hashed label so the id is deterministic and readable.
inline constexpr UiId kMinimapRootNode =
    static_cast<UiId>(0x31B7E4FAu); // hashed "ui_minimap"

// Fog texture palette (ported verbatim from UIMinimap::Draw).
inline constexpr Color kFogWalkableVisible{160, 160, 160, 255};
inline constexpr Color kFogWalkableExplored{60, 60, 60, 255};
inline constexpr Color kFogWallVisible{80, 80, 80, 255};
inline constexpr Color kFogWallExplored{30, 30, 30, 255};
} // namespace

MinimapController::MinimapController(UiRuntime& runtime) : m_runtime(runtime) {
  UiNodeDesc desc;
  desc.id = kMinimapRootNode;
  desc.parent = kRootUiId;
  desc.layout.kind = UiLayoutKind::Overlay;
  desc.layout.width = UiLength::Pixels(188.0f);
  desc.layout.height = UiLength::Pixels(188.0f);
  desc.layout.margin = UiInsets{/*left*/ 2346.0f, /*top*/ 26.0f, 0.0f, 0.0f};
  desc.visible = true;
  desc.hitTestVisible = false; // Display-only; no pointer capture.
  desc.capturePointer = false;
  desc.focusable = false;
  desc.captureKeyboard = false;
  desc.acceptsText = false;
  desc.modal = false;
  desc.zIndex = static_cast<std::int32_t>(UiDrawLayer::Hud);
  desc.customPainter = kInvalidUiResourceId;
  if (m_runtime.CreateNode(desc)) {
    m_rootNodeId = desc.id;
  }
}

MinimapController::~MinimapController() { UnloadResources(); }

void MinimapController::Shutdown() { UnloadResources(); }

void MinimapController::UnloadResources() {
  if (m_minimapTexture.id != 0) {
    UnloadTexture(m_minimapTexture);
    m_minimapTexture.id = 0;
  }
  m_minimapPixels.clear();
  m_minimapPixels.shrink_to_fit();
  m_minimapDirty = true;
  m_lastWasTown = false;
}

void MinimapController::EnterGameplay() {
  m_inGameplay = true;
  m_refreshTimer = 0.0f;
  m_minimapDirty = true;
  m_lastWasTown = false;
}

void MinimapController::LeaveGameplay() {
  m_inGameplay = false;
  m_refreshTimer = 0.0f;
}

void MinimapController::ToggleDebugReveal() {
  m_debugRevealMap = !m_debugRevealMap;
  m_minimapDirty = true;
  LOG_INFO("Minimap Debug Reveal: {}", m_debugRevealMap ? "ON" : "OFF");
}

void MinimapController::Update(const GameUiSnapshot& snapshot,
                               const LevelManager& levelManager,
                               const NoMoreDay::systems::SpatialHashGrid*,
                               float deltaSeconds) {
  m_hasRunUpdate = true;
  m_enemyDotCount = 0;
  m_hasPortalArrow = false;
  m_hasZoneText = false;
  m_hasKillText = false;
  m_affixTextCount = 0;

  const auto& map = levelManager.getMapSystem();
  const auto& fog = levelManager.getFogSystem();

  // Layout in Logic Space (fixed 2K reference).
  const float mapSize = 180.0f;
  const float margin = 30.0f;
  const float x = UI_REF_WIDTH - mapSize - margin;
  const float y = margin;

  int gridW = fog.getWidth();
  int gridH = fog.getHeight();
  m_hasFog = gridW > 0 && gridH > 0;

  m_hasPlayerPosition = snapshot.player.hasWorldPosition;
  m_isTown = levelManager.getCurrentBiomeID() == NoMoreDay::BiomeID::Town;

  // Zone text (from level manager world data + snapshot dimensional state).
  {
    char zoneBuf[128];
    const char* baseName = m_isTown ? "宁静村落" : "地下城";
    if (m_isTown) {
      utils::FormatToBuffer(zoneBuf, "{}", baseName);
    } else {
      int displayLevel = levelManager.getCurrentLevel();
      if (snapshot.minimap.dimensionalActive) {
        int difficultyLv = snapshot.minimap.dimensionalBaseLevel +
                           (snapshot.minimap.dimensionalDepth - 1);
        utils::FormatToBuffer(zoneBuf, "异界 - {}层 [Lv.{}]",
                              snapshot.minimap.dimensionalDepth, difficultyLv);
      } else {
        utils::FormatToBuffer(zoneBuf, "{} - {}层", baseName, displayLevel);
      }
    }
    std::snprintf(m_zoneText, sizeof(m_zoneText), "%s", zoneBuf);
    m_hasZoneText = true;
  }

  // Kill progress text (from snapshot).
  if (!m_isTown) {
    const std::uint32_t kills = snapshot.minimap.currentMapKills;
    const std::uint32_t required = snapshot.minimap.killRequirement;
    m_killGoalReached = required > 0 && kills >= required;
    if (m_killGoalReached) {
      utils::FormatToBuffer(m_killText, "击杀: {} (出口已标位)", kills);
    } else {
      utils::FormatToBuffer(m_killText, "击杀: {} / {}", kills, required);
    }
    m_hasKillText = true;

    // Portal arrow direction (snapshot next-level portal position).
    if (m_killGoalReached && snapshot.minimap.hasNextLevelPortal) {
      float dx = snapshot.minimap.nextLevelPortalX - snapshot.player.worldX;
      float dy = snapshot.minimap.nextLevelPortalY - snapshot.player.worldY;
      m_portalAngle = std::atan2(dy, dx);
      m_hasPortalArrow = true;
    }

    // Map affixes (mosaic level, level manager resonance data).
    if (levelManager.isMosaicLevel()) {
      const auto& resonance = levelManager.getCurrentResonance();
      auto addAffix = [&](const char* label, float value, bool isPositive) {
        if (m_affixTextCount >=
            static_cast<std::uint8_t>(std::size(m_affixTexts))) {
          return;
        }
        const std::size_t index = m_affixTextCount++;
        utils::FormatToBuffer(m_affixTexts[index], "{}: {:+.0f}%", label,
                              (value - 1.0f) * 100.0f);
        m_affixPositive[index] = isPositive;
      };
      if (resonance.totalEnemyDensity != 1.0f) {
        addAffix("怪物密度", resonance.totalEnemyDensity,
                 resonance.totalEnemyDensity < 1.0f);
      }
      if (resonance.totalDropRate != 1.0f) {
        addAffix("物品掉落", resonance.totalDropRate,
                 resonance.totalDropRate > 1.0f);
      }
      if (resonance.totalLevelMod != 0) {
        if (m_affixTextCount <
            static_cast<std::uint8_t>(std::size(m_affixTexts))) {
          const std::size_t index = m_affixTextCount;
          utils::FormatToBuffer(m_affixTexts[index], "怪物等级: {:+}",
                                resonance.totalLevelMod);
          m_affixPositive[index] = false;
          ++m_affixTextCount;
        }
      }
      if (resonance.dominantElement != FragmentElement::None) {
        if (m_affixTextCount <
            static_cast<std::uint8_t>(std::size(m_affixTexts))) {
          const char* elemName =
              FragmentElementzh[static_cast<std::size_t>(
                                    resonance.dominantElement)]
                  .data();
          std::snprintf(m_affixTexts[m_affixTextCount],
                        sizeof(m_affixTexts[m_affixTextCount]), "%s",
                        elemName);
          m_affixPositive[m_affixTextCount] = true; // GOLD tint below.
          ++m_affixTextCount;
        }
      }
    }
  }

  if (!m_hasFog || !m_hasPlayerPosition) {
    return;
  }

  m_playerGx = static_cast<int>(snapshot.player.worldX /
                                FogOfWarSystem::TILE_SIZE);
  m_playerGy = static_cast<int>(snapshot.player.worldY /
                                FogOfWarSystem::TILE_SIZE);
  const int viewRadius = 30;

  // --- Fog texture maintenance (retained GPU resource) ---------------------
  if (m_minimapTexture.id == 0 || m_minimapW != gridW || m_minimapH != gridH) {
    if (m_minimapTexture.id != 0) {
      UnloadTexture(m_minimapTexture);
    }
    m_minimapW = gridW;
    m_minimapH = gridH;
    m_minimapPixels.assign(gridW * gridH, BLACK);
    Image img = GenImageColor(gridW, gridH, BLACK);
    m_minimapTexture = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(m_minimapTexture, TEXTURE_FILTER_POINT);
    SetTextureWrap(m_minimapTexture, TEXTURE_WRAP_CLAMP);
    m_minimapDirty = true;
  }

  m_refreshTimer += deltaSeconds;
  if (m_refreshTimer >= 0.05f || m_minimapDirty || m_lastWasTown != m_isTown) {
    m_refreshTimer = 0.0f;
    m_lastWasTown = m_isTown;

    if (m_minimapPixels.size() != static_cast<std::size_t>(gridW * gridH)) {
      m_minimapPixels.assign(gridW * gridH, BLACK);
      m_minimapDirty = true;
    }

    bool changed = false;
    if (m_isTown) {
      // In town (safezone), reveal the entire town map on the minimap
      for (int gy = 0; gy < gridH; ++gy) {
        for (int gx = 0; gx < gridW; ++gx) {
          const int index = gy * gridW + gx;
          const Color oldC = m_minimapPixels[index];
          const Color c =
              map.isWalkable(gx, gy) ? kFogWalkableVisible : kFogWallVisible;
          if (c.r != oldC.r || c.g != oldC.g || c.b != oldC.b) {
            m_minimapPixels[index] = c;
            changed = true;
          }
        }
      }
    } else {
      // Determine update bounds around player (clamped to grid)
      const int scanRadius = viewRadius + 10;
      const int minGx = m_minimapDirty ? 0 : std::max(0, m_playerGx - scanRadius);
      const int maxGx = m_minimapDirty ? gridW : std::min(gridW, m_playerGx + scanRadius + 1);
      const int minGy = m_minimapDirty ? 0 : std::max(0, m_playerGy - scanRadius);
      const int maxGy = m_minimapDirty ? gridH : std::min(gridH, m_playerGy + scanRadius + 1);

      for (int gy = minGy; gy < maxGy; ++gy) {
        for (int gx = minGx; gx < maxGx; ++gx) {
          const int index = gy * gridW + gx;
          const Color oldC = m_minimapPixels[index];
          Color c = BLACK;

          const bool isExplored = fog.isExplored(gx, gy);
          if (isExplored || m_debugRevealMap) {
            const bool isVisible =
                m_debugRevealMap ? true : fog.isVisible(gx, gy);
            if (map.isWalkable(gx, gy)) {
              c = isVisible ? kFogWalkableVisible : kFogWalkableExplored;
            } else {
              c = isVisible ? kFogWallVisible : kFogWallExplored;
            }
          }

          if (c.r != oldC.r || c.g != oldC.g || c.b != oldC.b) {
            m_minimapPixels[index] = c;
            changed = true;
          }
        }
      }
    }

    if (changed || m_minimapDirty) {
      UpdateTexture(m_minimapTexture, m_minimapPixels.data());
    }

    m_minimapDirty = false;
  }

  // --- Enemy dots (from the snapshot monster view-model) -------------------
  const float minimapScale = mapSize / (float)(viewRadius * 2);
  for (const GameUiMonsterHealthView& monster : snapshot.monsters) {
    if (m_enemyDotCount >= m_enemyDots.size()) {
      break;
    }
    float dx = (monster.worldX - snapshot.player.worldX) /
               FogOfWarSystem::TILE_SIZE;
    float dy = (monster.worldY - snapshot.player.worldY) /
               FogOfWarSystem::TILE_SIZE;
    const int enemyGx =
        static_cast<int>(monster.worldX / FogOfWarSystem::TILE_SIZE);
    const int enemyGy =
        static_cast<int>(monster.worldY / FogOfWarSystem::TILE_SIZE);
    if (!m_debugRevealMap && !fog.isVisible(enemyGx, enemyGy)) {
      continue;
    }
    if (std::abs(dx) <= viewRadius && std::abs(dy) <= viewRadius) {
      EnemyDot& dot = m_enemyDots[m_enemyDotCount++];
      dot.x = x + (dx + viewRadius) * minimapScale;
      dot.y = y + (dy + viewRadius) * minimapScale;
    }
  }
}

void MinimapController::Paint(UiDrawList& drawList,
                              const UiViewport& viewport) const {
  (void)viewport; // Layout is in the fixed 2K reference logical space.
  // Never updated (no gameplay session frame has driven Update yet): emit
  // nothing so empty-snapshot hosts produce zero commands.
  if (!m_hasRunUpdate) {
    return;
  }
  auto& theme = UIRenderer::GetTheme();

  // Layout in Logic Space.
  const float mapSize = 180.0f;
  const float margin = 30.0f;
  const float x = UI_REF_WIDTH - mapSize - margin;
  const float y = margin;

  // Background shadow/border.
  drawList.FillRect(UiDrawLayer::Hud, kMinimapRootNode,
                    UiRect{{x - 4.0f, y - 4.0f}, {mapSize + 8.0f,
                                                  mapSize + 8.0f}},
                    UiColor{theme.panelBackground.r, theme.panelBackground.g,
                            theme.panelBackground.b, theme.panelBackground.a});
  drawList.StrokeRect(UiDrawLayer::Hud, kMinimapRootNode,
                      UiRect{{x - 4.0f, y - 4.0f},
                             {mapSize + 8.0f, mapSize + 8.0f}},
                      UiColor{theme.panelBorder.r, theme.panelBorder.g,
                              theme.panelBorder.b, theme.panelBorder.a},
                      1.0f);

  if (!m_hasFog || !m_hasPlayerPosition) {
    return;
  }

  // Fog texture (cropped around the player).
  const int viewRadius = 30;
  const float viewSize = static_cast<float>(viewRadius * 2);

  drawList.Image(
      UiDrawLayer::Hud, kMinimapRootNode,
      UiRect{{x, y}, {mapSize, mapSize}}, kMinimapTextureResourceId,
      UiColor{255, 255, 255, 255},
      UiRect{{static_cast<float>(m_playerGx - viewRadius),
              static_cast<float>(m_playerGy - viewRadius)},
             {viewSize, viewSize}});

  // Scanline / overlay effect (top-down gradient).
  drawList.FillRect(UiDrawLayer::Hud, kMinimapRootNode,
                    UiRect{{x, y}, {mapSize, mapSize}},
                    UiColor{255, 255, 255, 13});

  // Enemy dots.
  for (std::size_t i = 0; i < m_enemyDotCount; ++i) {
    const EnemyDot& dot = m_enemyDots[i];
    drawList.FillRect(UiDrawLayer::Hud, kMinimapRootNode,
                      UiRect{{dot.x - 2.5f, dot.y - 2.5f}, {5.0f, 5.0f}},
                      UiColor{theme.danger.r, theme.danger.g, theme.danger.b,
                              theme.danger.a});
  }

  // Player center marker.
  const float centerX = x + mapSize / 2.0f;
  const float centerY = y + mapSize / 2.0f;
  drawList.FillRect(UiDrawLayer::Hud, kMinimapRootNode,
                    UiRect{{centerX - 4.0f, centerY - 4.0f}, {8.0f, 8.0f}},
                    UiColor{theme.success.r, theme.success.g, theme.success.b,
                            theme.success.a});
  drawList.StrokeRect(UiDrawLayer::Hud, kMinimapRootNode,
                      UiRect{{centerX - 4.0f, centerY - 4.0f}, {8.0f, 8.0f}},
                      UiColor{255, 255, 255, 255}, 1.0f);

  // North indicator.
  drawList.Text(UiDrawLayer::Hud, kMinimapRootNode, "N",
                {x + mapSize / 2.0f - 5.0f, y - 20.0f}, 18.0f,
                UiColor{theme.textSecondary.r, theme.textSecondary.g,
                        theme.textSecondary.b, theme.textSecondary.a},
                kGlobalFontResourceId);

  // Decorative frame.
  drawList.StrokeRect(UiDrawLayer::Hud, kMinimapRootNode,
                      UiRect{{x, y}, {mapSize, mapSize}},
                      UiColor{theme.panelBorderHighlight.r,
                              theme.panelBorderHighlight.g,
                              theme.panelBorderHighlight.b,
                              theme.panelBorderHighlight.a},
                      2.0f);

  // Zone name (bottom-right, right-aligned).
  if (m_hasZoneText) {
    drawList.Text(UiDrawLayer::Hud, kMinimapRootNode, m_zoneText,
                  {x + mapSize, y + mapSize + 10.0f}, 18.0f,
                  UiColor{theme.textHighlight.r, theme.textHighlight.g,
                          theme.textHighlight.b, theme.textHighlight.a},
                  kGlobalFontResourceId, UiTextAlign::Right);
  }

  // Kill count + portal arrow + affixes (below the zone name).
  if (m_hasKillText) {
    drawList.Text(UiDrawLayer::Hud, kMinimapRootNode, m_killText,
                  {x + mapSize, y + mapSize + 35.0f}, 16.0f,
                  m_killGoalReached
                      ? UiColor{theme.success.r, theme.success.g,
                                theme.success.b, theme.success.a}
                      : UiColor{theme.textSecondary.r, theme.textSecondary.g,
                                theme.textSecondary.b, theme.textSecondary.a},
                  kGlobalFontResourceId, UiTextAlign::Right);

    // Portal arrow: triangle approximated with three lines (draw-list Line
    // commands; the legacy filled triangle + glow are painted as a stroked
    // triangle + dim center dot — see evidence §R5 for the visual delta).
    if (m_hasPortalArrow) {
      const float arrowX = x + mapSize - 20.0f;
      const float arrowY = y + mapSize + 43.0f;
      const float size = 10.0f;
      const float c = std::cos(m_portalAngle);
      const float s = std::sin(m_portalAngle);
      UiVec2 tip{arrowX + c * size, arrowY + s * size};
      UiVec2 v1{arrowX + std::cos(m_portalAngle + 2.4f) * size * 0.6f,
                arrowY + std::sin(m_portalAngle + 2.4f) * size * 0.6f};
      UiVec2 v2{arrowX + std::cos(m_portalAngle - 2.4f) * size * 0.6f,
                arrowY + std::sin(m_portalAngle - 2.4f) * size * 0.6f};
      const UiColor success{theme.success.r, theme.success.g, theme.success.b,
                            theme.success.a};
      drawList.Line(UiDrawLayer::Hud, kMinimapRootNode, tip, v1, success, 1.0f);
      drawList.Line(UiDrawLayer::Hud, kMinimapRootNode, tip, v2, success, 1.0f);
      drawList.Line(UiDrawLayer::Hud, kMinimapRootNode, v1, v2, success, 1.0f);
      // Glow: dim center dot (approximation of the legacy circle gradient).
      drawList.FillRect(UiDrawLayer::Hud, kMinimapRootNode,
                        UiRect{{arrowX - 1.5f, arrowY - 1.5f}, {3.0f, 3.0f}},
                        UiColor{success.r, success.g, success.b, 77});
    }

    // Map affixes.
    float bonusY = y + mapSize + 65.0f;
    for (std::uint8_t i = 0; i < m_affixTextCount; ++i) {
      const UiColor c =
          m_affixPositive[i]
              ? UiColor{components::Colors::MAP_AFFIX_POSITIVE.r,
                        components::Colors::MAP_AFFIX_POSITIVE.g,
                        components::Colors::MAP_AFFIX_POSITIVE.b,
                        components::Colors::MAP_AFFIX_POSITIVE.a}
              : UiColor{components::Colors::MAP_AFFIX_NEGATIVE.r,
                        components::Colors::MAP_AFFIX_NEGATIVE.g,
                        components::Colors::MAP_AFFIX_NEGATIVE.b,
                        components::Colors::MAP_AFFIX_NEGATIVE.a};
      drawList.Text(UiDrawLayer::Hud, kMinimapRootNode, m_affixTexts[i],
                    {x + mapSize, bonusY}, 14.0f, c, kGlobalFontResourceId,
                    UiTextAlign::Right);
      bonusY += 18.0f;
    }
  }
}

} // namespace NoMoreDay::ui
