#include "game/application/ui/UICharacterController.hpp"

#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/GameUiIntent.hpp"
#include "game/application/ui/UICommon.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/application/ui/UiResourceIds.hpp"
#include "game/application/ui/UISystem.hpp"
#include "core/utils/FmtBuffer.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/combat/CombatConstants.hpp"
#include "raylib.h"

#include <algorithm>
#include <cstdint>

namespace NoMoreDay::ui {

namespace {

// Legacy panel constants (UICharacter.cpp, unchanged).
constexpr float kPanelW = 450.0f;
constexpr float kPanelH = 780.0f;
constexpr float kPanelMargin = 40.0f;
constexpr float kHeaderHeight = 60.0f;
constexpr float kPadding = 25.0f;
constexpr float kAvatarSize = 90.0f;
constexpr float kRowGap = 36.0f;   // 28px row + 8px gap.
constexpr float kBtnSize = 26.0f;
constexpr float kTabH = 32.0f;

UiColor ToUiColor(Color color) noexcept {
  return UiColor{color.r, color.g, color.b, color.a};
}

// Applies the panel alpha to a color (Fade with alpha = m_alpha).
UiColor Faded(UiColor color, float alpha) noexcept {
  const auto a = static_cast<std::uint32_t>(
      static_cast<float>(color.a) * std::clamp(alpha, 0.0f, 1.0f));
  return UiColor{color.r, color.g, color.b, static_cast<std::uint8_t>(a)};
}

Rectangle ToRaylibRect(const UiRect& rect) noexcept {
  return Rectangle{rect.origin.x, rect.origin.y, rect.size.x, rect.size.y};
}

// Row value formatters (allocation-free, stack buffers; paint never formats
// into per-frame heap strings). Ported from the legacy UICharacter helpers.
void FormatPlainStat(char (&buffer)[64], float value, int precision) {
  switch (precision) {
  case 0:
    utils::FormatToBuffer(buffer, "{:.0f}", value);
    break;
  case 1:
    utils::FormatToBuffer(buffer, "{:.1f}", value);
    break;
  default:
    utils::FormatToBuffer(buffer, "{:.2f}", value);
    break;
  }
}

// Over-cap display: "cap (actual)" when effective < raw, single value else.
void FormatCappedStat(char (&buffer)[64], float effective, float raw,
                      bool isPercent, int precision) {
  if (effective < raw) {
    if (isPercent) {
      if (precision == 1) {
        utils::FormatToBuffer(buffer, "{:.1f}% ({:.1f}%)", effective * 100.0f,
                              raw * 100.0f);
      } else {
        utils::FormatToBuffer(buffer, "{:.0f}% ({:.0f}%)", effective * 100.0f,
                              raw * 100.0f);
      }
    } else {
      utils::FormatToBuffer(buffer, "{:.2f} ({:.2f})", effective, raw);
    }
  } else {
    if (isPercent) {
      if (precision == 1) {
        utils::FormatToBuffer(buffer, "{:.1f}%", effective * 100.0f);
      } else {
        utils::FormatToBuffer(buffer, "{:.0f}%", effective * 100.0f);
      }
    } else {
      utils::FormatToBuffer(buffer, "{:.2f}", effective);
    }
  }
}

constexpr UiColor kWhiteTint{255, 255, 255, 255};

} // namespace

UICharacterController::UICharacterController(UiRuntime& runtime,
                                             GameUiHost* uiHost)
    : m_runtime(runtime), m_uiHost(uiHost) {
  UiNodeDesc desc;
  desc.id = static_cast<UiId>(entt::hashed_string("ui_character_panel").value());
  desc.parent = kRootUiId;
  // Full-viewport anchor. The node is a declarative root for host-driven
  // layout; it always spans the whole viewport (the panel draws at fixed
  // logical coordinates, legacy UICharacter behaviour).
  desc.layout.kind = UiLayoutKind::Overlay;
  desc.layout.width = UiLength::Fraction(1.0f);
  desc.layout.height = UiLength::Fraction(1.0f);
  desc.layout.horizontalAlignment = UiAlignment::Start;
  desc.layout.verticalAlignment = UiAlignment::Start;
  desc.visible = true;
  // The panel must never intercept the gameplay mouse.
  desc.hitTestVisible = false;
  desc.capturePointer = false;
  desc.focusable = false;
  desc.captureKeyboard = false;
  desc.acceptsText = false;
  desc.modal = false;
  desc.zIndex = static_cast<std::int32_t>(UiDrawLayer::Panels);
  desc.customPainter = kInvalidUiResourceId;

  if (m_runtime.CreateNode(desc)) {
    m_rootNodeId = desc.id;
  } else {
    m_rootNodeId = kInvalidUiId;
  }
}

void UICharacterController::EnterGameplay() {
  m_inGameplay = true;
  ResetSessionState();
  // Session starts with the panel closed (opened via host KEY_C).
  SetVisible(false);
  SetAlpha(0.0f);
}

void UICharacterController::LeaveGameplay() {
  m_inGameplay = false;
  ResetSessionState();
  SetVisible(false);
  SetAlpha(0.0f);
}

void UICharacterController::SetVisible(bool visible) {
  m_visible = visible;
  if (m_rootNodeId != kInvalidUiId) {
    (void)m_runtime.SetNodeVisible(m_rootNodeId, visible);
  }
}

void UICharacterController::SetAlpha(float alpha) { m_alpha = alpha; }

void UICharacterController::Update(float dt) {
  // m_visible is the authoritative flag (host KEY_C/ESC route through
  // SetVisible); only the instance alpha animation remains here. alphaSpeed
  // matches the legacy UISystem::Update block (6.0f).
  constexpr float kAlphaSpeed = 6.0f;
  if (m_visible) {
    m_alpha = std::min(1.0f, m_alpha + dt * kAlphaSpeed);
  } else {
    m_alpha = std::max(0.0f, m_alpha - dt * kAlphaSpeed);
  }
}

UICharacterController::CharacterLayout UICharacterController::ComputeLayout()
    const {
  CharacterLayout layout;
  layout.panelX = m_panelX;
  layout.panelY = m_panelY;
  layout.panelW = kPanelW;
  layout.panelH = kPanelH;
  layout.padding = kPadding;

  // Avatar block (panelY + 80 ... + 90 + 30).
  layout.avatarSize = kAvatarSize;
  layout.avatarX = layout.panelX + kPadding;
  layout.avatarY = layout.panelY + 80.0f;
  layout.infoX = layout.avatarX + kAvatarSize + 25.0f;

  // Primary stats header + draft rows.
  layout.primaryY = layout.panelY + 80.0f + kAvatarSize + 30.0f;
  layout.attrRowY = layout.primaryY + 35.0f;
  layout.attrBtnX = layout.panelX + kPadding + 260.0f;

  // Tabs (after the 4 draft rows + separator).
  layout.tabY = layout.attrRowY + 4.0f * kRowGap + 20.0f + 15.0f;
  layout.tabH = kTabH;
  layout.tabW = (kPanelW - kPadding * 2.0f) / 4.0f;

  // Content area.
  layout.contentY = layout.tabY + 40.0f;
  const int totalDraft = m_draftStrength + m_draftDexterity +
                         m_draftIntelligence + m_draftVitality;
  const float bottomReserve = (totalDraft > 0) ? 50.0f : 0.0f;
  layout.contentH = layout.panelY + layout.panelH - layout.contentY -
                    kPadding - bottomReserve;
  layout.contentRect =
      UiRect{{layout.panelX + kPadding, layout.contentY},
             {kPanelW - kPadding * 2.0f, layout.contentH}};

  // 确认加点 (only rendered when draft > 0).
  layout.confirmButtonRect =
      UiRect{{layout.panelX + kPanelW - kPadding - 130.0f,
              layout.panelY + kPanelH - 50.0f},
             {130.0f, 40.0f}};

  // Confirm popup (fixed 360x200 centered in the UI reference space).
  constexpr float kPopupW = 360.0f;
  constexpr float kPopupH = 200.0f;
  const float popupX = (UI_REF_WIDTH - kPopupW) / 2.0f;
  const float popupY = (UI_REF_HEIGHT - kPopupH) / 2.0f;
  layout.popupBox = UiRect{{popupX, popupY}, {kPopupW, kPopupH}};
  layout.popupYesRect = UiRect{{popupX + 40.0f, popupY + 120.0f}, {110.0f, 40.0f}};
  layout.popupNoRect =
      UiRect{{popupX + kPopupW - 110.0f - 40.0f, popupY + 120.0f},
             {110.0f, 40.0f}};
  return layout;
}

void UICharacterController::EnqueueAllocationIntent(int strength,
                                                    int dexterity,
                                                    int intelligence,
                                                    int vitality) {
  if (m_uiHost == nullptr) {
    return; // Headless (tests): nothing to enqueue into.
  }
  GameUiIntent intent;
  intent.sourceNode = m_rootNodeId;
  intent.kind = GameUiIntentKind::ConfirmAttributeAllocation;
  intent.payload.allocationStrength = strength;
  intent.payload.allocationDexterity = dexterity;
  intent.payload.allocationIntelligence = intelligence;
  intent.payload.allocationVitality = vitality;
  m_uiHost->EnqueueIntent(intent);
}

void UICharacterController::UpdateInput(const GameUiSnapshot& snapshot) {
  if (!m_visible || !m_inGameplay) {
    return; // Hidden/faded panels never hit-test (legacy Draw did, but only
            // the open panel should respond to interaction).
  }

  // Panel drag (instance drag state; same UIPanelDragService pattern).
  UIPanelDragInputs dragInputs;
  dragInputs.mousePosition = UISystem::GetMousePositionLogic();
  dragInputs.isMousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
  dragInputs.isMouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
  UIPanelDragBounds dragBounds;
  dragBounds.panelWidth = kPanelW;
  dragBounds.panelHeight = kPanelH;
  dragBounds.headerHeight = kHeaderHeight;
  dragBounds.uiRefWidth = UI_REF_WIDTH;
  dragBounds.uiRefHeight = UI_REF_HEIGHT;
  UIPanelDragService::UpdatePanelDrag(m_panelState, UIPanelID::Character,
                                      m_activeDragPanel, m_panelX, m_panelY,
                                      dragInputs, dragBounds);

  const CharacterLayout layout = ComputeLayout();
  const Vector2 mousePos = UISystem::GetMousePositionLogic();
  const bool mousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

  const int totalDraft = m_draftStrength + m_draftDexterity +
                         m_draftIntelligence + m_draftVitality;
  const int remainingPoints =
      snapshot.player.availableAttributePoints - totalDraft;

  if (m_showConfirmPopup) {
    // Focus surface: only the confirm/cancel buttons respond. Confirm commits
    // the drafts through the ConfirmAttributeAllocation intent (executed by
    // the command handler next Update); failures surface through the result
    // notification (message box). Cancel keeps the drafts for editing.
    if (mousePressed) {
      if (CheckCollisionPointRec(mousePos, ToRaylibRect(layout.popupYesRect))) {
        EnqueueAllocationIntent(m_draftStrength, m_draftDexterity,
                                m_draftIntelligence, m_draftVitality);
        ResetDraftPoints();
        m_showConfirmPopup = false;
      } else if (CheckCollisionPointRec(mousePos,
                                        ToRaylibRect(layout.popupNoRect))) {
        m_showConfirmPopup = false;
      }
    }
    return;
  }

  // Tabs (click switches the content tab and resets the scroll).
  if (mousePressed) {
    for (int i = 0; i < 4; ++i) {
      const UiRect tabRect{
          {layout.panelX + kPadding + i * layout.tabW, layout.tabY},
          {layout.tabW - 2.0f, layout.tabH}};
      if (CheckCollisionPointRec(mousePos, ToRaylibRect(tabRect))) {
        m_activeCharTab = i;
        m_charPanelScroll = 0.0f;
      }
    }
  }

  // Draft attribute +/- buttons.
  auto HandleAttrRow = [&](int* draft, float rowY) {
    const UiRect minusRect{{layout.attrBtnX, rowY}, {kBtnSize, kBtnSize}};
    const UiRect plusRect{{layout.attrBtnX + 35.0f, rowY},
                          {kBtnSize, kBtnSize}};
    if (!mousePressed) {
      return;
    }
    if (*draft > 0 && CheckCollisionPointRec(mousePos, ToRaylibRect(minusRect))) {
      --(*draft);
    } else if (remainingPoints > 0 &&
               CheckCollisionPointRec(mousePos, ToRaylibRect(plusRect))) {
      ++(*draft);
    }
  };
  HandleAttrRow(&m_draftStrength, layout.attrRowY);
  HandleAttrRow(&m_draftDexterity, layout.attrRowY + kRowGap);
  HandleAttrRow(&m_draftIntelligence, layout.attrRowY + 2.0f * kRowGap);
  HandleAttrRow(&m_draftVitality, layout.attrRowY + 3.0f * kRowGap);

  // Content scroll (mouse wheel over the content area; clamped like legacy).
  if (CheckCollisionPointRec(mousePos, ToRaylibRect(layout.contentRect))) {
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
      m_charPanelScroll += wheel * 30.0f;
    }
  }
  if (m_charPanelScroll > 0.0f) {
    m_charPanelScroll = 0.0f;
  }
  if (m_lastContentHeight > layout.contentRect.size.y) {
    const float minScroll = layout.contentRect.size.y - m_lastContentHeight;
    if (m_charPanelScroll < minScroll) {
      m_charPanelScroll = minScroll;
    }
  } else {
    m_charPanelScroll = 0.0f;
  }

