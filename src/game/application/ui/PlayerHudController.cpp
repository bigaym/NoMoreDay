#include "game/application/ui/PlayerHudController.hpp"

#include "game/application/ui/UICommon.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiResourceIds.hpp"
#include "game/foundation/data/BladeMasteryData.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace NoMoreDay::ui {

namespace {

// Stable node id for the HUD draw commands (Hud layer).
inline constexpr UiId kPlayerHudNode =
    static_cast<UiId>(0x7E3A5B91u); // hashed "ui_player_hud"
inline constexpr UiColor kHudBarBgColor{0, 0, 0, 153};       // Fade(BLACK,0.6)
inline constexpr UiColor kHudBarBorderColor{169, 169, 169, 255}; // DARKGRAY
inline constexpr UiColor kHudHpColor{176, 48, 96, 255};      // MAROON
inline constexpr UiColor kHudManaColor{0, 0, 139, 255};      // DARKBLUE
inline constexpr UiColor kHudBarrierColor{102, 217, 232, 200};
inline constexpr UiColor kHudBarrierGlow{64, 160, 255, 255};
inline constexpr UiColor kHudTextWhite{255, 255, 255, 255};
inline constexpr UiColor kHudFpsGreen{0, 255, 0, 255};
inline constexpr UiColor kHudFpsYellow{255, 255, 0, 255};
inline constexpr UiColor kHudFpsRed{255, 0, 0, 255};
inline constexpr UiColor kHudFeedbackGold{255, 215, 0, 255};
inline constexpr UiColor kHudFeedbackBlue{135, 206, 235, 255};

const char* ResolveSummonDisplayName(uint32_t skillId, uint32_t archetypeId) {
    if (archetypeId == static_cast<uint32_t>(SummonArchetype::SpiritSword) ||
        skillId == 3) {
        return "飞剑";
    }
    if (archetypeId == static_cast<uint32_t>(SummonArchetype::ShadowEcho)) {
        return "Shadow Echo";
    }
    return "Summon";
}

const char* ResolveAttunementName(const uint8_t attunement) {
    switch (static_cast<BladeAttunement>(attunement)) {
    case BladeAttunement::Lightning:
        return "Lightning";
    case BladeAttunement::Frost:
        return "Frost";
    case BladeAttunement::Fire:
        return "Fire";
    default:
        return "None";
    }
}

const char* ResolveBladeResourceLabel(const uint8_t kind) {
    switch (static_cast<BladeResourceKind>(kind)) {
    case BladeResourceKind::SwordFlow:
        return "Sword Flow";
    case BladeResourceKind::SpiritBladeTier:
        return "Spirit Blade Tier";
    case BladeResourceKind::Bloodthirst:
        return "Bloodthirst";
    case BladeResourceKind::SwordIntent:
    default:
        return "Sword Intent";
    }
}

void FormatDurationLabel(char* out, size_t cap, const char* label,
                         const float duration) {
    std::snprintf(out, cap, "%s %.1fs", label, duration);
}

} // namespace

