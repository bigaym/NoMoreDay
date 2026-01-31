#include "game/states/MainMenuState.hpp"
#include "engine/persistence/SaveManager.hpp"
#include "engine/scene/StateManager.hpp"
#include "game/states/GameplayState.hpp"
#include "game/states/LoadingState.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "game/systems/world/PortalSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/UIRenderer.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "engine/resource/ResourceManager.hpp"
#include <filesystem>
#include <memory>
#include <raylib.h>

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
  m_timer += dt;
  if (!m_isGameStarting) {
    SpawnGPUParticles();
  }

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
    // Clear menu particles before loading gameplay
    m_isGameStarting = true;
    systems::GPUParticleSystem::Get().Clear();
    
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
    // Clear menu particles before loading gameplay
    m_isGameStarting = true;
    systems::GPUParticleSystem::Get().Clear();
    
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
      
      // Localized masks are now handled inside DrawButton
      // particles are rendered at the end of OnRender
  }

  Font font = UISystem::GetFont();

  // Draw Buttons
  DrawButton(m_startButton);
  DrawButton(m_continueButton, m_hasSave);
  DrawButton(m_exitButton);

  // 1. Dynamic GPU Particles (Ink/Ash)
  // Correct camera for 1:1 Screen Space rendering
  Camera2D menuCamera = { 0 };
  menuCamera.target = { 0, 0 };
  menuCamera.offset = { 0, 0 };
  menuCamera.rotation = 0.0f;
  menuCamera.zoom = 1.0f; 
  
  auto& ps = systems::GPUParticleSystem::Get();
  if (ps.IsInitialized()) {
      ps.Render(menuCamera);
  } else {
      LOG_LIMITED_WARN(5.0f, "MainMenuState: GPUParticleSystem NOT initialized during render!");
  }

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
  // Buttons already drawn above now
  // 按钮已在上方提前绘制，确保它们在粒子下方层级
  // DrawButton(m_startButton);
  // DrawButton(m_continueButton, m_hasSave);
  // DrawButton(m_exitButton);

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
  
  // Localized Pulsating Mask behind the button (Elliptical for better button coverage)
  // 按钮背后的局部脉动遮罩（强化阴影以应对亮色背景，并改为椭圆范围）
  float breathe = (sinf(m_timer * 1.5f) + 1.0f) * 0.5f;
  float shadowAlpha = 0.5f + breathe * 0.2f; // Increased from 0.35
  
  Vector2 center = {btn.bounds.x + btn.bounds.width / 2.0f, btn.bounds.y + btn.bounds.height / 2.0f};
  
  // Use multiple overlapping circles to approximate an ellipse for better coverage
  // 使用多个重叠圆来模拟椭圆阴影，确保完全覆盖按钮区域
  for (float offset : {-60.0f, 0.0f, 60.0f}) {
      DrawCircleGradient((int)(center.x + offset), (int)center.y, btn.bounds.height * 1.2f, Fade(BLACK, shadowAlpha * 0.6f), Fade(BLACK, 0.0f));
  }
  
  // Central core shadow
  DrawCircleGradient((int)center.x, (int)center.y, btn.bounds.width * 0.6f, Fade(BLACK, shadowAlpha), Fade(BLACK, 0.0f));

  // Use a slightly darker tint for the button texture to make the light text pop
  Color tint = enabled ? Color{160, 160, 160, 255} : GRAY;
  bool isPressed = enabled && btn.hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  Color textColor = enabled ? (isPressed ? components::Colors::MENU_BTN_TEXT_PRESS : (btn.hovered ? components::Colors::MENU_BTN_TEXT_HOVER : components::Colors::MENU_BTN_TEXT_NORMAL)) : LIGHTGRAY;

  UIRenderer::DrawButton(
      UISystem::GetFont(),
      tex,
      btn.bounds,
      btn.text.c_str(),
      32.0f,
      textColor,
      tint,
      enabled && btn.hovered,
      isPressed
  );
}

bool MainMenuState::IsButtonClicked(const Button &btn) {
  return btn.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void MainMenuState::SpawnGPUParticles() {
    static float spawnTimer = 0.0f;
    spawnTimer += GetFrameTime();
    
    if (spawnTimer > 0.05f) { 
        spawnTimer = 0.0f;
        
        auto& ps = systems::GPUParticleSystem::Get();
        if (!ps.IsInitialized()) {
            LOG_LIMITED_WARN(5.0f, "MainMenuState: Cannot spawn particles, GPUParticleSystem NOT initialized!");
            return;
        }

        int count = GetRandomValue(2, 6); // More particles per burst
        LOG_LIMITED_DEBUG(1.0f, "MainMenuState: Emitting {} GPU particles...", count);
        
        for (int i = 0; i < count; ++i) {
            components::GPUParticle p;
            // Spread particles across the width
            p.position = {(float)GetRandomValue(-200, GetScreenWidth() + 200), -50.0f};
            
            // Random downward drift with horizontal sway (Slower as requested)
            float speedY = (float)GetRandomValue(120, 360); // Halved from 240-720
            float speedX = (float)GetRandomValue(-80, 80);  // Halved from -160-160
            p.velocity = {speedX, speedY};
            
            // Gentler acceleration 
            p.acceleration = {sinf(m_timer * 0.3f + (float)i) * 10.0f, 5.0f}; 
            
            bool isAsh = GetRandomValue(0, 100) < 35;
            if (isAsh) {
                p.color = {220, 230, 255, (unsigned char)GetRandomValue(180, 255)}; 
                p.flags = 1; 
                p.scale = (float)GetRandomValue(8, 20); 
            } else {
                p.color = {5, 5, 10, (unsigned char)GetRandomValue(180, 255)}; 
                p.flags = 13; 
                p.scale = (float)GetRandomValue(12, 24); 
            }
            
            p.lifetime = (float)GetRandomValue(15, 25); // Longer existence
            p.maxLifetime = p.lifetime;
            p.growthRate = -0.02f; // Much slower shrinking to prevent early "disappearance"
            
            ps.Emit(p);
        }
    }
}

} // namespace NoMoreDay
