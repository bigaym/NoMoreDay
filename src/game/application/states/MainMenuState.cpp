#include "game/application/states/MainMenuState.hpp"
#include "game/foundation/data/BiomeTypes.hpp"
#include "game/application/persistence/SaveManager.hpp"
#include "game/application/scene/StateManager.hpp"
#include "game/application/states/GameplayState.hpp"
#include "game/application/states/LoadingState.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/systems/world/WorldConstants.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "game/systems/world/PortalSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/application/ui/UIRenderer.hpp"
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
  systems::GPUParticleSystem::Get().Clear();
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
          *levelData = levelMgr->prepareLevel(
              NoMoreDay::BiomeID::Town,
              LevelManager::DEFAULT_MAP_WIDTH,
              LevelManager::DEFAULT_MAP_HEIGHT, 1);
        },
        [levelMgr, levelData, ctx](StateManager &mgr) {
          ctx->registry->clear();
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
          *levelData = levelMgr->prepareLevel(
              NoMoreDay::BiomeID::Town,
              LevelManager::DEFAULT_MAP_WIDTH,
              LevelManager::DEFAULT_MAP_HEIGHT, 1);
        },
        [ctx, levelMgr, levelData](StateManager &mgr) {
          // 1. Restore save data into the world first
          SaveManager::Get().loadCharacter(*ctx->registry, 0);
          // 2. Activate level (spawns level entities like portals and stashes into the registry)
          levelMgr->activateLevel(std::move(*levelData));
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
  if (!m_isGameStarting && ps.IsInitialized()) {
      ps.Render(menuCamera);
  } else if (!ps.IsInitialized()) {
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
    
    // Physics Flags (Subtle drift only)
    const uint32_t FLAG_NO_DRAG = 1 << 8;
    const uint32_t FLAG_WANDER  = 1 << 9;
    const uint32_t FLAG_SINE_X  = 1 << 10;
    
    if (spawnTimer > 0.08f) { // Faster spawn to fill space
        spawnTimer = 0.0f;
        auto& ps = systems::GPUParticleSystem::Get();
        if (!ps.IsInitialized()) return;

        int count = GetRandomValue(2, 4); 
        
        for (int i = 0; i < count; ++i) {
            components::GPUParticle p;
            
            // Spawn across the whole screen width, mostly from edges
            p.position = { 
                (float)GetRandomValue(-100, GetScreenWidth() + 100), 
                (float)GetRandomValue(-100, GetScreenHeight() + 100) 
            };
            
            // Very slow drift
            p.velocity = { (float)GetRandomValue(-25, 25), (float)GetRandomValue(-20, 10) };
            p.acceleration = { 0, 0 };
            
            int typeRoll = GetRandomValue(0, 100);
            if (typeRoll < 50) {
                // Faint Ink Dust (Darker, slightly larger)
                p.color = {30, 30, 40, (unsigned char)GetRandomValue(120, 180)}; 
                p.flags = 0 | FLAG_NO_DRAG | FLAG_WANDER; 
                p.scale = (float)GetRandomValue(6, 12); 
            } else {
                // Faint Spirit Dust (Pale Cyan, smaller glow)
                p.color = {180, 245, 255, (unsigned char)GetRandomValue(100, 160)}; 
                p.flags = 1 | FLAG_NO_DRAG | FLAG_SINE_X;
                p.scale = (float)GetRandomValue(4, 8); 
            }
            
            p.lifetime = (float)GetRandomValue(12, 25);
            p.maxLifetime = p.lifetime;
            p.growthRate = 0.02f; // Slight growth instead of shrinking to keep visible
            ps.Emit(p);
        }
    }
}

} // namespace NoMoreDay
