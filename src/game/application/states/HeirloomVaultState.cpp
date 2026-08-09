// src/game/application/states/HeirloomVaultState.cpp
// 传家宝宝库界面实现
#include "game/application/states/HeirloomVaultState.hpp"
#include "core/logging/Logger.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/application/scene/StateManager.hpp"
#include "game/application/ui/UISystem.hpp"
#include <cmath>
#include <raylib.h>

namespace NoMoreDay {

void HeirloomVaultState::OnEnter() {
  LOG_INFO("[HeirloomVaultState] Entering Heirloom Vault...");

  // 加载宝库数据
  HeirloomVault::Get().load();

  // 重置 UI 状态
  m_hoveredIndex = -1;
  m_selectedIndex = -1;
  m_confirmedSelection = std::nullopt;
  m_scrollOffset = 0.0f;
  m_fadeAlpha = 0.0f;
}

void HeirloomVaultState::OnExit() {
  LOG_INFO("[HeirloomVaultState] Exiting Heirloom Vault.");
}

bool HeirloomVaultState::OnUpdate(float dt) {
  // 淡入动画
  m_fadeAlpha = std::min(m_fadeAlpha + dt * 4.0f, 1.0f);

  handleInput();

  return true; // 允许底层状态继续更新
}

void HeirloomVaultState::handleInput() {
  // ESC 关闭宝库
  if (IsKeyPressed(KEY_ESCAPE)) {
    m_stateManager->PopState();
    return;
  }

  // 滚轮滚动
  float wheel = GetMouseWheelMove();
  if (std::abs(wheel) > 0.01f) {
    m_scrollOffset -= wheel * 40.0f;

    // 限制滚动范围
    const auto &vault = HeirloomVault::Get();
    float maxScroll = std::max(
        0.0f, static_cast<float>(vault.size()) * (kSlotHeight + kSlotPadding) -
                  (static_cast<float>(GetScreenHeight()) - kPanelMargin * 2 -
                   100.0f));

    m_scrollOffset = std::clamp(m_scrollOffset, 0.0f, maxScroll);
  }

  // 键盘导航
  if (IsKeyPressed(KEY_UP) && m_selectedIndex > 0) {
    m_selectedIndex--;
  }
  if (IsKeyPressed(KEY_DOWN)) {
    const auto &vault = HeirloomVault::Get();
    if (m_selectedIndex < static_cast<int>(vault.size()) - 1) {
      m_selectedIndex++;
    }
  }

  // 确认选择
  if (IsKeyPressed(KEY_ENTER) && m_selectedIndex >= 0) {
    m_confirmedSelection = static_cast<size_t>(m_selectedIndex);
    LOG_INFO("[HeirloomVaultState] Confirmed selection: index {}",
             m_selectedIndex);
    m_stateManager->PopState();
    return;
  }

  // 鼠标点击选择
  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    if (m_hoveredIndex >= 0) {
      if (m_selectedIndex == m_hoveredIndex) {
        // 双击确认
        m_confirmedSelection = static_cast<size_t>(m_selectedIndex);
        LOG_INFO("[HeirloomVaultState] Double-click confirmed: index {}",
                 m_selectedIndex);
        m_stateManager->PopState();
        return;
      } else {
        m_selectedIndex = m_hoveredIndex;
      }
    }
  }
}

