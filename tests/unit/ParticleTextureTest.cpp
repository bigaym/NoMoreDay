#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/particle/ParticleTextureManager.hpp"

#include <cstddef>

using namespace NoMoreDay;

TEST_CASE("[Unit] ParticleTexture - GPUParticle ABI Layout") {
  CHECK(sizeof(components::GPUParticle) == 64);
  CHECK(offsetof(components::GPUParticle, textureIndex) == 52);
  CHECK(offsetof(components::GPUParticle, subUV) == 54);
  CHECK(offsetof(components::GPUParticle, animFrameCount) == 56);
  CHECK(offsetof(components::GPUParticle, blendMode) == 58);
  CHECK(offsetof(components::GPUParticle, subEmitterType) == 59);
  CHECK(offsetof(components::GPUParticle, subEmitterParam) == 60);
}

TEST_CASE("[Unit] ParticleTexture - Backward Compatible Defaults") {
  components::GPUParticle p = {};
  CHECK(p.textureIndex == -1);
  CHECK(p.subUV == 0);
  CHECK(p.animFrameCount == 0);
  CHECK(p.blendMode == 0);
  CHECK(p.subEmitterType == 0);
}

TEST_CASE("[Unit] ParticleTexture - Manager Init Load Shutdown") {
  auto &mgr = render::ParticleTextureManager::Get();
  mgr.Shutdown();

  mgr.Init(8, NoMoreDay::Constants::GPU::TEXTURE_LAYER_SIZE);
  CHECK(mgr.IsInitialized());
  CHECK(mgr.GetLayerCount() == 0);

  const int layer = mgr.LoadLayer("assets/shaders/textures/particles/fire_01.png");
  CHECK(layer == 0);
  CHECK(mgr.GetLayerCount() == 1);

  mgr.Bind(1);
  mgr.Unbind(1);
  mgr.Shutdown();
  CHECK(mgr.IsInitialized() == false);
}
