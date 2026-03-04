// src/game/states/NightmareFloorState.cpp
// 无尽梦魇层间过渡界面实现
#include "game/states/NightmareFloorState.hpp"
#include "core/logging/Logger.hpp"
#include "engine/scene/StateManager.hpp"
#include <cmath>
#include <raylib.h>


namespace NoMoreDay {

void NightmareFloorState::OnEnter() {
  LOG_INFO("[NightmareFloorState] Entering floor transition...");
  m_fadeAlpha = 0.0f;
  m_displayTimer = 0.0f;
  m_isAdvancing = false;
}

void NightmareFloorState::OnExit() {
  LOG_INFO("[NightmareFloorState] Exiting floor transition.");
}

bool NightmareFloorState::OnUpdate(float dt) {
  // 淡入动画
  m_fadeAlpha = std::min(m_fadeAlpha + dt * 3.0f, 1.0f);

  // 展示计时
  m_displayTimer += dt;

  handleInput();

  return true;
}

void NightmareFloorState::handleInput() {
  // 空格或回车快速推进
  if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
    if (!m_isAdvancing) {
      m_isAdvancing = true;
      auto &corruption = CorruptionSystem::Get();
      corruption.advanceFloor();
      corruption.recordHighestFloor();
      m_stateManager->PopState();
      return;
    }
  }

  // ESC 返回主菜单 (放弃当前进度)
  if (IsKeyPressed(KEY_ESCAPE)) {
    // 记录当前成绩
    auto &corruption = CorruptionSystem::Get();
    auto &leaderboard = LeaderboardSystem::Get();

    LeaderboardEntry entry;
    entry.highest_floor = corruption.getCurrentFloor();
    entry.peak_dps = corruption.getPeakDPS();
    entry.corruption_reached = corruption.getCorruption();

    leaderboard.addEntry(entry);
    leaderboard.save();

    corruption.reset();
    m_stateManager->PopState();
    return;
  }

  // 自动推进
  if (m_displayTimer >= kDisplayDuration && !m_isAdvancing) {
    m_isAdvancing = true;
    CorruptionSystem::Get().advanceFloor();
    CorruptionSystem::Get().recordHighestFloor();
    m_stateManager->PopState();
  }
}

void NightmareFloorState::OnRender() {
  const auto &corruption = CorruptionSystem::Get();
  const float screenW = static_cast<float>(GetScreenWidth());
  const float screenH = static_cast<float>(GetScreenHeight());

  // 半透明背景
  DrawRectangle(0, 0, static_cast<int>(screenW), static_cast<int>(screenH),
                ColorAlpha(BLACK, 0.8f * m_fadeAlpha));

  // 面板位置
  const float panelX = (screenW - kPanelWidth) / 2.0f;
  const float panelY = (screenH - kPanelHeight) / 2.0f;

  // 面板背景
  DrawRectangleRounded({panelX, panelY, kPanelWidth, kPanelHeight}, 0.03f, 8,
                       ColorAlpha(Color{20, 15, 30, 255}, m_fadeAlpha));

  // 边框 - 紫色腐化主题
  DrawRectangleRoundedLines({panelX, panelY, kPanelWidth, kPanelHeight}, 0.03f,
                            8,
                            ColorAlpha(Color{150, 50, 200, 255}, m_fadeAlpha));

  renderFloorInfo();
  renderCorruptionInfo();
  renderDifficultyPreview();
  renderButtons();
}

void NightmareFloorState::renderFloorInfo() {
  const auto &corruption = CorruptionSystem::Get();
  const float screenW = static_cast<float>(GetScreenWidth());
  const float screenH = static_cast<float>(GetScreenHeight());
  const float panelX = (screenW - kPanelWidth) / 2.0f;
  const float panelY = (screenH - kPanelHeight) / 2.0f;

  // 标题
  const char *title = "无尽梦魇";
  const int titleSize = 32;
  const int titleWidth = MeasureText(title, titleSize);
  DrawText(title, static_cast<int>(panelX + (kPanelWidth - titleWidth) / 2.0f),
           static_cast<int>(panelY + 20), titleSize,
           ColorAlpha(Color{200, 100, 255, 255}, m_fadeAlpha));

  // 当前层数
  char floorText[64];
  utils::FormatToBuffer(floorText, "第 {} 层", corruption.getCurrentFloor());
  const int floorWidth = MeasureText(floorText, 48);
  DrawText(floorText,
           static_cast<int>(panelX + (kPanelWidth - floorWidth) / 2.0f),
           static_cast<int>(panelY + 65), 48, ColorAlpha(WHITE, m_fadeAlpha));

  // Boss层标识
  if (corruption.isBossFloor()) {
    const char *bossText = "⚔ BOSS 层 ⚔";
    const int bossWidth = MeasureText(bossText, 24);
    DrawText(bossText,
             static_cast<int>(panelX + (kPanelWidth - bossWidth) / 2.0f),
             static_cast<int>(panelY + 120), 24, ColorAlpha(RED, m_fadeAlpha));
  }
}

