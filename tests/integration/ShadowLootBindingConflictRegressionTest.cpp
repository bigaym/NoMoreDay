#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/BindingRegistry.hpp"
#include "engine/render/passes/OccluderExtractPass.hpp"
#include "engine/render/passes/ShadowBuildPass.hpp"

#include "raylib.h"
#include "rlgl.h"

#include <vector>

namespace {

namespace LootPassBinding = NoMoreDay::RenderConstants::LootPassBinding;
namespace ShadowCS = NoMoreDay::RenderConstants::ShadowCS;
using NoMoreDay::RenderConstants::Binding;
using NoMoreDay::render::core::BindingDomain;
using NoMoreDay::render::core::BindingRegistry;

bool CreateMinimalGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "ShadowLootBindingConflictRegressionTest Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return false;
  }
  return NoMoreDay::utils::GPUUtils::CheckSupport().computeShaderSupported;
}

} // namespace

TEST_CASE("[Integration] Shadow & Loot Binding Governance - Slot Separation & Registry Governance") {
  // 1. Contractual Binding Point Verification
  // Shadow must use phase-local slot 0, Loot must use global slot 15.
  CHECK_EQ(ShadowCS::kOccluderBinding, 0u);
  CHECK_EQ(static_cast<uint32_t>(LootPassBinding::INSTANCE_SSBO), 15u);
  CHECK_EQ(static_cast<uint32_t>(Binding::SSBO_LOOT_INSTANCE), 15u);
  CHECK_NE(ShadowCS::kOccluderBinding, static_cast<uint32_t>(Binding::SSBO_LOOT_INSTANCE));

  // 2. BindingRegistry Domain Resolution
  uint32_t shadowLocalBinding = 999u;
  CHECK(BindingRegistry::TryResolve(BindingDomain::ShadowBuild, "phase_local_ssbo",
                                    shadowLocalBinding));
  CHECK_EQ(shadowLocalBinding, 0u);

  uint32_t shadowCasterBinding = 999u;
  CHECK(BindingRegistry::TryResolve(BindingDomain::ShadowBuild, "SHADOW_CASTER_IN",
                                    shadowCasterBinding));
  CHECK_EQ(shadowCasterBinding, 0u);

  uint32_t lootGlobalBinding = 999u;
  CHECK(BindingRegistry::TryResolve(BindingDomain::Global, "SSBO_LOOT_INSTANCE",
                                    lootGlobalBinding));
  CHECK_EQ(lootGlobalBinding, 15u);

  // 3. Domain Properties & Conflict Governance
  CHECK(BindingRegistry::IsPhaseLocalDomain(BindingDomain::ShadowBuild));
  CHECK(BindingRegistry::IsPhaseLocalDomain(BindingDomain::ShadowPrepare));
  CHECK(BindingRegistry::IsPhaseLocalDomain(BindingDomain::LightCulling));
  CHECK(BindingRegistry::IsPhaseLocalDomain(BindingDomain::ShadowResolve));
  CHECK_FALSE(BindingRegistry::IsPhaseLocalDomain(BindingDomain::Global));

  CHECK(BindingRegistry::IsPhaseLocalSSBO(BindingDomain::ShadowBuild, "phase_local_ssbo"));
  CHECK(BindingRegistry::IsAlias(BindingDomain::ShadowBuild, "phase_local_ssbo"));
  CHECK_FALSE(BindingRegistry::IsAlias(BindingDomain::ShadowBuild, "SHADOW_CASTER_IN"));
  CHECK_FALSE(BindingRegistry::IsPhaseLocalSSBO(BindingDomain::Global, "SSBO_LOOT_INSTANCE"));

  CHECK_FALSE(BindingRegistry::HasDomainConflicts(BindingDomain::Global));
  CHECK_FALSE(BindingRegistry::HasDomainConflicts(BindingDomain::ShadowBuild));
  CHECK_FALSE(BindingRegistry::HasAnyConflicts());
}