  // 确认加点 (only when draft points are pending).
  if (totalDraft > 0 && mousePressed &&
      CheckCollisionPointRec(mousePos,
                             ToRaylibRect(layout.confirmButtonRect))) {
    ShowConfirmPopup();
  }
}

void UICharacterController::Paint(UiDrawList& drawList,
                                  const UiViewport& viewport,
                                  const GameUiSnapshot& snapshot) const {
  if (m_alpha <= 0.001f || !m_inGameplay) {
    return; // Fully transparent / out of session: emit nothing.
  }
  if (!snapshot.player.hasPlayer) {
    return; // No player data to paint (legacy Draw early-out parity).
  }
  const CharacterLayout layout = ComputeLayout();
  const UiId node = m_rootNodeId;
  const auto& theme = UIRenderer::GetTheme();
  constexpr UiDrawLayer layer = UiDrawLayer::Panels;
  const float alpha = m_alpha;

  auto Fade = [alpha](UiColor color) { return Faded(color, alpha); };
  auto Theme = [&theme](const Color& color) { return ToUiColor(color); };

  // --- 1. Panel background (fixed logical coords; UI_REF == runtime space) --
  drawList.FillRect(
      layer, node,
      UiRect{{layout.panelX, layout.panelY}, {kPanelW, kPanelH}},
      Fade(Theme(theme.panelBackground)));
  drawList.StrokeRect(
      layer, node, UiRect{{layout.panelX, layout.panelY}, {kPanelW, kPanelH}},
      Fade(Theme(theme.panelBorder)), 1.0f);
// Header line.
drawList.Line(layer, node, UiVec2{layout.panelX, layout.panelY + kHeaderHeight},
              UiVec2{layout.panelX + kPanelW, layout.panelY + kHeaderHeight},
              Fade(Theme(theme.panelBorder)), 1.0f);

  // Title + close hint.
  drawList.Text(layer, node, "角色属性",
                {layout.panelX + kPadding, layout.panelY + 18.0f}, 28.0f,
                Fade(Theme(theme.textHighlight)), kGlobalFontResourceId);
  drawList.Text(layer, node, "按 'C' 关闭",
                {layout.panelX + kPanelW - 120.0f, layout.panelY + 25.0f},
                18.0f, Fade(Theme(theme.textSecondary)), kGlobalFontResourceId);

  // --- 2. Character overview (avatar & level) ---
  const GameUiPlayerSnapshot& player = snapshot.player;
  drawList.FillRect(
      layer, node,
      UiRect{{layout.avatarX, layout.avatarY},
             {layout.avatarSize, layout.avatarSize}},
      Fade(Theme(theme.slotBackground)));
  drawList.StrokeRect(
      layer, node,
      UiRect{{layout.avatarX, layout.avatarY},
             {layout.avatarSize, layout.avatarSize}},
      Fade(Theme(theme.panelBorder)), 1.0f);
  if (player.avatarTextureId != 0) {
    // The host syncs the current SpriteComponent texture under the fixed
    // avatar resource id (raw GL ids never cross the draw-list boundary).
    drawList.Image(layer, node,
                   UiRect{{layout.avatarX, layout.avatarY},
                          {layout.avatarSize, layout.avatarSize}},
                   kPlayerAvatarTextureResourceId, kWhiteTint);
  } else {
    drawList.Text(layer, node, "?", {layout.avatarX + 35.0f, layout.avatarY + 25.0f},
                  40.0f, Fade(Theme(theme.textSecondary)), kGlobalFontResourceId);
  }

  if (player.hasPlayer) {
    const bool isMaxLevel = (player.level >= 100);
    char buffer[64];
    utils::FormatToBuffer(buffer, "等级 {}", player.level);
    drawList.Text(layer, node, buffer,
                  {layout.infoX, layout.avatarY + 10.0f}, 26.0f,
                  Fade(Theme(isMaxLevel ? theme.textHighlight : theme.textPrimary)),
                  kGlobalFontResourceId);

    // XP bar background + fill.
    drawList.FillRect(layer, node,
                      UiRect{{layout.infoX, layout.avatarY + 50.0f}, {200.0f, 12.0f}},
                      Fade(Theme(theme.slotBackground)));
    float xpRatio = 0.0f;
    if (isMaxLevel) {
      xpRatio = 1.0f;
    } else if (player.requiredXp > 0.0f) {
      xpRatio = player.currentXp / player.requiredXp;
    }
    if (xpRatio > 1.0f) {
      xpRatio = 1.0f;
    }
    drawList.FillRect(
        layer, node,
        UiRect{{layout.infoX, layout.avatarY + 50.0f}, {200.0f * xpRatio, 12.0f}},
        Fade(Theme(isMaxLevel ? theme.textHighlight : theme.success)));
    if (isMaxLevel) {
      drawList.Text(layer, node, "MAX LEVEL",
                    {layout.infoX, layout.avatarY + 65.0f}, 16.0f,
                    Fade(Theme(theme.textHighlight)), kGlobalFontResourceId);
    } else {
      utils::FormatToBuffer(buffer, "XP: {:.0f} / {:.0f}", player.currentXp,
                            player.requiredXp);
      drawList.Text(layer, node, buffer,
                    {layout.infoX, layout.avatarY + 65.0f}, 16.0f,
                    Fade(Theme(theme.textSecondary)), kGlobalFontResourceId);
    }
  } else {
    drawList.Text(layer, node, "等级 ??",
                  {layout.infoX, layout.avatarY + 10.0f}, 26.0f,
                  Fade(Theme(theme.textSecondary)), kGlobalFontResourceId);
  }

  // No player data: the legacy Draw returned here (frame + avatar only).
  if (!player.hasPlayer) {
    return;
  }

  // --- 3. Primary stats (draft rows) ---
  drawList.Text(layer, node, "基础属性",
                {layout.panelX + kPadding, layout.primaryY}, 22.0f,
                Fade(Theme(theme.textHighlight)), kGlobalFontResourceId);

  const GameUiCharacterStatsView& cs = snapshot.characterStats;
  const int totalDraft = m_draftStrength + m_draftDexterity +
                         m_draftIntelligence + m_draftVitality;
  const int remainingPoints = player.availableAttributePoints - totalDraft;

  char buffer[64];
  utils::FormatToBuffer(buffer, "可用点数: {}", remainingPoints);
  drawList.Text(
      layer, node, buffer,
      {layout.panelX + kPanelW - kPadding - 140.0f, layout.primaryY + 4.0f},
      18.0f, Fade(Theme(remainingPoints > 0 ? theme.success : theme.textSecondary)),
      kGlobalFontResourceId);

  struct DraftRow {
    const char* label;
    float base;
    float effective;
    int draft;
  };
  const DraftRow rows[4] = {
      {"力量", cs.strength, cs.effectiveStrength, m_draftStrength},
      {"敏捷", cs.dexterity, cs.effectiveDexterity, m_draftDexterity},
      {"智力", cs.intelligence, cs.effectiveIntelligence, m_draftIntelligence},
      {"体能", cs.vitality, cs.effectiveVitality, m_draftVitality},
  };

  for (int i = 0; i < 4; ++i) {
    const float rowY = layout.attrRowY + i * kRowGap;
    drawList.Text(layer, node, rows[i].label,
                  {layout.panelX + kPadding, rowY + 4.0f}, 20.0f,
                  Fade(Theme(theme.textSecondary)), kGlobalFontResourceId);

    const float displayVal = rows[i].effective + static_cast<float>(rows[i].draft);
    const bool highlighted = rows[i].draft > 0 || rows[i].effective > rows[i].base;
    if (rows[i].draft > 0) {
      utils::FormatToBuffer(buffer, "{:.0f} (+{})", displayVal, rows[i].draft);
    } else {
      utils::FormatToBuffer(buffer, "{:.0f}", displayVal);
    }
    drawList.Text(layer, node, buffer,
                  {layout.panelX + kPadding + 100.0f, rowY + 4.0f}, 20.0f,
                  Fade(Theme(highlighted ? theme.success : theme.textPrimary)),
                  kGlobalFontResourceId);

    // +/- buttons (drawn only when actionable; - when draft > 0, + when
    // points remain — mirrors the legacy enable logic).
    if (rows[i].draft > 0) {
      drawList.Image(layer, node,
                     UiRect{{layout.attrBtnX, rowY}, {kBtnSize, kBtnSize}},
                     kPanelSquareTextureResourceId, kWhiteTint);
      drawList.Text(layer, node, "-", {layout.attrBtnX + 7.0f, rowY + 1.0f},
                    20.0f, Fade(Theme(theme.textPrimary)), kGlobalFontResourceId);
    }
    if (remainingPoints > 0) {
      drawList.Image(layer, node,
                     UiRect{{layout.attrBtnX + 35.0f, rowY}, {kBtnSize, kBtnSize}},
                     kPanelSquareTextureResourceId, kWhiteTint);
      drawList.Text(layer, node, "+",
                    {layout.attrBtnX + 42.0f, rowY + 1.0f}, 20.0f,
                    Fade(Theme(theme.textPrimary)), kGlobalFontResourceId);
    }
  }

// Separator line.
const float separatorY = layout.attrRowY + 4.0f * kRowGap + 20.0f;
drawList.Line(layer, node, UiVec2{layout.panelX + kPadding, separatorY},
              UiVec2{layout.panelX + kPanelW - kPadding * 2.0f, separatorY},
              Fade(Theme(theme.panelBorder)), 1.0f);

  // --- 4. Tabs ---
  const char* tabNames[4] = {"攻击", "防御", "召唤", "其他"};
  for (int i = 0; i < 4; ++i) {
    const UiRect tabRect{{layout.panelX + kPadding + i * layout.tabW, layout.tabY},
                         {layout.tabW - 2.0f, layout.tabH}};
    const bool isSelected = (m_activeCharTab == i);
    drawList.Image(layer, node, tabRect, kPanelRectTextureResourceId,
                   isSelected ? Theme(theme.textHighlight) : kWhiteTint);
    drawList.Text(
        layer, node, tabNames[i],
        {tabRect.origin.x + tabRect.size.x * 0.5f - 18.0f, tabRect.origin.y + 5.0f},
        18.0f, Fade(Theme(isSelected ? Color{0, 0, 0, 255} : theme.textSecondary)),
        kGlobalFontResourceId, UiTextAlign::Left);
  }

  // --- 5. Content (scissored tab rows) ---
  drawList.PushClip(layout.contentRect);
  const float startY = layout.contentY + m_charPanelScroll;
  float y = startY;
  const float rowX = layout.panelX + kPadding + 5.0f;
  const float rowW = layout.contentRect.size.x - 20.0f;

  auto DrawStatRow = [&](const char* label, const char* value, float fontSize) {
    drawList.Text(layer, node, label, {rowX, y}, fontSize,
                  Fade(Theme(theme.textSecondary)), kGlobalFontResourceId);
    drawList.Text(layer, node, value, {rowX + rowW, y}, fontSize,
                  Fade(Theme(theme.textPrimary)), kGlobalFontResourceId,
                  UiTextAlign::Right);
    y += fontSize + 8.0f;
  };
  auto DrawSectionHeader = [&](const char* text) {
    drawList.Text(layer, node, text, {rowX, y}, 20.0f,
                  Fade(Theme(theme.textHighlight)), kGlobalFontResourceId);
    y += 30.0f;
  };

  char value[64];
  if (m_activeCharTab == 0) {
    DrawSectionHeader("攻击基础");
    const float flatPhys = cs.flatDamage[static_cast<int>(DamageType::Physical)];
    const float physMult =
        cs.damageMultipliers[static_cast<int>(DamageType::Physical)];
    utils::FormatToBuffer(value, "{:.0f}",
                          (cs.minWeaponDamage + flatPhys) * physMult);
    DrawStatRow("物理伤害", value, 20.0f);

    if (cs.attackSpeed > 1.0f) {
      const float increase = (cs.attackSpeed - 1.0f) * 100.0f;
      utils::FormatToBuffer(value, "{:.2f} (+{:.0f}%)", cs.attackSpeed, increase);
      DrawStatRow("攻击速度", value, 20.0f);
    } else {
      FormatCappedStat(value, cs.attackSpeed, cs.rawAttackSpeed, false, 2);
      DrawStatRow("攻击速度", value, 20.0f);
    }

    utils::FormatToBuffer(value, "{:.0f}%", cs.accuracy * 100.0f);
    DrawStatRow("命中率", value, 20.0f);
    utils::FormatToBuffer(value, "{:.1f}%", cs.critChance * 100.0f);
    DrawStatRow("暴击几率", value, 20.0f);
    utils::FormatToBuffer(value, "{:.0f}%", cs.critDamage * 100.0f);
    DrawStatRow("暴击伤害", value, 20.0f);
    utils::FormatToBuffer(value, "{:.0f}", cs.armorPen);
    DrawStatRow("护甲穿透", value, 20.0f);
    utils::FormatToBuffer(value, "{:.2f}", cs.castSpeed);
    DrawStatRow("施法速度", value, 20.0f);

    y += 15.0f;
    DrawSectionHeader("伤害构成 (点伤 / 增益)");
    auto DrawDamageRow = [&](DamageType type, const char* name) {
      float flat = cs.flatDamage[static_cast<int>(type)];
      if (type == DamageType::Physical) {
        flat += (cs.minWeaponDamage + cs.maxWeaponDamage) * 0.5f;
      }
      const float mult =
          (cs.damageMultipliers[static_cast<int>(type)] - 1.0f) * 100.0f;
      utils::FormatToBuffer(value, "{:.0f} / +{:.0f}%", flat, mult);
      DrawStatRow(name, value, 20.0f);
    };
    DrawDamageRow(DamageType::Physical, "物理伤害");
    DrawDamageRow(DamageType::Fire, "火焰伤害");
    DrawDamageRow(DamageType::Cold, "冰霜伤害");
    DrawDamageRow(DamageType::Lightning, "闪电伤害");
    DrawDamageRow(DamageType::Poison, "毒素伤害");
    DrawDamageRow(DamageType::Shadow, "暗影伤害");

  } else if (m_activeCharTab == 1) {
    DrawSectionHeader("防御基础");
    utils::FormatToBuffer(value, "{:.0f} / {:.0f}", cs.health, cs.maxHealth);
    DrawStatRow("生命值", value, 20.0f);
    utils::FormatToBuffer(value, "{:.0f} / {:.0f}", cs.mana, cs.maxMana);
    DrawStatRow("法力值", value, 20.0f);
    utils::FormatToBuffer(value, "{:.0f}", cs.armor);
    DrawStatRow("护甲", value, 20.0f);
    utils::FormatToBuffer(value, "{:.1f}%", cs.armorDr * 100.0f);
    DrawStatRow("物理减伤", value, 20.0f);
    utils::FormatToBuffer(value, "{:.0f}", cs.dodgeRating);
    DrawStatRow("闪避评级", value, 20.0f);
    utils::FormatToBuffer(
        value, "{:.1f}% (Max {:.0f}%)", cs.dodgeChance * 100.0f,
        NoMoreDay::Constants::Combat::Scaling::DODGE_MAX_CHANCE * 100.0f);
    DrawStatRow("闪避几率", value, 20.0f);
    FormatCappedStat(value, cs.blockChance, cs.rawBlockChance, true, 1);
    DrawStatRow("格挡几率", value, 20.0f);
    utils::FormatToBuffer(value, "{:.0f}", cs.blockRating);
    DrawStatRow("格挡评级", value, 20.0f);
    utils::FormatToBuffer(value, "{:.1f}%", cs.blockEffect * 100.0f);
    DrawStatRow("格挡减伤", value, 20.0f);
    utils::FormatToBuffer(value, "{:.1f}%", cs.damageReduction * 100.0f);
    DrawStatRow("全局减伤", value, 20.0f);

    y += 15.0f;
    DrawSectionHeader("元素抗性");
    auto DrawRes = [&](DamageType type, const char* name) {
      FormatCappedStat(value,
                       cs.resistances[static_cast<int>(type)],
                       cs.rawResistances[static_cast<int>(type)], true, 0);
      DrawStatRow(name, value, 20.0f);
    };
    DrawRes(DamageType::Physical, "物理抗性");
    DrawRes(DamageType::Fire, "火焰抗性");
    DrawRes(DamageType::Cold, "冰霜抗性");
    DrawRes(DamageType::Lightning, "闪电抗性");
    DrawRes(DamageType::Poison, "毒素抗性");
    DrawRes(DamageType::Shadow, "暗影抗性");

  } else if (m_activeCharTab == 3) {
    DrawSectionHeader("回复与辅助");
    utils::FormatToBuffer(value, "{:.1f} /秒", cs.healthRegen);
    DrawStatRow("生命回复", value, 20.0f);
    utils::FormatToBuffer(value, "{:.1f} /秒", cs.manaRegen);
    DrawStatRow("法力回复", value, 20.0f);
    utils::FormatToBuffer(value, "{:.1f}%", cs.lifeSteal * 100.0f);
    DrawStatRow("生命吸取", value, 20.0f);
    utils::FormatToBuffer(value, "{:.0f}", cs.lifeOnHit);
    DrawStatRow("击回生命", value, 20.0f);

    y += 15.0f;
    DrawSectionHeader("综合属性");
    FormatCappedStat(value, cs.moveSpeed, cs.rawMoveSpeed, false, 2);
    DrawStatRow("移动速度", value, 20.0f);
    utils::FormatToBuffer(value, "+{:.0f}%", cs.magicFind * 100.0f);
    DrawStatRow("魔法寻宝", value, 20.0f);
    FormatCappedStat(value, cs.cooldownReduction, cs.rawCooldownReduction, true,
                     0);
    DrawStatRow("冷却缩减", value, 20.0f);
    utils::FormatToBuffer(value, "+{:.0f}%", (cs.durationScale - 1.0f) * 100.0f);
    DrawStatRow("技能持续时间", value, 20.0f);
    utils::FormatToBuffer(value, "+{:.0f}%", (cs.areaScale - 1.0f) * 100.0f);
    DrawStatRow("技能范围", value, 20.0f);
    utils::FormatToBuffer(value, "{:.0f}", cs.pickupRange);
    DrawStatRow("拾取范围", value, 20.0f);
    utils::FormatToBuffer(value, "+{:.0f}%", cs.goldBonus * 100.0f);
    DrawStatRow("金币加成", value, 20.0f);
    utils::FormatToBuffer(value, "+{:.0f}%", cs.experienceGainMult * 100.0f);
    DrawStatRow("经验加成", value, 20.0f);
    utils::FormatToBuffer(value, "{:.0f}", cs.thorns);
    DrawStatRow("荆棘伤害", value, 20.0f);
  }
  // Tab 2 (召唤) renders nothing (legacy behaviour).

  m_lastContentHeight = y - startY;
  drawList.PopClip();

  // --- 确认加点 (only when draft points are pending) ---
  if (totalDraft > 0) {
    drawList.Image(layer, node, layout.confirmButtonRect,
                   kPanelRectTextureResourceId, kWhiteTint);
    drawList.StrokeRect(layer, node, layout.confirmButtonRect,
                        Fade(Theme(theme.panelBorderHighlight)), 1.0f);
    drawList.Text(
        layer, node, "确认加点",
        {layout.confirmButtonRect.origin.x +
             layout.confirmButtonRect.size.x * 0.5f - 40.0f,
         layout.confirmButtonRect.origin.y + 9.0f},
        20.0f, Fade(Theme(theme.textPrimary)), kGlobalFontResourceId);
  }

  // --- 6. Confirmation popup (Modal layer, above the panel) ---
  if (m_showConfirmPopup) {
    constexpr UiDrawLayer modalLayer = UiDrawLayer::Modal;
    // Full-screen translucent overlay.
    drawList.FillRect(modalLayer, node,
                      UiRect{{0.0f, 0.0f}, viewport.LogicalSize()},
                      UiColor{0, 0, 0, 180});

    drawList.FillRect(modalLayer, node, layout.popupBox,
                      Fade(Theme(theme.panelBackground)));
    drawList.StrokeRect(modalLayer, node, layout.popupBox,
                        Fade(Theme(theme.textHighlight)), 2.0f);
    drawList.Text(modalLayer, node, "确认分配属性点吗?",
                  {layout.popupBox.origin.x + 55.0f,
                   layout.popupBox.origin.y + 45.0f},
                  24.0f, Fade(Theme(theme.textPrimary)), kGlobalFontResourceId);

    // Confirm button.
    drawList.Image(modalLayer, node, layout.popupYesRect,
                   kPanelRectTextureResourceId, kWhiteTint);
    drawList.Text(modalLayer, node, "确认",
                  {layout.popupYesRect.origin.x + 32.0f,
                   layout.popupYesRect.origin.y + 9.0f},
                  20.0f, Fade(Theme(theme.textPrimary)), kGlobalFontResourceId);
    // Cancel button.
    drawList.Image(modalLayer, node, layout.popupNoRect,
                   kPanelRectTextureResourceId, kWhiteTint);
    drawList.Text(modalLayer, node, "取消",
                  {layout.popupNoRect.origin.x + 32.0f,
                   layout.popupNoRect.origin.y + 9.0f},
                  20.0f, Fade(Theme(theme.textPrimary)), kGlobalFontResourceId);
  }
}

