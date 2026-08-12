#include "game/application/ui/SkillHotbarController.hpp"

#include "core/logging/Logger.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UICommon.hpp"
#include "game/application/ui/UiResourceIds.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>

namespace NoMoreDay::ui {

namespace {
// Stable node id for the hotbar/buff draw commands (Hud layer).
inline constexpr UiId kSkillHotbarNode =
    static_cast<UiId>(0x1C5A3F2Du); // hashed "ui_skill_hotbar"
inline constexpr UiColor kHotbarBgColor{0, 0, 0, 102};       // Fade(BLACK,0.4)
inline constexpr UiColor kHotbarBorderColor{169, 169, 169, 255}; // DARKGRAY
inline constexpr UiColor kHotbarIconDim{100, 100, 255, 255};
inline constexpr UiColor kHotbarIconCooldownGray{128, 128, 128, 179};
inline constexpr UiColor kHotbarKeyColor{169, 169, 169, 255};
inline constexpr UiColor kHotbarKeyHighlight{255, 215, 0, 255};
inline constexpr UiColor kHotbarManaCost{135, 206, 235, 255};
inline constexpr UiColor kHotbarCharges{255, 255, 255, 255};
inline constexpr UiColor kBuffTextGreen{0, 255, 0, 255};
inline constexpr UiColor kBuffTextRed{255, 0, 0, 255};
inline constexpr UiColor kBuffBorderRed{200, 40, 40, 255};
inline constexpr UiColor kBuffRingGreen{0, 255, 0, 102};
inline constexpr UiColor kBuffRingYellow{255, 255, 0, 102};
} // namespace

SkillHotbarController::SkillHotbarController(UiRuntime& runtime,
                                             TooltipController* tooltipController,
                                             GameUiHost* uiHost)
    : m_tooltip(tooltipController), m_uiHost(uiHost), m_runtime(runtime) {
  UiNodeDesc desc;
  desc.id = kSkillHotbarNode;
  desc.parent = kRootUiId;
  // Full-viewport overlay anchor. Paint emits draw-list commands (Hud layer);
  // the node is a declarative root for host-driven layout.
  desc.layout.kind = UiLayoutKind::Overlay;
  desc.layout.width = UiLength::Fraction(1.0f);
  desc.layout.height = UiLength::Fraction(1.0f);
  desc.layout.horizontalAlignment = UiAlignment::Start;
  desc.layout.verticalAlignment = UiAlignment::Start;
  desc.visible = true;
  // The hotbar must never intercept the gameplay mouse.
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

UIDragSession& SkillHotbarController::DragSession() noexcept {
  return m_uiHost ? m_uiHost->DragSession() : m_localDragSession;
}

void SkillHotbarController::EnterGameplay() {
  m_inGameplay = true;
  m_hasPlayerData = false;
  SetVisible(true);
}

void SkillHotbarController::LeaveGameplay() {
  m_inGameplay = false;
  m_hasPlayerData = false;
  SetVisible(false);
}

void SkillHotbarController::SetVisible(bool visible) {
  m_visible = visible;
  if (m_rootNodeId != kInvalidUiId) {
    (void)m_runtime.SetNodeVisible(m_rootNodeId, visible);
  }
}

void SkillHotbarController::Update(const GameUiSnapshot& snapshot,
                                   const UiInputFrame& input) {
  m_hasPlayerData = snapshot.player.hasPlayer;
  if (!m_hasPlayerData) {
    m_lastRevision = snapshot.revision;
    return;
  }

  // Rebuild the display caches when the frame data changed (revision).
  if (m_lastRevision != snapshot.revision) {
    m_lastRevision = snapshot.revision;
    CacheFromSnapshot(snapshot);
  }

  ProcessInteraction(snapshot, input);
}

void SkillHotbarController::CacheFromSnapshot(const GameUiSnapshot& snapshot) {
  // Slots: snapshot.skillBar.slots holds only non-empty slots (id != 0), keyed
  // by slotIndex. Reset the cache and copy the display data in.
  for (SlotCache& slot : m_slotCache) {
    slot = SlotCache{};
  }
  for (const GameUiSkillBarSlotView& slotView : snapshot.skillBar.slots) {
    if (slotView.slotIndex < 0 ||
        slotView.slotIndex >= static_cast<std::int32_t>(m_slotCache.size())) {
      continue;
    }
    SlotCache& slot = m_slotCache[slotView.slotIndex];
    slot.iconAssetId = slotView.iconId;
    slot.remainingCooldown = slotView.cooldown;
    slot.currentCharges = slotView.currentCharges;
    slot.manaCost = slotView.manaCost;
    slot.maxCharges = slotView.maxCharges;
    if (slotView.cooldownMax > 0.0f) {
      slot.cooldownRatio =
          std::clamp(slotView.cooldown / slotView.cooldownMax, 0.0f, 1.0f);
    }
    slot.hasEnoughMana = snapshot.player.mana >= slotView.manaCost;
  }

  // Buffs: bounded copy of the snapshot buff strip (buffs left, debuffs right).
  m_buffCount = 0;
  for (const GameUiBuffView& buffView : snapshot.buffs) {
    if (m_buffCount >= m_buffCache.size()) {
      break; // Capped; overflow is bounded by the gameplay effect cap.
    }
    BuffCache& buff = m_buffCache[m_buffCount++];
    buff.iconAssetId = buffView.iconAssetId;
    buff.iconText = nullptr;
    buff.stacks = buffView.stacks;
    buff.isDebuff = buffView.isDebuff;
    if (buffView.duration > 0.0f) {
      buff.ratio =
          std::clamp(buffView.remaining / buffView.duration, 0.0f, 1.0f);
    }
  }
}

void SkillHotbarController::ProcessInteraction(const GameUiSnapshot& snapshot,
                                               const UiInputFrame& input) {
  (void)snapshot;
  m_hoveredSlot = -1;
  m_hoveredBuff = -1;
  const UiVec2 mouseLogical = input.pointer.logicalPosition;

  // --- Slot interaction (hover / drag-drop / right-click) ---
  const float slotSize = 54.0f;
  const float padding = 8.0f;
  const float totalW = (slotSize * 5) + (padding * 4);
  const float startX = (UI_REF_WIDTH - totalW) / 2.0f;
  const float startY = UI_REF_HEIGHT - slotSize - 20.0f;

  for (int i = 0; i < 5; ++i) {
    const float x = startX + i * (slotSize + padding);
    const float y = startY;
    const bool isHovered =
        mouseLogical.x >= x && mouseLogical.x < x + slotSize &&
        mouseLogical.y >= y && mouseLogical.y < y + slotSize;
    if (!isHovered) {
      continue;
    }
    m_hoveredSlot = i;

    // U7 group 6-B: hover routes through the hosted tooltip controller.
    if (m_tooltip) {
      m_tooltip->SetHoveredSkillSlot(i);
    }
    // U8 收尾: mouse-over-UI 门控经 host 实例成员。
    if (m_uiHost) {
      m_uiHost->SetMouseOverUI(true);
    }

    // R8: drop enqueues a SkillAssign intent (handled by the command handler
    // on the next Update) instead of mutating ActiveSkillsComponent directly.
    // The drag session (UI-local) is cleared here, exactly like the legacy
    // drop path.
    UIDragSession& drag = DragSession();
    if (drag.isDraggingSkill && input.pointer.released) {
      if (drag.draggedSkillId != INVALID_SKILL_ID && m_uiHost) {
        GameUiIntent intent;
        intent.sourceNode = NodeId();
        intent.kind = GameUiIntentKind::SkillAssign;
        intent.payload.skillId = drag.draggedSkillId;
        intent.payload.skillTarget =
            static_cast<std::uint8_t>(GameUiSkillTarget::Hotbar);
        intent.payload.sourceSlot = i;
        m_uiHost->EnqueueIntent(std::move(intent));
        LOG_INFO("Enqueued SkillAssign for skill {} to hotbar slot {}",
                 drag.draggedSkillId, i);
      }
      drag.isDraggingSkill = false;
      drag.draggedSkillId = INVALID_SKILL_ID;
    }

    // Right-click context menu.
    if (input.pointer.pressedRight) {
      if (m_uiHost) {
        m_uiHost->OpenSkillContextMenu(i);
      }
    }
  }

  // --- Buff hover ---
  const float barTopY = UI_REF_HEIGHT - 30.0f - 28.0f;
  const float barWidth = 450.0f;
  const float hotbarW = (slotSize * 5) + (padding * 4);
  const float hotbarLeft = (UI_REF_WIDTH - hotbarW) / 2.0f;
  const float hotbarRight = hotbarLeft + hotbarW;
  const float hpLeftX = hotbarLeft - 50.0f - barWidth;
  const float manaLeftX = hotbarRight + 50.0f;
  const float iconSize = 40.0f;
  const int maxPerRow = 10;

  int currentBuffs = 0;
  int currentDebuffs = 0;
  for (std::size_t i = 0; i < m_buffCount; ++i) {
    const BuffCache& buff = m_buffCache[i];
    const int count = buff.isDebuff ? currentDebuffs++ : currentBuffs++;
    const float startX = buff.isDebuff ? manaLeftX : hpLeftX;
    const int row = count / maxPerRow;
    const int col = count % maxPerRow;
    const float x = startX + col * (iconSize + 4.0f);
    const float y = barTopY - 10.0f - (row + 1) * (iconSize + 4.0f);
    if (mouseLogical.x >= x && mouseLogical.x < x + iconSize &&
        mouseLogical.y >= y && mouseLogical.y < y + iconSize) {
      m_hoveredBuff = static_cast<int>(i);
      if (m_tooltip) {
        m_tooltip->SetHoveredBuff(static_cast<int>(i));
      }
      if (m_uiHost) {
        m_uiHost->SetMouseOverUI(true);
      }
    }
  }
}

void SkillHotbarController::Paint(UiDrawList& drawList,
                                  const UiViewport& viewport) const {
  if (!m_hasPlayerData || !m_visible) {
    return;
  }
  (void)viewport; // Layout is in the fixed 2K reference logical space.

  const char* labels[] = {"Q", "W", "E", "R", "RMB"};
  const float slotSize = 54.0f;
  const float padding = 8.0f;
  const float totalW = (slotSize * 5) + (padding * 4);
  const float startX = (UI_REF_WIDTH - totalW) / 2.0f;
  const float startY = UI_REF_HEIGHT - slotSize - 20.0f;

  for (int i = 0; i < 5; ++i) {
    const float x = startX + i * (slotSize + padding);
    const float y = startY;
    const SlotCache& slot = m_slotCache[i];

    // Slot background + border.
    drawList.FillRect(UiDrawLayer::Hud, kSkillHotbarNode,
                      UiRect{{x, y}, {slotSize, slotSize}},
                      kHotbarBgColor);
    drawList.StrokeRect(UiDrawLayer::Hud, kSkillHotbarNode,
                        UiRect{{x, y}, {slotSize, slotSize}},
                        kHotbarBorderColor, 1.0f);

    if (slot.iconAssetId != 0) {
      UiColor iconTint{255, 255, 255, 255};
      if (slot.cooldownRatio > 0.0f && slot.currentCharges == 0) {
        iconTint = kHotbarIconCooldownGray;
      } else if (!slot.hasEnoughMana) {
        iconTint = kHotbarIconDim;
      }
      drawList.Image(UiDrawLayer::Hud, kSkillHotbarNode,
                     UiRect{{x + 4.0f, y + 6.0f}, {slotSize - 8.0f,
                                                   slotSize - 8.0f}},
                     slot.iconAssetId, iconTint);
    }

    // Cooldown sweep (top-down darkening proportional to the ratio).
    if (slot.cooldownRatio > 0.0f && slot.remainingCooldown > 0.0f) {
      const float coverHeight = slotSize * slot.cooldownRatio;
      drawList.FillRect(UiDrawLayer::Hud, kSkillHotbarNode,
                        UiRect{{x, y}, {slotSize, coverHeight}},
                        UiColor{0, 0, 0, 128});
    }

    // Remaining cooldown text.
    if (slot.remainingCooldown > 0.0f) {
      char timeStr[16];
      std::snprintf(timeStr, sizeof(timeStr), "%.1f", slot.remainingCooldown);
      drawList.Text(UiDrawLayer::Hud, kSkillHotbarNode, timeStr,
                    {x + slotSize * 0.5f, y + slotSize * 0.5f - 10.0f},
                    24.0f, UiColor{255, 215, 0, 255}, kGlobalFontResourceId,
                    UiTextAlign::Center);
    }

    // Key label.
    drawList.Text(UiDrawLayer::Hud, kSkillHotbarNode, labels[i],
                  {x + 4.0f, y + 2.0f}, 12.0f,
                  (i == m_hoveredSlot) ? kHotbarKeyHighlight : kHotbarKeyColor,
                  kGlobalFontResourceId);

    // Mana cost.
    if (slot.manaCost > 0.0f) {
      char manaStr[16];
      std::snprintf(manaStr, sizeof(manaStr), "%.0f", slot.manaCost);
      drawList.Text(UiDrawLayer::Hud, kSkillHotbarNode, manaStr,
                    {x + 4.0f, y + slotSize - 14.0f}, 11.0f,
                    slot.hasEnoughMana ? kHotbarManaCost : kHotbarBorderColor,
                    kGlobalFontResourceId);
    }

    // Charges.
    if (slot.maxCharges > 1) {
      char chargeStr[8];
      std::snprintf(chargeStr, sizeof(chargeStr), "%d", slot.currentCharges);
      drawList.Text(UiDrawLayer::Hud, kSkillHotbarNode, chargeStr,
                    {x + slotSize - 14.0f, y + slotSize - 14.0f}, 13.0f,
                    kHotbarCharges, kGlobalFontResourceId);
    }
  }

  // --- Buff / debuff strip ---
  const float barTopY = UI_REF_HEIGHT - 30.0f - 28.0f;
  const float barWidth = 450.0f;
  const float hotbarW = (slotSize * 5) + (padding * 4);
  const float hotbarLeft = (UI_REF_WIDTH - hotbarW) / 2.0f;
  const float hotbarRight = hotbarLeft + hotbarW;
  const float hpLeftX = hotbarLeft - 50.0f - barWidth;
  const float manaLeftX = hotbarRight + 50.0f;
  const float iconSize = 40.0f;
  const int maxPerRow = 10;

  int currentBuffs = 0;
  int currentDebuffs = 0;
  for (std::size_t i = 0; i < m_buffCount; ++i) {
    const BuffCache& buff = m_buffCache[i];
    const int count = buff.isDebuff ? currentDebuffs++ : currentBuffs++;
    const float startX = buff.isDebuff ? manaLeftX : hpLeftX;
    const int row = count / maxPerRow;
    const int col = count % maxPerRow;
    const float x = startX + col * (iconSize + 4.0f);
    const float y = barTopY - 10.0f - (row + 1) * (iconSize + 4.0f);

    // Icon background.
    drawList.FillRect(UiDrawLayer::Hud, kSkillHotbarNode,
                      UiRect{{x, y}, {iconSize, iconSize}},
                      UiColor{20, 20, 26, 179});
    if (buff.iconAssetId != 0) {
      drawList.Image(UiDrawLayer::Hud, kSkillHotbarNode,
                     UiRect{{x + 2.0f, y + 2.0f},
                            {iconSize - 4.0f, iconSize - 4.0f}},
                     buff.iconAssetId, UiColor{255, 255, 255, 255});
    } else if (buff.iconText != nullptr && buff.iconText[0] != '\0') {
      drawList.Text(UiDrawLayer::Hud, kSkillHotbarNode, buff.iconText,
                    {x + iconSize * 0.5f, y + iconSize * 0.5f - 8.0f}, 16.0f,
                    buff.isDebuff ? kBuffTextRed : kBuffTextGreen,
                    kGlobalFontResourceId, UiTextAlign::Center);
    }

    // Duration ring (top-down overlay approximating the legacy DrawRing).
    if (buff.ratio > 0.0f) {
      const float coverHeight = iconSize * (1.0f - buff.ratio);
      drawList.FillRect(UiDrawLayer::Hud, kSkillHotbarNode,
                        UiRect{{x, y}, {iconSize, coverHeight}},
                        UiColor{0, 0, 0, 96});
    }

    // Stacks.
    if (buff.stacks > 1) {
      char stackStr[8];
      std::snprintf(stackStr, sizeof(stackStr), "%d", buff.stacks);
      drawList.Text(UiDrawLayer::Hud, kSkillHotbarNode, stackStr,
                    {x + iconSize - 14.0f, y + iconSize - 14.0f}, 12.0f,
                    kHotbarCharges, kGlobalFontResourceId);
    }

    // Border.
    drawList.StrokeRect(UiDrawLayer::Hud, kSkillHotbarNode,
                        UiRect{{x, y}, {iconSize, iconSize}},
                        buff.isDebuff ? kBuffBorderRed : kHotbarBorderColor,
                        1.0f);
  }
}

bool SkillHotbarController::IsVisible() const noexcept { return m_visible; }

bool SkillHotbarController::IsInGameplay() const noexcept {
  return m_inGameplay;
}

bool SkillHotbarController::HasPlayerData() const noexcept {
  return m_hasPlayerData;
}

UiId SkillHotbarController::NodeId() const noexcept { return m_rootNodeId; }

} // namespace NoMoreDay::ui
