#include "game/systems/vfx/HoloBladeRenderSystem.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/Common.hpp"
#include "game/components/vfx/HoloBladeComponent.hpp"
#include "raymath.h"
#include "rlgl.h"
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "core/logging/Logger.hpp"
#include <vector>
#include <algorithm>
#include <utility>

namespace NoMoreDay::systems {

struct HoloBladeInternal {
    Shader holoShader = {0};
    Texture2D noiseTex = {0};
    core::ComputeBuffer instanceBuffer;
    unsigned int quadVAO = 0;
    unsigned int quadVBO = 0;
    bool initialized = false;
    
    // Persistent buffers to avoid allocation
    std::vector<components::HoloBladeInstance> hostBuffer;
    std::vector<std::pair<unsigned int, entt::entity>> renderQueue;
    
    // Cache shader locations
    int mvpLoc = -1;
    int timeLoc = -1;
    int noiseLoc = -1;
    int offsetLoc = -1;

    void Init(const NoMoreDay::SharedContext &context) {
        if (initialized) return;
        
        LOG_INFO("Initializing HoloBladeRenderSystem (Instanced)...");
        
        holoShader = LoadShader("assets/shaders/vfx/holo_blade_instanced.vs", 
                                "assets/shaders/vfx/holo_blade_instanced.fs");
        
        // Cache locations
        mvpLoc = GetShaderLocation(holoShader, "mvp");
        timeLoc = GetShaderLocation(holoShader, "time");
        noiseLoc = GetShaderLocation(holoShader, "noiseTex");
        offsetLoc = GetShaderLocation(holoShader, "uInstanceOffset");

        if (context.resources) {
            noiseTex = context.resources->getTexture(entt::hashed_string("vfx_noise"));
        }
        
        // Quad for instancing (-0.5 to 0.5)
        float vertices[] = {
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f
        };
        
        quadVAO = rlLoadVertexArray();
        rlEnableVertexArray(quadVAO);
        quadVBO = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
        rlSetVertexAttribute(0, 3, RL_FLOAT, false, 5 * sizeof(float), 0);
        rlEnableVertexAttribute(0);
        rlSetVertexAttribute(1, 2, RL_FLOAT, false, 5 * sizeof(float), (int)(3 * sizeof(float)));
        rlEnableVertexAttribute(1);
        rlDisableVertexArray();
        
        // Reserve sufficient capacity to avoid runtime reallocations
        instanceBuffer.Create(2000 * sizeof(components::HoloBladeInstance), nullptr, RL_DYNAMIC_DRAW);
        hostBuffer.reserve(2000);
        renderQueue.reserve(2000);
        
        initialized = true;
    }
    