PlayerHudController::PlayerHudController(UiRuntime& runtime)
    : m_runtime(runtime) {
  UiNodeDesc desc;
  desc.id = kPlayerHudNode;
  desc.parent = kRootUiId;
  // Full-viewport HUD anchor. Paint emits draw-list commands (Hud layer); the
  // node is a declarative root for host-driven layout, spanning the viewport.
  desc.layout.kind = UiLayoutKind::Overlay;
  desc.layout.width = UiLength::Fraction(1.0f);
  desc.layout.height = UiLength::Fraction(1.0f);
  desc.layout.horizontalAlignment = UiAlignment::Start;
  desc.layout.verticalAlignment = UiAlignment::Start;
  desc.visible = true;
  // The gameplay HUD must never intercept the gameplay mouse.
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

void PlayerHudController::EnterGameplay() {
  m_inGameplay = true;
  m_hasPlayerData = false;
  SetVisible(true);
}

void PlayerHudController::LeaveGameplay() {
  m_inGameplay = false;
  m_hasPlayerData = false;
  SetVisible(false);
}

void PlayerHudController::SetVisible(bool visible) {
  m_visible = visible;
  if (m_rootNodeId != kInvalidUiId) {
    (void)m_runtime.SetNodeVisible(m_rootNodeId, visible);
  }
}

void PlayerHudController::Update(const GameUiSnapshot& snapshot, int fps,
                                 float timeSeconds) {
  m_hasPlayerData = snapshot.player.hasPlayer;
  m_lastTimeSeconds = timeSeconds;
  m_swordIntent.SetIconResourceId(kSwordIntentIconResourceId);

  const GameUiPlayerSnapshot& player = snapshot.player;
  if (!player.hasPlayer) {
    m_lastRevision = snapshot.revision;
    return;
  }

  // Rebuild the text caches only when the frame data changed (revision).
  if (m_lastRevision == snapshot.revision) {
    return;
  }
  m_lastRevision = snapshot.revision;

  // FPS text.
  std::snprintf(m_fpsText.data(), m_fpsText.size(), "FPS: %d", fps);

  // HP text: "cur / max" plus optional " (+barrier)".
  std::snprintf(m_hpText.data(), m_hpText.size(), "%d / %d",
                static_cast<int>(player.health),
                static_cast<int>(player.maxHealth));
  if (player.barrier > 0.0f) {
    char suffix[24];
    std::snprintf(suffix, sizeof(suffix), " (+%d)",
                  static_cast<int>(player.barrier));
    std::strncat(m_hpText.data(), suffix, m_hpText.size() - 1);
  }

  // Mana text.
  std::snprintf(m_manaText.data(), m_manaText.size(), "%d / %d",
                static_cast<int>(player.mana),
                static_cast<int>(player.maxMana));

  // Bar metrics (resolved here, painted in Paint without touching the data).
  m_hpPct = player.maxHealth > 0.0f
                ? std::clamp(player.health / player.maxHealth, 0.0f, 1.0f)
                : 0.0f;
  m_manaPct = player.maxMana > 0.0f
                  ? std::clamp(player.mana / player.maxMana, 0.0f, 1.0f)
                  : 0.0f;
  m_hasBarrier = player.barrier > 0.0f || player.maxBarrier > 0.0f;
  m_barrierDisplayValue = player.barrier;
  m_maxBarrier = player.maxBarrier;
  m_barrierPct = player.maxHealth > 0.0f
                     ? std::clamp(player.barrier / player.maxHealth, 0.0f, 1.0f)
                     : 0.0f;
  m_barrierOverflow = player.barrier > player.maxBarrier;

  // Blade widget state.
  m_hasBladeResource = player.hasBladeResource;
  m_hasSwordIntent = player.hasSwordIntent;
  m_showRestartReady = false;
  if (player.hasBladeResource) {
    const char* label = ResolveBladeResourceLabel(player.bladeResourceKind);
    m_bladeDetailText[0] = '\0';
    if (player.hasMastery) {
      if (player.bladeResourceKind ==
          static_cast<uint8_t>(BladeResourceKind::SpiritBladeTier)) {
        std::snprintf(m_bladeDetailText.data(), m_bladeDetailText.size(),
                      "Attunement: %s",
                      ResolveAttunementName(player.heavenlyAttunement));
      } else if (player.bladeResourceKind ==
                 static_cast<uint8_t>(BladeResourceKind::Bloodthirst)) {
        std::snprintf(m_bladeDetailText.data(), m_bladeDetailText.size(), "%s",
                      player.bloodOathActive ? "Blood Oath: Active"
                                             : "Blood Oath: Dormant");
      }
      // Runtime detail cue: the builder resolved the active heavenly field.
      if (player.bladeResourceKind ==
              static_cast<uint8_t>(BladeResourceKind::SpiritBladeTier) &&
          player.heavenlyFieldDuration > 0.0f) {
        char runtimeDetail[64];
        FormatDurationLabel(runtimeDetail, sizeof(runtimeDetail),
                            "Field Active", player.heavenlyFieldDuration);
        std::strncat(m_bladeDetailText.data(), " | ",
                     m_bladeDetailText.size() - std::strlen(m_bladeDetailText.data()) - 1);
        std::strncat(m_bladeDetailText.data(), runtimeDetail,
                     m_bladeDetailText.size() - std::strlen(m_bladeDetailText.data()) - 1);
      }
    }

    m_swordIntent.Update(player.bladeResourceCurrent, player.bladeResourceMax,
                         static_cast<BladeResourceKind>(player.bladeResourceKind),
                         label, m_bladeDetailText.data(), timeSeconds,
                         1.0f / 60.0f);

    // Feedback text (runtime cue first, then static cue).
    m_feedbackText[0] = '\0';
    const char* runtimeFeedback = nullptr;
    char runtimeFeedbackScratch[48];
    if (player.bladeResourceKind ==
            static_cast<uint8_t>(BladeResourceKind::Bloodthirst) &&
        player.bloodOathActive && player.maxHealth > 0.0f) {
      const float healthRatio =
          std::clamp(player.health / player.maxHealth, 0.0f, 1.0f);
      if (healthRatio <= 0.35f) {
        std::snprintf(runtimeFeedbackScratch, sizeof(runtimeFeedbackScratch),
                      "Danger: %.0f%% HP", healthRatio * 100.0f);
        runtimeFeedback = runtimeFeedbackScratch;
      } else if (player.bloodSeaHasVoidKeystone &&
                 player.bloodSeaMiasmaBonus > 0.0f) {
        runtimeFeedback = "Miasma Pressure";
      }
    }
    if (runtimeFeedback != nullptr && runtimeFeedback[0] != '\0') {
      std::strncpy(m_feedbackText.data(), runtimeFeedback,
                   m_feedbackText.size() - 1);
    } else if (player.bladeResourceKind ==
               static_cast<uint8_t>(BladeResourceKind::SwordFlow)) {
      if (player.restartWindowReady && player.restartWindowTimer > 0.0f) {
        m_showRestartReady = true;
        std::snprintf(m_feedbackText.data(), m_feedbackText.size(),
                      "Restart Ready %.1fs", player.restartWindowTimer);
      } else if (player.critBonusFeedbackTimer > 0.0f) {
        std::snprintf(m_feedbackText.data(), m_feedbackText.size(),
                      "%s", "暴击剑流 +1");
      }
    } else if (player.bladeResourceKind ==
                   static_cast<uint8_t>(BladeResourceKind::Bloodthirst) &&
               player.bloodOathActive &&
               player.bladeResourceCurrent >= 8) {
      std::snprintf(m_feedbackText.data(), m_feedbackText.size(), "%s",
                    "Danger: Blood Oath");
    }
  } else if (player.hasSwordIntent) {
    m_swordIntent.Update(player.swordIntentStacks, player.swordIntentMaxStacks,
                         BladeResourceKind::SwordIntent, "Sword Intent", "",
                         timeSeconds, 1.0f / 60.0f);
  }

  // Summon rows (bounded: the builder groups by key; copy into fixed slots).
  m_summonRowCount = 0;
  for (const GameUiSummonGroupView& group : player.summonGroups) {
    if (m_summonRowCount >= m_summonRows.size()) {
      break; // Capped; overflow is telemetry-free but bounded by construction.
    }
    SummonRow& row = m_summonRows[m_summonRowCount++];
    row.iconId = group.iconId;
    row.lifeRatio = group.maxLifeRatio;
    row.count = group.count;
    const char* baseName = ResolveSummonDisplayName(group.skillId,
                                                    group.archetypeId);
    if (group.count > 1) {
      std::snprintf(row.displayName.data(), row.displayName.size(), "%s x%u",
                    baseName, group.count);
    } else {
      std::snprintf(row.displayName.data(), row.displayName.size(), "%s",
                    baseName);
    }
  }
}

void PlayerHudController::Paint(UiDrawList& drawList,
                                const UiViewport& viewport) const {
  if (!m_hasPlayerData || !m_visible) {
    return;
  }
  (void)viewport; // Layout is in the fixed 2K reference logical space.

  // --- Logic Metrics (2K Reference, synced with the legacy HUD) ---
  const float hotbarW = (54.0f * 5) + (8.0f * 4); // 302
  const float hotbarLeft = (UI_REF_WIDTH - hotbarW) / 2.0f; // 1129
  const float hotbarRight = hotbarLeft + hotbarW;           // 1431
  const float barWidth = 450.0f;
  const float barHeight = 28.0f;
  const float margin = 50.0f;
  const float barBottomY = UI_REF_HEIGHT - 30.0f;
  const float barTopY = barBottomY - barHeight;

  // FPS counter (top-left).
  UiColor fpsColor = kHudFpsGreen;
  int fps = 0;
  if (std::sscanf(m_fpsText.data(), "FPS: %d", &fps) == 1) {
    if (fps < 30) fpsColor = kHudFpsRed;
    else if (fps < 60) fpsColor = kHudFpsYellow;
  }
  drawList.Text(UiDrawLayer::Hud, kPlayerHudNode, m_fpsText.data(),
                {10.0f, 10.0f}, 20.0f, fpsColor, kGlobalFontResourceId);

  // HP bar background + fill + border.
  const float hpRightX = hotbarLeft - margin;
  const float hpLeftX = hpRightX - barWidth;
  const float hpPct = std::clamp(m_hpPct, 0.0f, 1.0f);
  drawList.FillRect(UiDrawLayer::Hud, kPlayerHudNode,
                    UiRect{{hpLeftX, barTopY}, {barWidth, barHeight}},
                    kHudBarBgColor);
  drawList.FillRect(UiDrawLayer::Hud, kPlayerHudNode,
                    UiRect{{hpLeftX, barTopY}, {barWidth * hpPct, barHeight}},
                    kHudHpColor);
  drawList.StrokeRect(UiDrawLayer::Hud, kPlayerHudNode,
                      UiRect{{hpLeftX, barTopY}, {barWidth, barHeight}},
                      kHudBarBorderColor, 2.0f);

  // Barrier overlay (cyan) + overflow glow pulse.
  if (m_hasBarrier && m_barrierDisplayValue > 0.0f) {
    const float barrierPct = std::clamp(m_barrierPct, 0.0f, 1.0f);
    drawList.FillRect(UiDrawLayer::Hud, kPlayerHudNode,
                      UiRect{{hpLeftX, barTopY},
                             {barWidth * barrierPct, barHeight}},
                      kHudBarrierColor);
    if (m_barrierOverflow && m_maxBarrier > 0.0f) {
      const float pulse =
          (std::sin(m_lastTimeSeconds * 4.0f) + 1.0f) * 0.5f;
      const float glowAlpha = 0.3f + pulse * 0.4f;
      UiColor glow = kHudBarrierGlow;
      glow.a = static_cast<std::uint8_t>(glowAlpha * 255.0f);
      drawList.StrokeRect(UiDrawLayer::Hud, kPlayerHudNode,
                          UiRect{{hpLeftX, barTopY}, {barWidth, barHeight}},
                          glow, 3.0f);
    }
  }

  // HP text (left aligned inside the bar).
  drawList.Text(UiDrawLayer::Hud, kPlayerHudNode, m_hpText.data(),
                {hpLeftX + 10.0f, barTopY + 4.0f}, 18.0f, kHudTextWhite,
                kGlobalFontResourceId);

  // Mana bar (right of hotbar).
  const float manaLeftX = hotbarRight + margin;
  const float manaPct = std::clamp(m_manaPct, 0.0f, 1.0f);
  drawList.FillRect(UiDrawLayer::Hud, kPlayerHudNode,
                    UiRect{{manaLeftX, barTopY}, {barWidth, barHeight}},
                    kHudBarBgColor);
  drawList.FillRect(UiDrawLayer::Hud, kPlayerHudNode,
                    UiRect{{manaLeftX, barTopY},
                           {barWidth * manaPct, barHeight}},
                    kHudManaColor);
  drawList.StrokeRect(UiDrawLayer::Hud, kPlayerHudNode,
                      UiRect{{manaLeftX, barTopY}, {barWidth, barHeight}},
                      kHudBarBorderColor, 2.0f);
  drawList.Text(UiDrawLayer::Hud, kPlayerHudNode, m_manaText.data(),
                {manaLeftX + barWidth - 120.0f, barTopY + 4.0f}, 18.0f,
                kHudTextWhite, kGlobalFontResourceId);

  // Blade widget.
  if (m_hasBladeResource || m_hasSwordIntent) {
    m_swordIntent.Paint(drawList, viewport);
    if (m_feedbackText[0] != '\0') {
      const bool showRestartReady = m_showRestartReady;
      const float pulseSpeed = showRestartReady ? 10.0f : 12.0f;
      const float pulse = 0.7f + 0.3f * std::sin(m_lastTimeSeconds * pulseSpeed);
      UiColor feedbackColor = showRestartReady ? kHudFeedbackGold
                                               : kHudFeedbackBlue;
      feedbackColor.a = static_cast<std::uint8_t>(pulse * 255.0f);
      drawList.Text(UiDrawLayer::Hud, kPlayerHudNode, m_feedbackText.data(),
                    {UI_REF_WIDTH * 0.5f, UI_REF_HEIGHT - 248.0f}, 18.0f,
                    feedbackColor, kGlobalFontResourceId, UiTextAlign::Center);
    }
  }

  // Summon status (top-left).
  float startY = 40.0f;
  for (std::size_t i = 0; i < m_summonRowCount; ++i) {
    const SummonRow& row = m_summonRows[i];
    const float width = 150.0f;
    const float height = 40.0f;
    drawList.FillRect(UiDrawLayer::Hud, kPlayerHudNode,
                      UiRect{{10.0f, startY}, {width, height}},
                      UiColor{20, 20, 26, 204});
    drawList.StrokeRect(UiDrawLayer::Hud, kPlayerHudNode,
                        UiRect{{10.0f, startY}, {width, height}},
                        UiColor{70, 70, 80, 255}, 1.0f);
    if (row.iconId != 0) {
      drawList.Image(UiDrawLayer::Hud, kPlayerHudNode,
                     UiRect{{14.0f, startY + 4.0f}, {32.0f, 32.0f}},
                     static_cast<UiResourceId>(row.iconId),
                     UiColor{255, 255, 255, 255});
    } else {
      drawList.FillRect(UiDrawLayer::Hud, kPlayerHudNode,
                        UiRect{{14.0f, startY + 4.0f}, {32.0f, 32.0f}},
                        UiColor{64, 64, 64, 128});
    }
    // Life ratio bar.
    drawList.FillRect(UiDrawLayer::Hud, kPlayerHudNode,
                      UiRect{{52.0f, startY + height - 12.0f},
                             {90.0f, 6.0f}},
                      UiColor{0, 0, 0, 128});
    const UiColor barColor = row.lifeRatio > 0.3f
                                 ? UiColor{135, 206, 235, 204}
                                 : UiColor{255, 165, 0, 204};
    drawList.FillRect(UiDrawLayer::Hud, kPlayerHudNode,
                      UiRect{{52.0f, startY + height - 12.0f},
                             {90.0f * std::clamp(row.lifeRatio, 0.0f, 1.0f),
                              6.0f}},
                      barColor);
    // Name text.
    drawList.Text(UiDrawLayer::Hud, kPlayerHudNode, row.displayName.data(),
                  {52.0f, startY + 4.0f}, 14.0f, kHudTextWhite,
                  kGlobalFontResourceId);
    startY += 45.0f;
  }
}

bool PlayerHudController::IsVisible() const noexcept {
  return m_visible;
}

bool PlayerHudController::IsInGameplay() const noexcept {
  return m_inGameplay;
}

bool PlayerHudController::HasPlayerData() const noexcept {
  return m_hasPlayerData;
}

UiId PlayerHudController::NodeId() const noexcept {
  return m_rootNodeId;
}

} // namespace NoMoreDay::ui
