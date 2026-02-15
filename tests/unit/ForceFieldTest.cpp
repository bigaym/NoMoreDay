#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/particle/ForceFieldManager.hpp"

#include <cstddef>

using namespace NoMoreDay;

TEST_CASE("[Unit] ForceField - GPU ABI Layout") {
  CHECK(sizeof(components::GPUForceField) == 32);
  CHECK(offsetof(components::GPUForceField, posX) == 0);
  CHECK(offsetof(components::GPUForceField, radius) == 8);
  CHECK(offsetof(components::GPUForceField, strength) == 12);
  CHECK(offsetof(components::GPUForceField, type) == 16);
  CHECK(offsetof(components::GPUForceField, noiseFrequency) == 24);
}

TEST_CASE("[Unit] ForceField - Add Remove And Capacity") {
  auto &mgr = render::ForceFieldManager::Get();
  mgr.Shutdown();
  mgr.Init(2);

  components::GPUForceField radial = {};
  radial.posX = 10.0f;
  radial.posY = 20.0f;
  radial.radius = 120.0f;
  radial.strength = 80.0f;
  radial.type = static_cast<uint32_t>(components::ForceFieldType::Radial);

  components::GPUForceField vortex = radial;
  vortex.type = static_cast<uint32_t>(components::ForceFieldType::Vortex);

  const int id0 = mgr.AddForceField(radial);
  const int id1 = mgr.AddForceField(vortex);
  const int id2 = mgr.AddForceField(radial);

  CHECK(id0 >= 0);
  CHECK(id1 >= 0);
  CHECK(id2 == -1);
  CHECK(mgr.GetActiveCount() == 2);

  mgr.RemoveForceField(id0);
  CHECK(mgr.GetActiveCount() == 1);

  const int recycled = mgr.AddForceField(radial);
  CHECK(recycled == id0);
  CHECK(mgr.GetActiveCount() == 2);

  mgr.SyncToGPU();
  mgr.BindSSBO(NoMoreDay::RenderConstants::ParticleCS::FORCE_FIELDS);
  mgr.ClearAll();
  CHECK(mgr.GetActiveCount() == 0);
  mgr.Shutdown();
}
