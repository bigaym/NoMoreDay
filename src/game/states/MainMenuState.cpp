#include "game/states/MainMenuState.hpp"
#include "engine/persistence/SaveManager.hpp"
#include "engine/scene/StateManager.hpp"
#include "game/states/GameplayState.hpp"
#include "game/states/LoadingState.hpp"
#include "game/systems/ui/UISystem.hpp" // Include UISystem
#include "game/systems/world/LevelManager.hpp"
#include "game/systems/world/PortalSystem.hpp" // Required for GameplayState's unique_ptr<PortalSystem>
#include <filesystem>
#include <memory>
#include <raylib.h>
#include <thread>
#include "engine/resource/UIAssetRegistry.hpp"
#include "engine/resource/ResourceManager.hpp"

namespace NoMoreDay {

MainMenuState::MainMenuState(StateManager &manager, SharedContext &context)
    : IState(manager, context) {
  float screenWidth = (float)GetScreenWidth();
  float screenHeight = (float)GetScreenHeight();

  float btnWidth = 260;
  float btnHeight = 75;
  float centerX = (screenWidth - btnWidth) / 2.0f;

  m_startButton = {
      {centerX, screenHeight * 0.45f, btnWidth, btnHeight}, "NEW GAME", false};
  m_continueButton = {{centerX, screenHeight * 0.45f + 95, btnWidth, btnHeight},
                      "CONTINUE",
                      false};
  m_exitButton = {{centerX, screenHeight * 0.45f + 190, btnWidth, btnHeight},
                  "EXIT",
                  false};

  m_hasSave = std::filesystem::exists("saves/slot_0.json");
}

void MainMenuState::OnEnter() {
  // Load main menu background
  m_context->resources->loadTexture(assets::ui::textures::Home_Page.id,
                                    std::string(assets::ui::textures::Home_Page.path));
  m_titleOpacity = 0.0f;
}

void MainMenuState::OnExit() {
  // Cleanup
}

bool MainMenuState::OnUpdate(float dt) {
  m_titleOpacity = std::min(1.0f, m_titleOpacity + dt * 2.0f);

  Vector2 mousePos = GetMousePosition();

  m_startButton.hovered =
      CheckCollisionPointRec(mousePos, m_startButton.bounds);
  m_continueButton.hovered =
      m_hasSave && CheckCollisionPointRec(mousePos, m_continueButton.bounds);
  m_exitButton.hovered = CheckCollisionPointRec(mousePos, m_exitButton.bounds);

  if (IsButtonClicked(m_startButton)) {
    auto levelData = std::make_shared<LevelManager::LevelData>();
    auto *levelMgr = m_context->levelManager;

    SharedContext* ctx = m_context;
    // Transition to Loading State
    m_stateManager->ChangeState<LoadingState>(
        [levelMgr, levelData]() {
          *levelData =
              levelMgr->prepareLevel(NoMoreDay::BiomeID::Cave, 128, 128, 1);
        },
        [levelMgr, levelData, ctx](StateManager &mgr) {
          levelMgr->activateLevel(std::move(*levelData));
          mgr.ChangeState<GameplayState>(*ctx->renderContext);
        });
  } else if (m_hasSave && IsButtonClicked(m_continueButton)) {
    auto levelData = std::make_shared<LevelManager::LevelData>();
    auto *levelMgr = m_context->levelManager;

    SharedContext* ctx = m_context;
    m_stateManager->ChangeState<LoadingState>(
        [levelMgr, levelData]() {
          // Towns are usually smaller
          *levelData =
              levelMgr->prepareLevel(NoMoreDay::BiomeID::Town, 64, 64, 1);
        },
        [ctx, levelMgr, levelData](StateManager &mgr) {
          levelMgr->activateLevel(std::move(*levelData));
          // Restore save data into the world
          SaveManager::Get().loadCharacter(*ctx->registry, 0);
          mgr.ChangeState<GameplayState>(*ctx->renderContext);
        });
  } else if (IsButtonClicked(m_exitButton)) {
    m_stateManager
        ->PopState(); // Popping the last state will exit the game loop
  }

  return true;
}

void MainMenuState::OnRender() {
  ClearBackground(BLACK);

  // Draw Background
  Texture2D bg = m_context->resources->getTexture(assets::ui::textures::Home_Page.id);
  if (bg.id > 0) {
      Rectangle source = {0.0f, 0.0f, (float)bg.width, (float)bg.height};
      Rectangle dest = {0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight()};
      DrawTexturePro(bg, source, dest, {0.0f, 0.0f}, 0.0f, WHITE);
  }

  Font font = UISystem::GetFont();

  // Draw Title -- 跳过，因为背景图有文字显示
  // const char *title = "NOMOREDAY";
  // float fontSize = 80.0f;
  // float titleWidth = IsFontValid(font)
  //                        ? MeasureTextEx(font, title, fontSize, 1.0f).x
  //                        : (float)MeasureText(title, (int)fontSize);

  // if (IsFontValid(font)) {
  //   DrawTextEx(
  //       font, title,
  //       {(GetScreenWidth() - titleWidth) / 2.0f, GetScreenHeight() * 0.2f},
  //       fontSize, 1.0f, Fade(RED, m_titleOpacity));
  // } else {
  //   DrawText(title, (int)(GetScreenWidth() - titleWidth) / 2,
  //            (int)(GetScreenHeight() * 0.2f), (int)fontSize,
  //            Fade(RED, m_titleOpacity));
  // }

  // Draw Buttons
  DrawButton(m_startButton);
  DrawButton(m_continueButton, m_hasSave);
  DrawButton(m_exitButton);

  // Version Info
  const char *ver = "v0.1 Alpha - State Manager Demo";
  float verSize = 20.0f;
  if (IsFontValid(font)) {
    DrawTextEx(font, ver, {10.0f, GetScreenHeight() - 25.0f}, verSize, 1.0f,
               DARKGRAY);
  } else {
    DrawText(ver, 10, GetScreenHeight() - 25, (int)verSize, DARKGRAY);
  }
}

void MainMenuState::DrawButton(const Button &btn, bool enabled) {
  Texture2D tex = m_context->resources->getTexture(assets::ui::textures::Button_Menu.id);
  
  Color tint = enabled ? WHITE : GRAY;
  Color textColor = enabled ? (btn.hovered ? YELLOW : WHITE) : LIGHTGRAY;

  UIRenderer::DrawButton(
      UISystem::GetFont(),
      tex,
      btn.bounds,
      btn.text.c_str(),
      32.0f,
      textColor,
      tint,
      enabled && btn.hovered,
      enabled && btn.hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT)
  );
}

bool MainMenuState::IsButtonClicked(const Button &btn) {
  return btn.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

} // namespace NoMoreDay