void HeirloomVaultState::OnRender() {
  const auto &vault = HeirloomVault::Get();
  const float screenW = static_cast<float>(GetScreenWidth());
  const float screenH = static_cast<float>(GetScreenHeight());

  // 半透明背景遮罩
  DrawRectangle(0, 0, static_cast<int>(screenW), static_cast<int>(screenH),
                ColorAlpha(BLACK, 0.7f * m_fadeAlpha));

  // 面板位置
  const float panelX = (screenW - kPanelWidth) / 2.0f;
  const float panelY = kPanelMargin;
  const float panelH = screenH - kPanelMargin * 2.0f;

  // 面板背景
  DrawRectangleRounded(
      {panelX - 10, panelY - 10, kPanelWidth + 20, panelH + 20}, 0.02f, 8,
      ColorAlpha(Color{30, 30, 40, 255}, m_fadeAlpha));

  // 边框
  DrawRectangleRoundedLines(
      {panelX - 10, panelY - 10, kPanelWidth + 20, panelH + 20}, 0.02f, 8,
      ColorAlpha(Color{180, 150, 100, 255}, m_fadeAlpha));

  // 标题
  const char *title = "传家宝宝库";
  const int titleSize = 28;
  const int titleWidth = MeasureText(title, titleSize);
  DrawText(title, static_cast<int>(panelX + (kPanelWidth - titleWidth) / 2.0f),
           static_cast<int>(panelY + 10), titleSize,
           ColorAlpha(GOLD, m_fadeAlpha));

  // 说明文字
  const char *subtitle = "选择一件传家宝带入新游戏 (ESC 取消)";
  DrawText(subtitle, static_cast<int>(panelX + 20),
           static_cast<int>(panelY + 50), 16,
           ColorAlpha(LIGHTGRAY, m_fadeAlpha * 0.8f));

  // 内容区域 (启用裁剪)
  const float contentY = panelY + 80.0f;
  const float contentH = panelH - 130.0f;

  BeginScissorMode(static_cast<int>(panelX), static_cast<int>(contentY),
                   static_cast<int>(kPanelWidth), static_cast<int>(contentH));

  // 重置悬停状态
  m_hoveredIndex = -1;

  // 渲染传家宝槽位
  float currentY = contentY - m_scrollOffset;
  for (size_t i = 0; i < vault.size(); ++i) {
    const auto *data = vault.getHeirloom(i);
    if (data) {
      // 跳过不可见的槽位
      if (currentY + kSlotHeight < contentY || currentY > contentY + contentH) {
        currentY += kSlotHeight + kSlotPadding;
        continue;
      }

      renderSlot(*data, static_cast<int>(i),
                 panelX + (kPanelWidth - kSlotWidth) / 2.0f, currentY);
      currentY += kSlotHeight + kSlotPadding;
    }
  }

  // 空宝库提示
  if (vault.size() == 0) {
    const char *emptyMsg = "宝库是空的";
    const int emptyMsgW = MeasureText(emptyMsg, 20);
    DrawText(emptyMsg,
             static_cast<int>(panelX + (kPanelWidth - emptyMsgW) / 2.0f),
             static_cast<int>(contentY + contentH / 2.0f - 10), 20,
             ColorAlpha(GRAY, m_fadeAlpha));
  }

  EndScissorMode();

  // 底部按钮
  const float btnY = panelY + panelH - 50.0f;
  const float btnW = 120.0f;
  const float btnH = 35.0f;

  // 确认按钮
  Rectangle confirmBtn = {panelX + kPanelWidth / 2.0f - btnW - 10, btnY, btnW,
                          btnH};
  bool confirmHovered = CheckCollisionPointRec(GetMousePosition(), confirmBtn);
  DrawRectangleRounded(confirmBtn, 0.2f, 4,
                       ColorAlpha(confirmHovered ? Color{60, 80, 60, 255}
                                                 : Color{40, 60, 40, 255},
                                  m_fadeAlpha));
  DrawRectangleRoundedLines(
      confirmBtn, 0.2f, 4,
      ColorAlpha(m_selectedIndex >= 0 ? GREEN : GRAY, m_fadeAlpha));

  const char *confirmText = "确认";
  DrawText(confirmText,
           static_cast<int>(confirmBtn.x +
                            (btnW - MeasureText(confirmText, 16)) / 2.0f),
           static_cast<int>(confirmBtn.y + 10), 16,
           ColorAlpha(m_selectedIndex >= 0 ? WHITE : GRAY, m_fadeAlpha));

  if (confirmHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
      m_selectedIndex >= 0) {
    m_confirmedSelection = static_cast<size_t>(m_selectedIndex);
    m_stateManager->PopState();
    return;
  }

  // 取消按钮
  Rectangle cancelBtn = {panelX + kPanelWidth / 2.0f + 10, btnY, btnW, btnH};
  bool cancelHovered = CheckCollisionPointRec(GetMousePosition(), cancelBtn);
  DrawRectangleRounded(cancelBtn, 0.2f, 4,
                       ColorAlpha(cancelHovered ? Color{80, 50, 50, 255}
                                                : Color{60, 40, 40, 255},
                                  m_fadeAlpha));
  DrawRectangleRoundedLines(cancelBtn, 0.2f, 4, ColorAlpha(RED, m_fadeAlpha));

  const char *cancelText = "取消";
  DrawText(cancelText,
           static_cast<int>(cancelBtn.x +
                            (btnW - MeasureText(cancelText, 16)) / 2.0f),
           static_cast<int>(cancelBtn.y + 10), 16,
           ColorAlpha(WHITE, m_fadeAlpha));

  if (cancelHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    m_stateManager->PopState();
    return;
  }

  // 悬停槽位的详细提示
  if (m_hoveredIndex >= 0) {
    const auto *hoveredData =
        vault.getHeirloom(static_cast<size_t>(m_hoveredIndex));
    if (hoveredData) {
      renderTooltip(*hoveredData);
    }
  }
}