TEST_CASE("[Integration] Shadow & Loot Binding Governance - Same-Frame Coexistence without Mutation") {
  if (!CreateMinimalGpuContext()) {
    WARN("OpenGL 4.3 compute shader context unavailable; skipping GL execution test");
    return;
  }

  // Set up Loot buffer on slot 15 with canary data
  std::vector<NoMoreDay::components::GPULootInstance> lootCanaries(16);
  for (size_t i = 0; i < lootCanaries.size(); ++i) {
    lootCanaries[i].worldPosX = static_cast<float>(i * 100 + 10);
    lootCanaries[i].worldPosY = static_cast<float>(i * 100 + 20);
    lootCanaries[i].labelOffsetX = 5.0f;
    lootCanaries[i].labelOffsetY = -24.0f;
    lootCanaries[i].itemId = static_cast<uint32_t>(1000 + i);
    lootCanaries[i].rarityColor = 0xFF00FFFF;
    lootCanaries[i].glowIntensity = 1.5f;
    lootCanaries[i].flags = 1u;
  }

  unsigned int lootBufferId = rlLoadShaderBuffer(
      lootCanaries.size() * sizeof(NoMoreDay::components::GPULootInstance),
      lootCanaries.data(), RL_DYNAMIC_DRAW);
  REQUIRE(lootBufferId != 0);

  // Set up Shadow Occluder buffer on slot 0
  std::vector<NoMoreDay::components::GPUShadowCaster> occluders(8);
  for (size_t i = 0; i < occluders.size(); ++i) {
    occluders[i].posX = static_cast<float>(i * 32);
    occluders[i].posY = static_cast<float>(i * 32);
    occluders[i].radius = 16.0f;
    occluders[i].occluderHeight = 24.0f;
    occluders[i].shapeIndex = 0;
    occluders[i].dynamicFlag = 0;
  }

  unsigned int shadowBufferId = rlLoadShaderBuffer(
      occluders.size() * sizeof(NoMoreDay::components::GPUShadowCaster),
      occluders.data(), RL_DYNAMIC_DRAW);
  REQUIRE(shadowBufferId != 0);

  // Bind Loot SSBO to global slot 15
  NoMoreDay::utils::GPUUtils::BindBufferBase(
      static_cast<uint32_t>(LootPassBinding::INSTANCE_SSBO), lootBufferId);

  // Bind Shadow SSBO to local slot 0
  NoMoreDay::utils::GPUUtils::BindBufferBase(
      ShadowCS::kOccluderBinding, shadowBufferId);

  // Both slots must coexist simultaneously without collision
  CHECK_EQ(ShadowCS::kOccluderBinding, 0u);
  CHECK_EQ(static_cast<uint32_t>(LootPassBinding::INSTANCE_SSBO), 15u);

  // Pass boundary cleanup: Shadow pass exits and unbinds slot 0
  NoMoreDay::utils::GPUUtils::BindBufferBase(ShadowCS::kOccluderBinding, 0);

  // Read back Loot buffer to verify data integrity
  std::vector<NoMoreDay::components::GPULootInstance> readbackLoot(lootCanaries.size());
  rlReadShaderBuffer(lootBufferId, readbackLoot.data(),
                     readbackLoot.size() * sizeof(NoMoreDay::components::GPULootInstance), 0);

  for (size_t i = 0; i < lootCanaries.size(); ++i) {
    CHECK_EQ(readbackLoot[i].worldPosX, lootCanaries[i].worldPosX);
    CHECK_EQ(readbackLoot[i].worldPosY, lootCanaries[i].worldPosY);
    CHECK_EQ(readbackLoot[i].itemId, lootCanaries[i].itemId);
    CHECK_EQ(readbackLoot[i].glowIntensity, lootCanaries[i].glowIntensity);
  }

  // Clean up global loot binding and buffers
  NoMoreDay::utils::GPUUtils::BindBufferBase(
      static_cast<uint32_t>(LootPassBinding::INSTANCE_SSBO), 0);

  rlUnloadShaderBuffer(lootBufferId);
  rlUnloadShaderBuffer(shadowBufferId);
}
