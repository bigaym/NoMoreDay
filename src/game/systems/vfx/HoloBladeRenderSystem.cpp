#include "game/systems/vfx/HoloBladeRenderSystem.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/Common.hpp"
#include "game/components/vfx/HoloBladeComponent.hpp"
#include "raymath.h"
#include "rlgl.h"


namespace NoMoreDay::systems {

void HoloBladeRenderSystem::Render(entt::registry &registry,
                                   const NoMoreDay::SharedContext &context) {
  auto view = registry.view<const components::HoloBlade, const Position,
                            const SpriteComponent>();

  static Shader holoShader = {0};
  static Texture2D noiseTex = {0};

  if (holoShader.id == 0 && context.resources) {
    holoShader =
        context.resources->getShader(entt::hashed_string("sh_holo_blade"));
  }
  if (noiseTex.id == 0 && context.resources) {
    noiseTex = context.resources->getTexture(entt::hashed_string("vfx_noise"));
  }

  if (holoShader.id == 0)
    return;

  float time = (float)GetTime();
  int timeLoc = GetShaderLocation(holoShader, "time");
  int noiseTexLoc = GetShaderLocation(holoShader, "noiseTex");
  int holoColLoc = GetShaderLocation(holoShader, "holoColor");
  int rimLoc = GetShaderLocation(holoShader, "rimStrength");
  int speedLoc = GetShaderLocation(holoShader, "noiseSpeed");

  SetShaderValue(holoShader, timeLoc, &time, SHADER_UNIFORM_FLOAT);
  SetShaderValueTexture(holoShader, noiseTexLoc, noiseTex);

  for (auto entity : view) {
    const auto &holo = view.get<const components::HoloBlade>(entity);
    if (!holo.isVisible)
      continue;

    const auto &pos = view.get<const Position>(entity);
    const auto &sprite = view.get<const SpriteComponent>(entity);

    Vector4 col = ColorNormalize(holo.holoColor);
    SetShaderValue(holoShader, holoColLoc, &col, SHADER_UNIFORM_VEC4);
    SetShaderValue(holoShader, rimLoc, &holo.rimStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(holoShader, speedLoc, &holo.noiseSpeed,
                   SHADER_UNIFORM_FLOAT);

    float width = (float)sprite.texture.width * sprite.scale * holo.scale;
    float height = (float)sprite.texture.height * sprite.scale * holo.scale;

    Vector2 origin = {width / 2.0f, height / 2.0f};
    Rectangle source = {0.0f, 0.0f, (float)sprite.texture.width,
                        (float)sprite.texture.height};
    Rectangle dest = {pos.x, pos.y, width, height};

    // Use rotation if available
    float rotation = 0.0f;
    if (auto *rot = registry.try_get<Rotation>(entity)) {
      rotation = rot->angle;
    }

    BeginShaderMode(holoShader);
    DrawTexturePro(sprite.texture, source, dest, origin, rotation, WHITE);
    EndShaderMode();
  }
}

} // namespace NoMoreDay::systems
