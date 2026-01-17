#include "game/systems/vfx/HoloBladeRenderSystem.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/Common.hpp"
#include "game/components/vfx/HoloBladeComponent.hpp"
#include "raymath.h"
#include "rlgl.h"
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "core/logging/Logger.hpp"
#include <map>

namespace NoMoreDay::systems {

struct HoloBladeInternal {
    Shader holoShader = {0};
    Texture2D noiseTex = {0};
    core::ComputeBuffer instanceBuffer;
    unsigned int quadVAO = 0;
    unsigned int quadVBO = 0;
    bool initialized = false;
    
    std::vector<components::HoloBladeInstance> hostBuffer;
    
    void Init(const NoMoreDay::SharedContext &context) {
        if (initialized) return;
        
        LOG_INFO("Initializing HoloBladeRenderSystem (Instanced)...");
        
        holoShader = LoadShader("assets/shaders/vfx/holo_blade_instanced.vs", 
                                "assets/shaders/vfx/holo_blade_instanced.fs");
        
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
        
        instanceBuffer.Create(1000 * sizeof(components::HoloBladeInstance), nullptr, RL_DYNAMIC_DRAW);
        hostBuffer.reserve(1000);
        
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

void HoloBladeRenderSystem::Render(entt::registry &registry,
                                   const NoMoreDay::SharedContext &context) {
  s_data.Init(context);
  if (!s_data.initialized) return;

  auto view = registry.view<const components::HoloBlade, const Position,
                            const SpriteComponent>();
  
  if (view.size_hint() == 0) return;

  // We group by texture primarily, but for HoloBlades they usually share the same sword texture.
  
  std::map<unsigned int, std::vector<entt::entity>> batches;
  for (auto entity : view) {
      const auto &holo = view.get<const components::HoloBlade>(entity);
      if (!holo.isVisible) continue;
      const auto &sprite = view.get<const SpriteComponent>(entity);
      batches[sprite.texture.id].push_back(entity);
  }

  float time = (float)GetTime();
  rlDrawRenderBatchActive();
  
  BeginShaderMode(s_data.holoShader);
  
  // Update MVP matrix
  Matrix matModelView = rlGetMatrixModelview();
  Matrix matProjection = rlGetMatrixProjection();
  Matrix matMVP = MatrixMultiply(matModelView, matProjection);
  int mvpLoc = GetShaderLocation(s_data.holoShader, "mvp");
  SetShaderValueMatrix(s_data.holoShader, mvpLoc, matMVP);

  int timeLoc = GetShaderLocation(s_data.holoShader, "time");
  int noiseLoc = GetShaderLocation(s_data.holoShader, "noiseTex");
  SetShaderValue(s_data.holoShader, timeLoc, &time, SHADER_UNIFORM_FLOAT);
  SetShaderValueTexture(s_data.holoShader, noiseLoc, s_data.noiseTex);

  int totalRendered = 0;
  for (auto const& [texId, entities] : batches) {
      if (entities.empty()) continue;
      
      const auto &firstSprite = view.get<const SpriteComponent>(entities[0]);
      Texture2D tex = firstSprite.texture;

      s_data.hostBuffer.clear();
      
      for (auto entity : entities) {
          const auto &holo = view.get<const components::HoloBlade>(entity);
          const auto &pos = view.get<const Position>(entity);
          const auto &sprite = view.get<const SpriteComponent>(entity);
          
          components::HoloBladeInstance inst;
          inst.position = {pos.x, pos.y};
          inst.rotation = 0.0f;
          if (auto *rot = registry.try_get<Rotation>(entity)) {
              inst.rotation = rot->angle * (PI / 180.0f);
          }
          
          // Spirit swords might want rotation towards target or orbit dir
          // But our shader rotates the quad.
          
          inst.scale = sprite.scale * holo.scale * (float)tex.width; 
          inst.holoColor = ColorNormalize(holo.holoColor);
          inst.rimStrength = holo.rimStrength;
          inst.noiseSpeed = holo.noiseSpeed;
          
          s_data.hostBuffer.push_back(inst);
          if (s_data.hostBuffer.size() >= 1000) break;
      }
      
      if (s_data.hostBuffer.empty()) continue;
      
      totalRendered += (int)s_data.hostBuffer.size();
      s_data.instanceBuffer.Update(s_data.hostBuffer.data(), s_data.hostBuffer.size() * sizeof(components::HoloBladeInstance));
      s_data.instanceBuffer.BindBase(4);
      
      rlActiveTextureSlot(0);
      rlEnableTexture(tex.id);
      
      rlEnableVertexArray(s_data.quadVAO);
      rlDrawVertexArrayInstanced(0, 6, (int)s_data.hostBuffer.size());
      rlDisableVertexArray();
  }
  
  EndShaderMode();
}

} // namespace NoMoreDay::systems
