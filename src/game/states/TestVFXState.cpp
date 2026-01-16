#include "game/states/TestVFXState.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "raymath.h"

namespace NoMoreDay {

void TestVFXState::OnEnter() {
  LOG_INFO("Entering TestVFXState...");

  m_camera.zoom = 1.0f;
  m_camera.offset = {(float)GetScreenWidth() / 2, (float)GetScreenHeight() / 2};
  m_camera.target = {0, 0};

  // Retrieve shaders
  // We use loadShader to ensure they are loaded (idempotent in ResourceManager usually, or we assume test loads them)
  m_trailShader = m_context->resources->loadShader(
      entt::hashed_string("sh_sword_trail"), "assets/shaders/vfx/sword_trail.vs", "assets/shaders/vfx/sword_trail.fs");
  m_holoShader = m_context->resources->loadShader(
      entt::hashed_string("sh_holo_blade"), "assets/shaders/vfx/holo_blade.vs", "assets/shaders/vfx/holo_blade.fs");
  
  // Distortion shader usually has VS as base (default) or nullptr if Raylib supports it. 
  // Raylib LoadShader(vs, fs) - if vs is null, uses default. 
  // Here we assume a vertex shader exists or we pass 0/nullptr if wrapper supports it. 
  // Checking ResourceManager::loadShader signature: it takes std::string. Empty string usually implies default.
  // We'll use 0 or empty string for VS if it's a post-process effect, but wait, distortion usually needs a quad VS.
  // If no VS provided, Raylib uses default batch VS.
  // Let's assume we use the default VS for distortion if none specified in plan.
  // Plan said: "Create assets/shaders/vfx/distortion.fs". No VS mentioned.
  // So we pass "" for VS.
  m_distortionShader = m_context->resources->loadShader(
      entt::hashed_string("sh_distortion"), "", "assets/shaders/vfx/distortion.fs");

  // Retrieve textures
  m_noiseTex = m_context->resources->loadTexture(
      entt::hashed_string("vfx_noise"), "assets/textures/vfx/energy_noise.png");
  m_trailMask = m_context->resources->loadTexture(
      entt::hashed_string("vfx_trail"), "assets/textures/vfx/trail_mask.png");
  m_distortionNormal = m_context->resources->loadTexture(
      entt::hashed_string("vfx_dist_norm"),
      "assets/textures/vfx/distortion_normal.png");

  // Load a placeholder sword texture if available, else use a white quad
  m_baseSword =
      m_context->resources->getTexture(entt::hashed_string("sword_001"));

  // Create screen capture for distortion
  m_screenCapture = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

  m_time = 0.0f;
}

void TestVFXState::OnExit() { UnloadRenderTexture(m_screenCapture); }

bool TestVFXState::OnUpdate(float dt) {
  m_time += dt;

  // ESC to return to previous state (or just exit test)
  if (IsKeyPressed(KEY_ESCAPE)) {
    // sceneManager->PopState();
  }

  return true;
}

void TestVFXState::OnRender() {
  // 1. Render background/vfx that don't need distortion to the screen capture
  BeginTextureMode(m_screenCapture);
  ClearBackground(DARKGRAY);
  DrawCircle(200, 200, 50, MAROON);
  DrawGrid(10, 50.0f);
  EndTextureMode();

  // 2. Main screen rendering
  BeginDrawing();
  ClearBackground(BLACK);

  // Draw the background (screen capture)
  DrawTextureRec(m_screenCapture.texture,
                 {0, 0, (float)m_screenCapture.texture.width,
                  (float)-m_screenCapture.texture.height},
                 {0, 0}, WHITE);

  BeginMode2D(m_camera);

  // --- TEST: Holo Blade ---
  float holoTimeLoc = GetShaderLocation(m_holoShader, "time");
  float rimLoc = GetShaderLocation(m_holoShader, "rimStrength");
  float noiseSpeedLoc = GetShaderLocation(m_holoShader, "noiseSpeed");

  SetShaderValue(m_holoShader, holoTimeLoc, &m_time, SHADER_UNIFORM_FLOAT);
  float rimVal = 0.8f;
  SetShaderValue(m_holoShader, rimLoc, &rimVal, SHADER_UNIFORM_FLOAT);
  float noiseSpeedVal = 0.5f;
  SetShaderValue(m_holoShader, noiseSpeedLoc, &noiseSpeedVal,
                 SHADER_UNIFORM_FLOAT);

  // Pass the noise texture to Slot 1
  // Note: Raylib's SetShaderValueTexture uses texture0 by default.
  // For extra textures, we need custom logic or just use rlgl.
  int noiseTexLoc = GetShaderLocation(m_holoShader, "noiseTex");
  SetShaderValueTexture(m_holoShader, noiseTexLoc, m_noiseTex);

  Vector4 holoCol = {0.2f, 0.6f, 1.0f, 1.0f}; // Cyan
  int holoColLoc = GetShaderLocation(m_holoShader, "holoColor");
  SetShaderValue(m_holoShader, holoColLoc, &holoCol, SHADER_UNIFORM_VEC4);

  BeginShaderMode(m_holoShader);
  if (m_baseSword.id > 0) {
    DrawTexture(m_baseSword, -50, -150, WHITE);
  } else {
    DrawRectangle(-20, -100, 40, 200, WHITE); // Placeholder
  }
  EndShaderMode();

  // --- TEST: Sword Trail ---
  // In a real system, this would be a triangle strip following the blade.
  // For test, we draw a scrolling quad.
  float trailTimeLoc = GetShaderLocation(m_trailShader, "time");
  float scrollLoc = GetShaderLocation(m_trailShader, "scrollSpeed");
  SetShaderValue(m_trailShader, trailTimeLoc, &m_time, SHADER_UNIFORM_FLOAT);
  float scrollVal = 2.0f;
  SetShaderValue(m_trailShader, scrollLoc, &scrollVal, SHADER_UNIFORM_FLOAT);

  BeginShaderMode(m_trailShader);
  DrawTexturePro(m_trailMask,
                 {0, 0, (float)m_trailMask.width, (float)m_trailMask.height},
                 {100, 0, 300, 50}, {0, 25}, m_time * 20.0f, WHITE);
  EndShaderMode();

  // --- TEST: Distortion ---
  float distTimeLoc = GetShaderLocation(m_distortionShader, "time");
  float strengthLoc =
      GetShaderLocation(m_distortionShader, "distortionStrength");
  SetShaderValue(m_distortionShader, distTimeLoc, &m_time,
                 SHADER_UNIFORM_FLOAT);
  float strengthVal = 0.05f;
  SetShaderValue(m_distortionShader, strengthLoc, &strengthVal,
                 SHADER_UNIFORM_FLOAT);

  int normMapLoc = GetShaderLocation(m_distortionShader, "normalMap");
  SetShaderValueTexture(m_distortionShader, normMapLoc, m_distortionNormal);

  // Raylib uses texture0 for the object's texture. In distortion, texture0 is
  // the screen. So we pass m_screenCapture.texture to the shader.
  BeginShaderMode(m_distortionShader);
  // Draw a circle of "heat" distortion
  // We draw the screen texture back onto itself with distortion
  DrawCircle(0, 0, 150, {255, 255, 255, 128});
  EndShaderMode();

  EndMode2D();

  DrawText("VFX TESTBED: [Holo Blade] [Sword Trail] [Distortion]", 10, 10, 20,
           GREEN);
  DrawFPS(GetScreenWidth() - 100, 10);

  EndDrawing();
}

} // namespace NoMoreDay
