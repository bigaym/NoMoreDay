#include "engine/render/PopupRenderer.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include <rlgl.h>
#include <string>

namespace NoMoreDay::render {

PopupRenderer &PopupRenderer::Get() {
  static PopupRenderer instance;
  return instance;
}

void PopupRenderer::Init() {
  if (m_initialized)
    return;

  LOG_INFO("Initializing PopupRenderer...");
  m_popups.resize(MAX_POPUPS);

  CreateResources();
  LoadGlyphAtlas();

  m_initialized = true;
}

void PopupRenderer::Shutdown() {
  if (!m_initialized)
    return;

  m_instanceBuffer.Destroy();
  if (m_shader.id != 0)
    UnloadShader(m_shader);
  if (m_atlas.id != 0)
    UnloadTexture(m_atlas);

  if (m_vbo != 0) {
    NoMoreDay::render::resources::GPUResourceRegistry::Get().UnregisterResource(
        m_vbo, NoMoreDay::render::graph::ResourceKind::VertexBuffer);
    rlUnloadVertexBuffer(m_vbo);
    m_vbo = 0;
  }
  if (m_vao != 0) {
    NoMoreDay::render::resources::GPUResourceRegistry::Get().UnregisterResource(
        m_vao, NoMoreDay::render::graph::ResourceKind::VertexArray);
    rlUnloadVertexArray(m_vao);
    m_vao = 0;
  }

  m_initialized = false;
}

void PopupRenderer::Emit(Vector2 position, int amount, bool isCrit,
                         Color color) {
  if (!m_initialized)
    return;

  PopupState &p = m_popups[m_nextSlot];
  p.position = position;
  p.velocity = {(float)GetRandomValue(-40, 40),
                (float)GetRandomValue(-160, -120)};
  p.timer = 0.0f;
  p.lifeTime = isCrit ? 1.0f : 0.7f;
  p.amount = amount;
  p.isCrit = isCrit;
  p.color = color;
  p.active = true;

  // Convert to glyph indices (digits 0-9)
  std::string s = std::to_string(std::abs(amount));
  p.glyphCount = (uint8_t)std::min((size_t)16, s.length());
  for (int i = 0; i < p.glyphCount; ++i) {
    p.glyphs[i] = (uint8_t)(s[i] - '0');
  }

  m_nextSlot = (m_nextSlot + 1) % MAX_POPUPS;
}

void PopupRenderer::EmitStatus(Vector2 position, const char *text,
                               Color color) {
  if (!m_initialized)
    return;

  PopupState &p = m_popups[m_nextSlot];
  p.position = position;
  p.velocity = {0.0f, -80.0f}; // Status float up
  p.timer = 0.0f;
  p.lifeTime = 1.2f;
  p.amount = 0;
  p.isCrit = false;
  p.color = color;
  p.active = true;

  // Map Chinese status text to Row 1 (indices 16+)
  std::string s = text;
  p.glyphCount = 0;

  if (s == "暴击") {
    p.glyphs[0] = 16;
    p.glyphs[1] = 17;
    p.glyphCount = 2;
  } else if (s == "闪避") {
    p.glyphs[0] = 18;
    p.glyphs[1] = 19;
    p.glyphCount = 2;
  } else if (s == "格挡") {
    p.glyphs[0] = 20;
    p.glyphs[1] = 21;
    p.glyphCount = 2;
  } else if (s == "免疫") {
    p.glyphs[0] = 22;
    p.glyphs[1] = 23;
    p.glyphCount = 2;
  } else if (s == "吸收") {
    p.glyphs[0] = 24;
    p.glyphs[1] = 25;
    p.glyphCount = 2;
  } else {
    p.glyphs[0] = 15; // Question mark
    p.glyphCount = 1;
  }

  m_nextSlot = (m_nextSlot + 1) % MAX_POPUPS;
}

void PopupRenderer::Update(float dt) {
  m_activeCount = 0;
  for (auto &p : m_popups) {
    if (!p.active)
      continue;

    p.timer += dt;
    if (p.timer >= p.lifeTime) {
      p.active = false;
      continue;
    }

    // Apply physics
    p.position.x += p.velocity.x * dt;
    p.position.y += p.velocity.y * dt;
    p.velocity.y += 300.0f * dt; // Gravity

    m_activeCount++;
  }
}

void PopupRenderer::Render(const Matrix &viewProj) {
  if (!m_initialized || m_activeCount == 0)
    return;

  // 1. Prepare data for GPU
  components::GPUPopupInstance *gpuData =
      (components::GPUPopupInstance *)m_instanceBuffer.BeginWrite();
  int totalInstances = 0;

  for (const auto &p : m_popups) {
    if (!p.active)
      continue;

    int charCount = p.glyphCount;

    for (int i = 0; i < charCount; ++i) {
      if (totalInstances >= MAX_POPUPS)
        break;

      components::GPUPopupInstance &inst = gpuData[totalInstances];
      inst.position = p.position;
      inst.timer = p.timer;
      inst.lifeTime = p.lifeTime;
      inst.colorPacked = (p.color.r << 0) | (p.color.g << 8) |
                         (p.color.b << 16) | (p.color.a << 24);
      inst.scale = p.isCrit ? 1.5f : 1.0f;
      inst.glyphData = (uint32_t)p.glyphs[i];

      // Pack flags: bit0 = isCrit, bits1-7 = charIndex, bits8-15 = string
      // length
      inst.flags =
          (p.isCrit ? 1 : 0) | (uint32_t(i) << 1) | (uint32_t(charCount) << 8);

      totalInstances++;
    }

    if (totalInstances >= MAX_POPUPS)
      break;
  }

  m_instanceBuffer.Flush();

  // 2. Execute Instanced Draw
  rlEnableShader(m_shader.id);
  rlSetUniformMatrix(m_uViewProj, viewProj);

  // Bind atlas to slot 0
  rlActiveTextureSlot(0);
  rlEnableTexture(m_atlas.id);
  int unit = 0;
  rlSetUniform(m_uAtlas, &unit, RL_SHADER_UNIFORM_SAMPLER2D, 1);

  // Bind SSBO (RenderConstants::Binding::SSBO_POPUP_DATA)
  using NoMoreDay::RenderConstants::Binding;
  m_instanceBuffer.BindBase(static_cast<uint32_t>(Binding::SSBO_POPUP_DATA));

  rlEnableVertexArray(m_vao);
  rlDrawVertexArrayInstanced(0, 6, totalInstances);
  rlDisableVertexArray();

  rlDisableTexture();
  rlDisableShader();

  m_instanceBuffer.Lock();
}

void PopupRenderer::CreateResources() {
  // Persistent Buffer for instances
  m_instanceBuffer.Create(MAX_POPUPS * sizeof(components::GPUPopupInstance));

  // Load Shaders
  m_shader = LoadShader("assets/shaders/vfx/popup.vert",
                        "assets/shaders/vfx/popup.frag");
  m_uViewProj = GetShaderLocation(m_shader, "uViewProj");
  m_uAtlas = GetShaderLocation(m_shader, "uAtlas");
  m_uTime = GetShaderLocation(m_shader, "uTime");

  // Create Quad VAO
  float vertices[] = {-0.5f, -0.5f, 0.0f, 0.0f, 0.5f,  -0.5f, 1.0f, 0.0f,
                      0.5f,  0.5f,  1.0f, 1.0f, -0.5f, -0.5f, 0.0f, 0.0f,
                      0.5f,  0.5f,  1.0f, 1.0f, -0.5f, 0.5f,  0.0f, 1.0f};

  m_vao = rlLoadVertexArray();
  NoMoreDay::render::resources::GPUResourceRegistry::Get().RegisterResource(
      m_vao, NoMoreDay::render::graph::ResourceKind::VertexArray,
      NoMoreDay::render::graph::RenderOwnerTag::Unknown, 0u, "PopupQuadVAO");
  rlEnableVertexArray(m_vao);
  m_vbo = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
  NoMoreDay::render::resources::GPUResourceRegistry::Get().RegisterResource(
      m_vbo, NoMoreDay::render::graph::ResourceKind::VertexBuffer,
      NoMoreDay::render::graph::RenderOwnerTag::Unknown, sizeof(vertices),
      "PopupQuadVBO");
  rlSetVertexAttribute(0, 2, RL_FLOAT, false, 4 * sizeof(float), 0);
  rlEnableVertexAttribute(0);
  rlSetVertexAttribute(1, 2, RL_FLOAT, false, 4 * sizeof(float),
                       2 * sizeof(float));
  rlEnableVertexAttribute(1);
  rlDisableVertexArray();
}

void PopupRenderer::LoadGlyphAtlas() {
  // Load from registry via AssetLoadingSystem
  using namespace assets::ui::fonts;
  m_atlas = NoMoreDay::AssetLoadingSystem::GetTexture(Fast_Font_Img.id);

  if (m_atlas.id == 0) {
    LOG_WARN(
        "PopupRenderer: Could not load glyph atlas (ID: {}), using placeholder",
        Fast_Font_Img.id);
    Image img = GenImageColor(1024, 128, {255, 255, 255, 100});
    m_atlas = LoadTextureFromImage(img);
    UnloadImage(img);
  } else {
    LOG_INFO("PopupRenderer: Loaded glyph atlas successfully.");
  }
}

} // namespace NoMoreDay::render