void HeirloomVaultState::renderSlot(const HeirloomData &data, int index,
                                    float x, float y) {
  Rectangle slotRect = {x, y, kSlotWidth, kSlotHeight};

  // 检测鼠标悬停
  Vector2 mousePos = GetMousePosition();
  bool hovered = CheckCollisionPointRec(mousePos, slotRect);
  if (hovered) {
    m_hoveredIndex = index;
  }

  bool selected = (index == m_selectedIndex);

  // 背景颜色
  Color bgColor = selected  ? Color{60, 50, 80, 255}
                  : hovered ? Color{50, 50, 60, 255}
                            : Color{35, 35, 45, 255};

  DrawRectangleRounded(slotRect, 0.05f, 4, ColorAlpha(bgColor, m_fadeAlpha));

  // 边框 (根据稀有度着色)
  Color borderColor = GRAY;
  switch (static_cast<Rarity>(data.heirloom.original_rarity)) {
  case Rarity::Mythic:
    borderColor = Color{255, 100, 100, 255};
    break;
  case Rarity::Legendary:
    borderColor = Color{255, 165, 0, 255};
    break;
  case Rarity::Epic:
    borderColor = Color{160, 32, 240, 255};
    break;
  case Rarity::Rare:
    borderColor = Color{255, 255, 0, 255};
    break;
  case Rarity::Magic:
    borderColor = Color{100, 100, 255, 255};
    break;
  default:
    borderColor = GRAY;
    break;
  }

  if (selected) {
    borderColor = ColorBrightness(borderColor, 0.3f);
  }

  DrawRectangleRoundedLines(slotRect, 0.05f, 4,
                            ColorAlpha(borderColor, m_fadeAlpha));

  // 物品名称
  DrawText(data.heirloom.display_name.c_str(), static_cast<int>(x + 10),
           static_cast<int>(y + 10), 18, ColorAlpha(borderColor, m_fadeAlpha));

  // 物品类型和槽位
  const char *slotName = "";
  switch (data.item.slot) {
  case EquipmentSlot::MainHand:
    slotName = "主手";
    break;
  case EquipmentSlot::OffHand:
    slotName = "副手";
    break;
  case EquipmentSlot::Head:
    slotName = "头部";
    break;
  case EquipmentSlot::Chest:
    slotName = "胸甲";
    break;
  case EquipmentSlot::Hands:
    slotName = "手套";
    break;
  case EquipmentSlot::Legs:
    slotName = "腿甲";
    break;
  case EquipmentSlot::Feet:
    slotName = "靴子";
    break;
  case EquipmentSlot::Neck:
    slotName = "项链";
    break;
  case EquipmentSlot::Ring1:
  case EquipmentSlot::Ring2:
  case EquipmentSlot::Ring:
    slotName = "戒指";
    break;
  default:
    slotName = "其他";
    break;
  }

  DrawText(slotName, static_cast<int>(x + 10), static_cast<int>(y + 35), 14,
           ColorAlpha(LIGHTGRAY, m_fadeAlpha * 0.8f));

  // Tier 标识
  char tierText[16];
  utils::FormatToBuffer(tierText, "Tier {}", data.heirloom.tier);
  DrawText(tierText, static_cast<int>(x + kSlotWidth - 70),
           static_cast<int>(y + 10), 14, ColorAlpha(GOLD, m_fadeAlpha));

  // 属性预览 (Attack/Defense)
  char statsText[64];
  if (data.item.attack > 0) {
    utils::FormatToBuffer(statsText, "攻击: {:.0f}", data.item.attack);
  } else if (data.item.defense > 0) {
    utils::FormatToBuffer(statsText, "防御: {:.0f}", data.item.defense);
  } else {
    utils::FormatToBuffer(statsText, "词缀: {}", data.item.affixes.size());
  }

  DrawText(statsText, static_cast<int>(x + 10), static_cast<int>(y + 55), 14,
           ColorAlpha(WHITE, m_fadeAlpha * 0.9f));

  // 有效战力预览
  char powerText[64];
  utils::FormatToBuffer(powerText, "Lv1: {:.0f}% | Lv50: {:.0f}%",
                        data.effective_power_at_level_1,
                        data.effective_power_at_level_50);
  DrawText(powerText, static_cast<int>(x + 10), static_cast<int>(y + 75), 12,
           ColorAlpha(Color{150, 200, 150, 255}, m_fadeAlpha * 0.7f));
}

