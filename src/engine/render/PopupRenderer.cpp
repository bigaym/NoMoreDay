#include "engine/render/PopupRenderer.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include <rlgl.h>
#include <string>

namespace NoMoreDay::render {

PopupRenderer& PopupRenderer::Get() {
    static PopupRenderer instance;
    return instance;
}

void PopupRenderer::Init() {
    if (m_initialized) return;

    LOG_INFO("Initializing PopupRenderer...");
    m_popups.resize(MAX_POPUPS);
    
    CreateResources();
    LoadGlyphAtlas();
    
    m_initialized = true;
}

void PopupRenderer::Shutdown() {
    if (!m_initialized) return;

    m_instanceBuffer.Destroy();
    if (m_shader.id != 0) UnloadShader(m_shader);
    if (m_atlas.id != 0) UnloadTexture(m_atlas);
    
    if (m_vbo != 0) rlUnloadVertexBuffer(m_vbo);
    if (m_vao != 0) rlUnloadVertexArray(m_vao);

    m_initialized = false;
}

void PopupRenderer::Emit(Vector2 position, int amount, bool isCrit, Color color) {
    // Find an inactive slot (ring buffer style)
    static int nextSlot = 0;
    
    PopupState& p = m_popups[nextSlot];
    p.position = position;
    p.velocity = { (float)GetRandomValue(-50, 50), (float)GetRandomValue(-150, -100) };
    p.timer = 0.0f;
    p.lifeTime = isCrit ? 1.2f : 0.8f;
    p.amount = amount;
    snprintf(p.amountText, 16, "%d", std::abs(amount));
    p.isCrit = isCrit;
    p.color = color;
    p.active = true;

    nextSlot = (nextSlot + 1) % MAX_POPUPS;
}

void PopupRenderer::EmitStatus(Vector2 position, const char* text, Color color) {
    // Basic implementation: convert text to dummy amount or map to glyphs
    // For Phase 2, we focus on numeric damage popups.
    Emit(position, 0, false, color);
}

void PopupRenderer::Update(float dt) {
    m_activeCount = 0;
    for (auto& p : m_popups) {
        if (!p.active) continue;

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

void PopupRenderer::Render(const Matrix& viewProj) {
    if (!m_initialized || m_activeCount == 0) return;

    // 1. Prepare data for GPU
    components::GPUPopupInstance* gpuData = (components::GPUPopupInstance*)m_instanceBuffer.BeginWrite();
    int totalInstances = 0;
    
    for (const auto& p : m_popups) {
        if (!p.active) continue;

        const char* s = p.amountText;
        int charCount = (int)strlen(s);
        
        for (int i = 0; i < charCount; ++i) {
            if (totalInstances >= MAX_POPUPS) break;

            components::GPUPopupInstance& inst = gpuData[totalInstances];
            inst.position = p.position;
            inst.timer = p.timer;
            inst.lifeTime = p.lifeTime;
            inst.colorPacked = (p.color.r << 0) | (p.color.g << 8) | (p.color.b << 16) | (p.color.a << 24);
            inst.scale = p.isCrit ? 1.5f : 1.0f;
            
            // Set glyph data based on character
            char c = s[i];
            if (c >= '0' && c <= '9') {
                inst.glyphData = (uint32_t)(c - '0');
            } else {
                inst.glyphData = 10; // Placeholder for non-digits
            }

            // Pack flags: bit0 = isCrit, bits1-7 = charIndex, bits8-15 = string length (for centering)
            inst.flags = (p.isCrit ? 1 : 0) | (uint32_t(i) << 1) | (uint32_t(charCount) << 8);

            totalInstances++;
        }
        
        if (totalInstances >= MAX_POPUPS) break;
    }

    m_instanceBuffer.Flush();

    // 2. Execute Instanced Draw
    rlEnableShader(m_shader.id);
    rlSetUniformMatrix(m_uViewProj, viewProj);
    
    rlActiveTextureSlot(0);
    rlEnableTexture(m_atlas.id);
    rlSetUniform(m_uAtlas, &m_atlas.id, RL_SHADER_UNIFORM_SAMPLER2D, 1);

    m_instanceBuffer.BindBase(0);

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
    m_shader = LoadShader("assets/shaders/vfx/popup.vert", "assets/shaders/vfx/popup.frag");
    m_uViewProj = GetShaderLocation(m_shader, "uViewProj");
    m_uAtlas = GetShaderLocation(m_shader, "uAtlas");
    m_uTime = GetShaderLocation(m_shader, "uTime");

    // Create Quad VAO
    float vertices[] = {
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f
    };

    m_vao = rlLoadVertexArray();
    rlEnableVertexArray(m_vao);
    m_vbo = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
    rlSetVertexAttribute(0, 2, RL_FLOAT, false, 4 * sizeof(float), 0);
    rlEnableVertexAttribute(0);
    rlSetVertexAttribute(1, 2, RL_FLOAT, false, 4 * sizeof(float), 2 * sizeof(float));
    rlEnableVertexAttribute(1);
    rlDisableVertexArray();
}

void PopupRenderer::LoadGlyphAtlas() {
    // Try to load existing atlas, or create placeholder
    m_atlas = LoadTexture("assets/textures/vfx/popup_glyphs.png");
    if (m_atlas.id == 0) {
        LOG_WARN("PopupRenderer: Could not load glyph atlas, using placeholder");
        Image img = GenImageColor(512, 64, { 255, 255, 255, 100 });
        m_atlas = LoadTextureFromImage(img);
        UnloadImage(img);
    }
}

} // namespace NoMoreDay::render
