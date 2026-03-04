#include "game/systems/ui/UIMinimap.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/UIRenderer.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/WorldState.hpp" // Added
#include "game/data/AffixMapping.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/world/FogOfWarSystem.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "engine/physics/SpatialGrid.hpp"
#include "raylib.h"
#include <vector>
#include <algorithm>

using namespace NoMoreDay;

void UIMinimap::Cleanup() {
  if (s_minimapTexture.id != 0) {
    UnloadTexture(s_minimapTexture);
    s_minimapTexture.id = 0;
  }
  s_minimapPixels.clear();
  s_minimapPixels.shrink_to_fit();
  s_partialBuffer.clear();
  s_partialBuffer.shrink_to_fit();
}

void UIMinimap::ToggleDebugReveal() {
  s_debugRevealMap = !s_debugRevealMap;
  s_minimapDirty = true;
  LOG_INFO("Minimap Debug Reveal: {}", s_debugRevealMap ? "ON" : "OFF");
}

void UIMinimap::Draw(entt::registry &registry,
                     const LevelManager &levelManager, const NoMoreDay::systems::SpatialHashGrid* grid) {
  const auto &map = levelManager.getMapSystem();
  const auto &fog = levelManager.getFogSystem();

  // Scale
  float scale = UIRenderer::GetScale();
  auto &theme = UIRenderer::GetTheme();
  Font font = UISystem::GetFont();

  // Layout in Logic Space
  const float mapSize = Constants::MAP_SIZE;
  const float margin = Constants::MARGIN;
  const float x = UI_REF_WIDTH - mapSize - margin;
  const float y = margin;

  // Helpers
  auto DrawRectScaled = [&](float x, float y, float w, float h, Color c) {
    DrawRectangle((int)(x * scale), (int)(y * scale), (int)(w * scale),
                  (int)(h * scale), c);
  };
  auto DrawRectLinesScaled = [&](float x, float y, float w, float h,
                                 float thick, Color c) {
    DrawRectangleLinesEx({x * scale, y * scale, w * scale, h * scale},
                         thick * scale, c);
  };

  // Background Shadow/Border
  DrawRectScaled(x - 4, y - 4, mapSize + 8, mapSize + 8, theme.panelBackground);
  DrawRectLinesScaled(x - 4, y - 4, mapSize + 8, mapSize + 8, 1.0f,
                      theme.panelBorder);

  int gridW = fog.getWidth();
  int gridH = fog.getHeight();
  if (gridW == 0 || gridH == 0)
    return;

  auto viewRect = registry.view<PlayerTag, Position>();
  if (viewRect.begin() == viewRect.end())
    return;

  entt::entity playerEntity = viewRect.front();
  const auto &playerPos = viewRect.get<Position>(playerEntity);

  int playerGx = static_cast<int>(playerPos.x / FogOfWarSystem::TILE_SIZE);
  int playerGy = static_cast<int>(playerPos.y / FogOfWarSystem::TILE_SIZE);
  const int viewRadius = Constants::VIEW_RADIUS;
  float minimapScale = mapSize / (float)(viewRadius * 2);

  // Initialize Texture
  if (s_minimapTexture.id == 0 || s_minimapW != gridW || s_minimapH != gridH) {
    if (s_minimapTexture.id != 0)
      UnloadTexture(s_minimapTexture);
    s_minimapW = gridW;
    s_minimapH = gridH;
    s_minimapPixels.assign(gridW * gridH, BLACK); // Clear full buffer
    // Create texture with BLACK initially
    Image img = GenImageColor(gridW, gridH, BLACK);
    s_minimapTexture = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(s_minimapTexture, TEXTURE_FILTER_POINT);
    s_minimapDirty = true;
  }

  // Optimize: Partial Texture Update logic
  // Update frequency can be per frame if we only update small region
  static float minimapRefreshTimer = 0.0f;
  minimapRefreshTimer += GetFrameTime();
  
  if (minimapRefreshTimer >= Constants::REFRESH_INTERVAL || s_minimapDirty) {
      minimapRefreshTimer = 0.0f;
      
      // Determine update bounds (clamped to grid)
      int minGx = std::max(0, playerGx - viewRadius);
      int maxGx = std::min(gridW, playerGx + viewRadius + 1);
      int minGy = std::max(0, playerGy - viewRadius);
      int maxGy = std::min(gridH, playerGy + viewRadius + 1);
      
      int uWidth = maxGx - minGx;
      int uHeight = maxGy - minGy;

      if (uWidth > 0 && uHeight > 0) {
          // Resize partial buffer
          if (s_partialBuffer.size() < (size_t)(uWidth * uHeight)) {
              s_partialBuffer.resize(uWidth * uHeight);
          }

          // Only iterate potentially visible area
          // This loop is now Small (60x60 = 3600 iterations) instead of Full Map (500x500 = 250k iterations)
          for (int ly = 0; ly < uHeight; ++ly) {
              int gy = minGy + ly;
              for (int lx = 0; lx < uWidth; ++lx) {
                  int gx = minGx + lx;
                  
                  // int mapIndex = gy * gridW + gx; // Only needed if we update s_minimapPixels global cache relative to map

                  Color c = BLACK;
                  bool isExplored = fog.isExplored(gx, gy);
                  
                  if (isExplored || s_debugRevealMap) {
                      bool isVisible = s_debugRevealMap ? true : fog.isVisible(gx, gy);
                      if (map.isWalkable(gx, gy)) {
                          c = isVisible ? Color{160, 160, 160, 255} : Color{60, 60, 60, 255};
                      } else {
                          c = isVisible ? Color{80, 80, 80, 255} : Color{30, 30, 30, 255};
                      }
                  }
                  
                  s_partialBuffer[ly * uWidth + lx] = c;
              }
          }
          
          // Use UpdateTextureRec to upload ONLY the changed region
          Rectangle updateRect = { (float)minGx, (float)minGy, (float)uWidth, (float)uHeight };
          UpdateTextureRec(s_minimapTexture, updateRect, s_partialBuffer.data());
      }
      s_minimapDirty = false;
  }

  // Draw Map Texture
  Rectangle sourceRec = {(float)playerGx - viewRadius,
                         (float)playerGy - viewRadius, (float)viewRadius * 2,
                         (float)viewRadius * 2};
  Rectangle destRec = {x * scale, y * scale, mapSize * scale, mapSize * scale};
  DrawTexturePro(s_minimapTexture, sourceRec, destRec, {0, 0}, 0.0f, WHITE);

  // Scanline / Overlay effect
  DrawRectangleGradientV((int)(x * scale), (int)(y * scale),
                         (int)(mapSize * scale), (int)(mapSize * scale),
                         Fade(WHITE, 0.05f), Fade(BLACK, 0.1f));

  // 绘制怪物 (Draw Enemies)
  if (grid) {
     // Use Spatial Grid Query if available
     // Search slightly larger than view to clip smoothly
     float worldViewR = (float)viewRadius * FogOfWarSystem::TILE_SIZE * 1.5f; 
     
     // Callback based query
     grid->query(Position{playerPos.x, playerPos.y}, worldViewR, [&](entt::entity entity, const Position& enemyPos) {
         if (registry.valid(entity) && registry.all_of<EnemyTag>(entity)) {
             // KilledTag check is done in valid? No, need to check exclude
             if (registry.any_of<KilledTag>(entity) || registry.any_of<DormantTag>(entity)) return;

             // Logic pos check
             float dx = (enemyPos.x - playerPos.x) / FogOfWarSystem::TILE_SIZE;
             float dy = (enemyPos.y - playerPos.y) / FogOfWarSystem::TILE_SIZE;
             const int enemyGx =
                 static_cast<int>(enemyPos.x / FogOfWarSystem::TILE_SIZE);
             const int enemyGy =
                 static_cast<int>(enemyPos.y / FogOfWarSystem::TILE_SIZE);
             if (!s_debugRevealMap && !fog.isVisible(enemyGx, enemyGy)) {
                 return;
             }
             
             if (std::abs(dx) <= viewRadius && std::abs(dy) <= viewRadius) {
                 float logicX = x + (dx + viewRadius) * minimapScale;
                 float logicY = y + (dy + viewRadius) * minimapScale;
                 DrawCircle((int)(logicX * scale), (int)(logicY * scale),
                            Constants::ENEMY_MARKER_SIZE * scale, theme.danger);
             }
         }
     });
  } else {
      // Fallback to full iteration (slow)
      auto enemyView = registry.view<EnemyTag, Position>(entt::exclude<KilledTag, DormantTag>);
      for (auto entity : enemyView) {
        const auto &enemyPos = enemyView.get<Position>(entity);
        float dx = (enemyPos.x - playerPos.x) / FogOfWarSystem::TILE_SIZE;
        float dy = (enemyPos.y - playerPos.y) / FogOfWarSystem::TILE_SIZE;
        const int enemyGx =
            static_cast<int>(enemyPos.x / FogOfWarSystem::TILE_SIZE);
        const int enemyGy =
            static_cast<int>(enemyPos.y / FogOfWarSystem::TILE_SIZE);
        if (!s_debugRevealMap && !fog.isVisible(enemyGx, enemyGy)) {
          continue;
        }
        if (std::abs(dx) <= viewRadius && std::abs(dy) <= viewRadius) {
          float logicX = x + (dx + viewRadius) * minimapScale;
          float logicY = y + (dy + viewRadius) * minimapScale;
          DrawCircle((int)(logicX * scale), (int)(logicY * scale),
                     Constants::ENEMY_MARKER_SIZE * scale, theme.danger);
        }
      }
  }

  // Player Center (Marker)
  float centerX = x + mapSize / 2.0f;
  float centerY = y + mapSize / 2.0f;
  DrawCircle((int)(centerX * scale), (int)(centerY * scale),
             Constants::PLAYER_MARKER_SIZE * scale, theme.success);
  DrawCircleLines((int)(centerX * scale), (int)(centerY * scale),
                  Constants::PLAYER_MARKER_SIZE * scale, WHITE);

  // North Indicator
  UIRenderer::DrawTextUI(font, "N", x + mapSize / 2.0f - 5.0f, y - 20.0f, 18,
                         theme.textSecondary, 1.0f);

  // Decorative Frame
  DrawRectLinesScaled(x, y, mapSize, mapSize, 2.0f, theme.panelBorderHighlight);

  // Coordinates or Zone Name
  char zoneBuf[128];
  const char* baseName = (levelManager.getCurrentBiomeID() == NoMoreDay::BiomeID::Town) ? "宁静村落" : "地下城";
  
  if (levelManager.getCurrentBiomeID() == NoMoreDay::BiomeID::Town) {
      utils::FormatToBuffer(zoneBuf, "{}", baseName);
  } else {
      int displayLevel = levelManager.getCurrentLevel();
      // Check for Dimensional State for accurate difficulty level
      if (registry.ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
          const auto& state = registry.ctx().get<NoMoreDay::ActiveDimensionalState>();
          if (state.isActive) {
              int difficultyLv = state.selectedBaseLevel + (state.currentDepth - 1);
              utils::FormatToBuffer(zoneBuf, "异界 - {}层 [Lv.{}]",
                                    state.currentDepth, difficultyLv);
          } else {
              utils::FormatToBuffer(zoneBuf, "{} - {}层", baseName,
                                    displayLevel);
          }
      } else {
          utils::FormatToBuffer(zoneBuf, "{} - {}层", baseName,
                                displayLevel);
      }
  }
  const char* zoneName = zoneBuf;

  float tw = IsFontValid(font) ? MeasureTextEx(font, zoneName, 18, 1.0f).x
                               : (float)MeasureText(zoneName, 18);
  UIRenderer::DrawTextUI(font, zoneName, x + mapSize - tw, y + mapSize + 10.0f,
                         18, theme.textHighlight, 1.0f);

  // 1. Kill Count (Below Minimap Level Name)
  auto *pStats = registry.try_get<PlayerStats>(playerEntity);
  if (pStats && levelManager.getCurrentBiomeID() != NoMoreDay::BiomeID::Town) {
    using namespace NoMoreDay::Constants::Enemy;
    char killBuf[64];
    Color killColor =
        (pStats->current_map_kills >= NEXT_LEVEL_PORTAL_KILL_REQUIREMENT)
            ? theme.success
            : theme.textSecondary;

    if (pStats->current_map_kills < NEXT_LEVEL_PORTAL_KILL_REQUIREMENT) {
      utils::FormatToBuffer(killBuf, "击杀: {} / {}",
                            pStats->current_map_kills,
                            NEXT_LEVEL_PORTAL_KILL_REQUIREMENT);
    } else {
      utils::FormatToBuffer(killBuf, "击杀: {} (出口已标位)",
                            pStats->current_map_kills);
    }

    float killTw = IsFontValid(font) ? MeasureTextEx(font, killBuf, 16, 1.0f).x
                                     : (float)MeasureText(killBuf, 16);
    UIRenderer::DrawTextUI(font, killBuf, x + mapSize - killTw,
                           y + mapSize + 35.0f, 16, killColor, 1.0f);

    // 2. Navigation Arrow (Points to NextLevel Portal)
    if (pStats->current_map_kills >= NEXT_LEVEL_PORTAL_KILL_REQUIREMENT) {
      entt::entity exitPortal = entt::null;
      auto portalView = registry.view<PortalComponent, Position>();
      for (auto e : portalView) {
        if (portalView.get<PortalComponent>(e).type == PortalType::NextLevel) {
          exitPortal = e;
          break;
        }
      }

      if (exitPortal != entt::null) {
        const auto &portalPos = portalView.get<Position>(exitPortal);
        float dx = portalPos.x - playerPos.x;
        float dy = portalPos.y - playerPos.y;
        float angle = atan2f(dy, dx);

        // Draw rotating arrow next to text
        float arrowX = x + mapSize - killTw - 20.0f;
        float arrowY = y + mapSize + 43.0f;

        // Draw simple arrow head pointing towards portal
        Vector2 center = {arrowX * scale, arrowY * scale};
        float arrowSize = 10.0f * scale;
        Vector2 v1 = {center.x + cosf(angle) * arrowSize,
                      center.y + sinf(angle) * arrowSize};
        Vector2 v2 = {center.x + cosf(angle + 2.4f) * arrowSize * 0.6f,
                      center.y + sinf(angle + 2.4f) * arrowSize * 0.6f};
        Vector2 v3 = {center.x + cosf(angle - 2.4f) * arrowSize * 0.6f,
                      center.y + sinf(angle - 2.4f) * arrowSize * 0.6f};

        DrawTriangle(v1, v2, v3, theme.success);
        // Glow effect
        DrawCircleGradient((int)center.x, (int)center.y, arrowSize * 1.5f,
                           Fade(theme.success, 0.3f), BLANK);
      }
    }

    // 3. Map Affixes (Bonuses/Modifiers)
    if (levelManager.isMosaicLevel()) {
      const auto &resonance = levelManager.getCurrentResonance();
      float bonusY = y + mapSize + 65.0f;

      auto drawBonus = [&](const char *label, float value, bool isPositive) {
        char buf[64];
        utils::FormatToBuffer(buf, "{}: {:+.0f}%", label,
                              (value - 1.0f) * 100.0f);
        Color c = isPositive ? components::Colors::MAP_AFFIX_POSITIVE
                             : components::Colors::MAP_AFFIX_NEGATIVE;
        float tw = MeasureTextEx(font, buf, 14, 1.0f).x;
        UIRenderer::DrawTextUI(font, buf, x + mapSize - tw, bonusY, 14, c,
                               1.0f);
        bonusY += 18.0f;
      };

      if (resonance.totalEnemyDensity != 1.0f) {
        // Density buff (value > 1.0) is negative for player (more enemies)
        drawBonus("怪物密度", resonance.totalEnemyDensity,
                  resonance.totalEnemyDensity < 1.0f);
      }
      if (resonance.totalDropRate != 1.0f) {
        drawBonus("物品掉落", resonance.totalDropRate,
                  resonance.totalDropRate > 1.0f);
      }
      if (resonance.totalLevelMod != 0) {
        char buf[64];
        utils::FormatToBuffer(buf, "怪物等级: {:+}",
                              resonance.totalLevelMod);
        float tw = MeasureTextEx(font, buf, 14, 1.0f).x;
        UIRenderer::DrawTextUI(font, buf, x + mapSize - tw, bonusY, 14,
                               components::Colors::MAP_AFFIX_NEGATIVE, 1.0f);
        bonusY += 18.0f;
      }

      // Dominant Element
      if (resonance.dominantElement != FragmentElement::None) {
        const char *elemName =
            FragmentElementzh[static_cast<size_t>(resonance.dominantElement)]
                .data();
        float tw = MeasureTextEx(font, elemName, 14, 1.0f).x;
        UIRenderer::DrawTextUI(font, elemName, x + mapSize - tw, bonusY, 14,
                               GOLD, 1.0f);
        bonusY += 18.0f;
      }
    }
  }
}