void HeirloomVaultState::renderTooltip(const HeirloomData &data) {
  Vector2 mousePos = GetMousePosition();

  const float tooltipW = 280.0f;
  const float tooltipH = 200.0f;

  // 确保提示框不超出屏幕
  float tooltipX = mousePos.x + 15;
  float tooltipY = mousePos.y + 15;

  if (tooltipX + tooltipW > GetScreenWidth()) {
    tooltipX = mousePos.x - tooltipW - 10;
  }
  if (tooltipY + tooltipH > GetScreenHeight()) {
    tooltipY = GetScreenHeight() - tooltipH - 10;
  }

  // 背景
  DrawRectangleRounded({tooltipX, tooltipY, tooltipW, tooltipH}, 0.05f, 4,
                       ColorAlpha(Color{20, 20, 30, 245}, m_fadeAlpha));
  DrawRectangleRoundedLines({tooltipX, tooltipY, tooltipW, tooltipH}, 0.05f, 4,
                            ColorAlpha(GOLD, m_fadeAlpha));

  float textY = tooltipY + 10;

  // 名称
  DrawText(data.item.name.c_str(), static_cast<int>(tooltipX + 10),
           static_cast<int>(textY), 16, ColorAlpha(GOLD, m_fadeAlpha));
  textY += 25;

  // 等级要求
  char reqText[32];
  utils::FormatToBuffer(reqText, "需求等级: {}",
                        data.heirloom.original_level_requirement);
  DrawText(reqText, static_cast<int>(tooltipX + 10), static_cast<int>(textY),
           14, ColorAlpha(LIGHTGRAY, m_fadeAlpha));
  textY += 20;

  // 词缀列表 (最多显示 4 个)
  DrawText("词缀:", static_cast<int>(tooltipX + 10), static_cast<int>(textY),
           14, ColorAlpha(Color{150, 180, 255, 255}, m_fadeAlpha));
  textY += 18;

  size_t affixCount = std::min(data.item.affixes.size(), size_t{4});
  for (size_t i = 0; i < affixCount; ++i) {
    const auto &affix = data.item.affixes[i];
    // 使用 GetAffixDescription 获取词缀描述文本
    std::string affixDesc = GetAffixDescription(affix, false);
    DrawText(affixDesc.c_str(), static_cast<int>(tooltipX + 15),
             static_cast<int>(textY), 12, ColorAlpha(SKYBLUE, m_fadeAlpha));
    textY += 16;
  }

  if (data.item.affixes.size() > 4) {
    DrawText("  ...", static_cast<int>(tooltipX + 10), static_cast<int>(textY),
             12, ColorAlpha(GRAY, m_fadeAlpha));
    textY += 16;
  }

  // 传家宝特殊说明
  textY += 10;
  DrawText("传家宝属性随等级提升逐步解锁", static_cast<int>(tooltipX + 10),
           static_cast<int>(textY), 11,
           ColorAlpha(Color{200, 180, 100, 255}, m_fadeAlpha * 0.8f));
}

} // namespace NoMoreDay