void NightmareFloorState::renderCorruptionInfo() {
  const auto &corruption = CorruptionSystem::Get();
  const float screenW = static_cast<float>(GetScreenWidth());
  const float screenH = static_cast<float>(GetScreenHeight());
  const float panelX = (screenW - kPanelWidth) / 2.0f;
  const float panelY = (screenH - kPanelHeight) / 2.0f;

  const float infoY = panelY + 150;

  // 腐化值
  char corruptText[64];
  utils::FormatToBuffer(corruptText, "腐化值: {}",
                        corruption.getCorruption());
  DrawText(corruptText, static_cast<int>(panelX + 30), static_cast<int>(infoY),
           20, ColorAlpha(Color{180, 100, 220, 255}, m_fadeAlpha));

  // 腐化进度条
  const float barX = panelX + 30;
  const float barY = infoY + 28;
  const float barW = kPanelWidth - 60;
  const float barH = 12;

  // 背景
  DrawRectangleRounded({barX, barY, barW, barH}, 0.5f, 4,
                       ColorAlpha(Color{40, 30, 50, 255}, m_fadeAlpha));

  // 填充 (腐化值/1000 为满)
  float fillRatio = std::min(1.0f, corruption.getCorruption() / 1000.0f);
  DrawRectangleRounded({barX, barY, barW * fillRatio, barH}, 0.5f, 4,
                       ColorAlpha(Color{150, 50, 200, 255}, m_fadeAlpha));
}

void NightmareFloorState::renderDifficultyPreview() {
  const auto &corruption = CorruptionSystem::Get();
  const float screenW = static_cast<float>(GetScreenWidth());
  const float screenH = static_cast<float>(GetScreenHeight());
  const float panelX = (screenW - kPanelWidth) / 2.0f;
  const float panelY = (screenH - kPanelHeight) / 2.0f;

  const float infoY = panelY + 200;

  DrawText("难度加成:", static_cast<int>(panelX + 30), static_cast<int>(infoY),
           18, ColorAlpha(LIGHTGRAY, m_fadeAlpha));

  // 怪物属性倍率
  char statText[64];
  utils::FormatToBuffer(statText, "  怪物属性: ×{:.1f}",
                        corruption.calculateStatMultiplier());
  DrawText(statText, static_cast<int>(panelX + 30),
           static_cast<int>(infoY + 25), 16,
           ColorAlpha(Color{255, 150, 150, 255}, m_fadeAlpha));

  // 怪物生命倍率
  char hpText[64];
  utils::FormatToBuffer(hpText, "  怪物生命: ×{:.1f}",
                        corruption.calculateHealthMultiplier());
  DrawText(hpText, static_cast<int>(panelX + 30), static_cast<int>(infoY + 45),
           16, ColorAlpha(Color{255, 150, 150, 255}, m_fadeAlpha));

  // 掉落加成
  char dropText[64];
  utils::FormatToBuffer(dropText, "  掉落加成: +{:.0f}%",
                        corruption.calculateDropRateBonus() * 100.0f);
  DrawText(dropText, static_cast<int>(panelX + 30),
           static_cast<int>(infoY + 65), 16,
           ColorAlpha(Color{150, 255, 150, 255}, m_fadeAlpha));

  // 双T7几率
  char t7Text[64];
  utils::FormatToBuffer(t7Text, "  双T7几率: {:.1f}%",
                        corruption.calculateDoubleT7Chance() * 100.0f);
  DrawText(t7Text, static_cast<int>(panelX + 30), static_cast<int>(infoY + 85),
           16, ColorAlpha(GOLD, m_fadeAlpha));
}

void NightmareFloorState::renderButtons() {
  const float screenW = static_cast<float>(GetScreenWidth());
  const float screenH = static_cast<float>(GetScreenHeight());
  const float panelX = (screenW - kPanelWidth) / 2.0f;
  const float panelY = (screenH - kPanelHeight) / 2.0f;

  const float btnY = panelY + kPanelHeight - 60;

  // 提示文字
  const char *hint = "按 [空格] 继续  |  [ESC] 结束挑战";
  const int hintWidth = MeasureText(hint, 16);
  DrawText(hint, static_cast<int>(panelX + (kPanelWidth - hintWidth) / 2.0f),
           static_cast<int>(btnY + 5), 16,
           ColorAlpha(GRAY, m_fadeAlpha * 0.8f));

  // 倒计时进度条
  const float countdownY = btnY + 30;
  const float barW = kPanelWidth - 100;
  const float barX = panelX + 50;

  DrawRectangleRounded({barX, countdownY, barW, 8}, 0.5f, 4,
                       ColorAlpha(Color{40, 40, 50, 255}, m_fadeAlpha));

  float progress = std::min(1.0f, m_displayTimer / kDisplayDuration);
  DrawRectangleRounded({barX, countdownY, barW * progress, 8}, 0.5f, 4,
                       ColorAlpha(Color{100, 200, 100, 255}, m_fadeAlpha));
}

} // namespace NoMoreDay