bool UICharacterController::IsConfirmPopupVisible() const noexcept {
  return m_showConfirmPopup;
}

void UICharacterController::CloseConfirmPopup() {
  // Escape cancel: dismiss the confirm dialog; the draft points stay so the
  // user can resume editing (legacy ESC behaviour).
  m_showConfirmPopup = false;
}

void UICharacterController::ShowConfirmPopup() {
  // Shared by the UpdateInput confirm-click path and the host Escape test
  // seam. No-op while the panel is hidden (a hidden panel must not hold an
  // open focus surface). The gameplay gate is implicit: UpdateInput only runs
  // while the panel is visible + in gameplay, so the visible check here is
  // sufficient (and keeps the host Escape chain testable headless, where the
  // host is never "in gameplay").
  if (m_visible) {
    m_showConfirmPopup = true;
  }
}

void UICharacterController::ResetDraftPoints() {
  m_draftStrength = 0;
  m_draftDexterity = 0;
  m_draftIntelligence = 0;
  m_draftVitality = 0;
}

bool UICharacterController::IsVisible() const noexcept { return m_visible; }

float UICharacterController::Alpha() const noexcept { return m_alpha; }

bool UICharacterController::IsInGameplay() const noexcept {
  return m_inGameplay;
}

UiId UICharacterController::NodeId() const noexcept { return m_rootNodeId; }

void UICharacterController::ResetSessionState() {
  m_activeCharTab = 0;
  m_charPanelScroll = 0.0f;
  m_lastContentHeight = 0.0f;
  ResetDraftPoints();
  m_showConfirmPopup = false;
}

} // namespace NoMoreDay::ui