    void Shutdown() {
        if (!initialized) return;
        UnloadShader(holoShader);
        rlUnloadVertexArray(quadVAO);
        rlUnloadVertexBuffer(quadVBO);
        instanceBuffer.Release();
        initialized = false;
    }
};

static HoloBladeInternal s_data;

struct DrawBatch {
    Texture2D texture;
    int startOffset;
    int count;
};

void HoloBladeRenderSystem::Render(entt::registry &registry,
                                   const NoMoreDay::SharedContext &context) {
  s_data.Init(context);
  if (!s_data.initialized) return;

  auto view = registry.view<const components::HoloBlade, const Position,
                            const SpriteComponent>();
  
  if (view.size_hint() == 0) return;

  // 1. Collect entities (Zero-Alloc)
  s_data.renderQueue.clear();
  for (auto entity : view) {
      const auto &holo = view.get<const components::HoloBlade>(entity);
      if (!holo.isVisible) continue;
      const auto &sprite = view.get<const SpriteComponent>(entity);
      s_data.renderQueue.emplace_back(sprite.texture.id, entity);
  }

  if (s_data.renderQueue.empty()) return;

  // 2. Sort by Texture ID to batch draw calls
  std::sort(s_data.renderQueue.begin(), s_data.renderQueue.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  s_data.hostBuffer.clear();
  std::vector<DrawBatch> batches;
  // Reserve roughly assuming not too many different textures per frame
  batches.reserve(10); 

  // 3. Generate Instance Data & Batches
  if (!s_data.renderQueue.empty()) {
      DrawBatch currentBatch;
      currentBatch.count = 0;
      unsigned int currentTexId = 0;
      bool firstBatch = true;

      for (const auto& pair : s_data.renderQueue) {
          entt::entity entity = pair.second;
          const auto &sprite = view.get<const SpriteComponent>(entity);

          // Start new batch if texture changes
          if (firstBatch || sprite.texture.id != currentTexId) {
              if (!firstBatch && currentBatch.count > 0) {
                  batches.push_back(currentBatch);
              }
              
              currentTexId = sprite.texture.id;
              currentBatch.texture = sprite.texture;
              currentBatch.startOffset = (int)s_data.hostBuffer.size();
              currentBatch.count = 0;
              firstBatch = false;
          }

          // Build Instance Data
          const auto &holo = view.get<const components::HoloBlade>(entity);
          const auto &pos = view.get<const Position>(entity);

          components::HoloBladeInstance inst;
          inst.position = {pos.x, pos.y};
          inst.rotation = 0.0f;
          if (auto *rot = registry.try_get<Rotation>(entity)) {
              inst.rotation = rot->angle * (PI / 180.0f);
          }
          
          inst.scale = sprite.scale * holo.scale * (float)currentBatch.texture.width; 
          inst.holoColor = ColorNormalize(holo.holoColor);
          inst.rimStrength = holo.rimStrength;
          inst.noiseSpeed = holo.noiseSpeed;
          
          s_data.hostBuffer.push_back(inst);
          currentBatch.count++;
          
          // Hard cap safety
          if (s_data.hostBuffer.size() >= 4096) break;
      }
      // Push final batch
      if (currentBatch.count > 0) {
          batches.push_back(currentBatch);
      }
  }

  if (s_data.hostBuffer.empty()) return;

  // 4. Single GPU Upload
  // Check resize
  if (s_data.hostBuffer.size() * sizeof(components::HoloBladeInstance) > s_data.instanceBuffer.GetSize()) {
       s_data.instanceBuffer.Create(s_data.hostBuffer.size() * sizeof(components::HoloBladeInstance) * 2, nullptr, RL_DYNAMIC_DRAW);
  }
  s_data.instanceBuffer.Update(s_data.hostBuffer.data(), s_data.hostBuffer.size() * sizeof(components::HoloBladeInstance));
  s_data.instanceBuffer.BindBase(4);

  // 5. Render Batches
  float time = (float)GetTime();
  rlDrawRenderBatchActive();
  
  BeginShaderMode(s_data.holoShader);
  
  // Update MVP matrix
  Matrix matModelView = rlGetMatrixModelview();
  Matrix matProjection = rlGetMatrixProjection();
  Matrix matMVP = MatrixMultiply(matModelView, matProjection);
  
  SetShaderValueMatrix(s_data.holoShader, s_data.mvpLoc, matMVP);
  SetShaderValue(s_data.holoShader, s_data.timeLoc, &time, SHADER_UNIFORM_FLOAT);
  SetShaderValueTexture(s_data.holoShader, s_data.noiseLoc, s_data.noiseTex);

  rlEnableVertexArray(s_data.quadVAO);

  for (const auto& batch : batches) {
      if (s_data.offsetLoc != -1) {
          SetShaderValue(s_data.holoShader, s_data.offsetLoc, &batch.startOffset, SHADER_UNIFORM_INT);
      }

      rlActiveTextureSlot(0);
      rlEnableTexture(batch.texture.id);
      
      rlDrawVertexArrayInstanced(0, 6, batch.count);
  }
  
  rlDisableVertexArray();
  EndShaderMode();
}

} // namespace NoMoreDay::systems