#include "doctest.h"

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPULootSystem.hpp"
#include "engine/render/GPUTextSystem.hpp"
#include "engine/render/core/BindingRegistry.hpp"

namespace NoMoreDay::tests {

TEST_CASE("[Unit] GPUTextSystem - Non-blocking Readback Ring Initial State and Submission Contracts") {
  using namespace NoMoreDay::render;

  auto &textSystem = GPUTextSystem::Get();
  CHECK(textSystem.GetRingWriteIndex() == 0u);
  CHECK(textSystem.GetRingReadIndex() == 0u);
  CHECK(GPUTextSystem::kRingDepth == 3u);

  // CanSubmitReadbackCopy contract: empty slot admits copy; armed slot drops copy.
  CHECK(GPUTextSystem::CanSubmitReadbackCopy(false));
  CHECK_FALSE(GPUTextSystem::CanSubmitReadbackCopy(true));

  // TryPublishReadySnapshot contract tests:
  // 1. Unarmed slot preserves snapshot
  auto outcome = GPUTextSystem::TryPublishReadySnapshot(
      /*slotArmed=*/false, /*frameEligible=*/true, /*fenceSignaled=*/true,
      /*readIndex=*/0u, /*ringDepth=*/GPUTextSystem::kRingDepth,
      /*pendingSnapshot=*/128u, /*currentSnapshot=*/42u);
  CHECK_FALSE(outcome.published);
  CHECK(outcome.snapshot == 42u);
  CHECK(outcome.nextReadIndex == 0u);

  // 2. Ineligible frame preserves snapshot
  outcome = GPUTextSystem::TryPublishReadySnapshot(
      /*slotArmed=*/true, /*frameEligible=*/false, /*fenceSignaled=*/true,
      /*readIndex=*/0u, /*ringDepth=*/GPUTextSystem::kRingDepth,
      /*pendingSnapshot=*/128u, /*currentSnapshot=*/42u);
  CHECK_FALSE(outcome.published);
  CHECK(outcome.snapshot == 42u);
  CHECK(outcome.nextReadIndex == 0u);

  // 3. Unsignaled fence preserves snapshot
  outcome = GPUTextSystem::TryPublishReadySnapshot(
      /*slotArmed=*/true, /*frameEligible=*/true, /*fenceSignaled=*/false,
      /*readIndex=*/0u, /*ringDepth=*/GPUTextSystem::kRingDepth,
      /*pendingSnapshot=*/128u, /*currentSnapshot=*/42u);
  CHECK_FALSE(outcome.published);
  CHECK(outcome.snapshot == 42u);
  CHECK(outcome.nextReadIndex == 0u);

  // 4. Signaled fence on eligible armed slot publishes new snapshot and advances index
  outcome = GPUTextSystem::TryPublishReadySnapshot(
      /*slotArmed=*/true, /*frameEligible=*/true, /*fenceSignaled=*/true,
      /*readIndex=*/0u, /*ringDepth=*/GPUTextSystem::kRingDepth,
      /*pendingSnapshot=*/128u, /*currentSnapshot=*/42u);
  CHECK(outcome.published);
  CHECK(outcome.snapshot == 128u);
  CHECK(outcome.nextReadIndex == 1u);

  // 5. Wrap-around at ring depth
  outcome = GPUTextSystem::TryPublishReadySnapshot(
      /*slotArmed=*/true, /*frameEligible=*/true, /*fenceSignaled=*/true,
      /*readIndex=*/2u, /*ringDepth=*/GPUTextSystem::kRingDepth,
      /*pendingSnapshot=*/256u, /*currentSnapshot=*/128u);
  CHECK(outcome.published);
  CHECK(outcome.snapshot == 256u);
  CHECK(outcome.nextReadIndex == 0u);
}

TEST_CASE("[Unit] GPULootSystem - Non-blocking Readback Ring Contracts and Preservations") {
  using namespace NoMoreDay::render;

  auto &lootSystem = GPULootSystem::Get();
  CHECK(lootSystem.GetRingWriteIndex() == 0u);
  CHECK(lootSystem.GetRingReadIndex() == 0u);
  CHECK(GPULootSystem::kRingDepth == 3u);

  CHECK(GPULootSystem::CanSubmitReadbackCopy(false));
  CHECK_FALSE(GPULootSystem::CanSubmitReadbackCopy(true));

  // 1. Unarmed slot preserves snapshot
  auto outcome = GPULootSystem::TryPublishReadySnapshot(
      /*slotArmed=*/false, /*frameEligible=*/true, /*fenceSignaled=*/true,
      /*readIndex=*/0u, /*ringDepth=*/GPULootSystem::kRingDepth,
      /*pendingSnapshot=*/64u, /*currentSnapshot=*/10u);
  CHECK_FALSE(outcome.published);
  CHECK(outcome.snapshot == 10u);
  CHECK(outcome.nextReadIndex == 0u);

  // 2. Ineligible frame preserves snapshot
  outcome = GPULootSystem::TryPublishReadySnapshot(
      /*slotArmed=*/true, /*frameEligible=*/false, /*fenceSignaled=*/true,
      /*readIndex=*/0u, /*ringDepth=*/GPULootSystem::kRingDepth,
      /*pendingSnapshot=*/64u, /*currentSnapshot=*/10u);
  CHECK_FALSE(outcome.published);
  CHECK(outcome.snapshot == 10u);
  CHECK(outcome.nextReadIndex == 0u);

  // 3. Unsignaled fence preserves snapshot
  outcome = GPULootSystem::TryPublishReadySnapshot(
      /*slotArmed=*/true, /*frameEligible=*/true, /*fenceSignaled=*/false,
      /*readIndex=*/0u, /*ringDepth=*/GPULootSystem::kRingDepth,
      /*pendingSnapshot=*/64u, /*currentSnapshot=*/10u);
  CHECK_FALSE(outcome.published);
  CHECK(outcome.snapshot == 10u);
  CHECK(outcome.nextReadIndex == 0u);

  // 4. Signaled fence on eligible armed slot publishes new snapshot and advances index
  outcome = GPULootSystem::TryPublishReadySnapshot(
      /*slotArmed=*/true, /*frameEligible=*/true, /*fenceSignaled=*/true,
      /*readIndex=*/1u, /*ringDepth=*/GPULootSystem::kRingDepth,
      /*pendingSnapshot=*/64u, /*currentSnapshot=*/10u);
  CHECK(outcome.published);
  CHECK(outcome.snapshot == 64u);
  CHECK(outcome.nextReadIndex == 2u);

  // 5. Wrap-around at ring depth
  outcome = GPULootSystem::TryPublishReadySnapshot(
      /*slotArmed=*/true, /*frameEligible=*/true, /*fenceSignaled=*/true,
      /*readIndex=*/2u, /*ringDepth=*/GPULootSystem::kRingDepth,
      /*pendingSnapshot=*/88u, /*currentSnapshot=*/64u);
  CHECK(outcome.published);
  CHECK(outcome.snapshot == 88u);
  CHECK(outcome.nextReadIndex == 0u);
}

TEST_CASE("[Unit] BindingRegistry - TextIndirectArgs Phase-Local Domain Governance") {
  using NoMoreDay::render::core::BindingDomain;
  using NoMoreDay::render::core::BindingRegistry;

  uint32_t counterBinding = 999u;
  CHECK(BindingRegistry::TryResolve(BindingDomain::TextIndirectArgs, "COUNTER_IN",
                                    counterBinding));
  CHECK_EQ(counterBinding, 0u);

  uint32_t commandBinding = 999u;
  CHECK(BindingRegistry::TryResolve(BindingDomain::TextIndirectArgs, "COMMAND_OUT",
                                    commandBinding));
  CHECK_EQ(commandBinding, 1u);

  uint32_t aliasBinding = 999u;
  CHECK(BindingRegistry::TryResolve(BindingDomain::TextIndirectArgs, "phase_local_ssbo",
                                    aliasBinding));
  CHECK_EQ(aliasBinding, 0u);

  CHECK(BindingRegistry::IsPhaseLocalDomain(BindingDomain::TextIndirectArgs));
  CHECK(BindingRegistry::IsPhaseLocalSSBO(BindingDomain::TextIndirectArgs, "phase_local_ssbo"));
  CHECK(BindingRegistry::IsAlias(BindingDomain::TextIndirectArgs, "phase_local_ssbo"));
  CHECK_FALSE(BindingRegistry::IsAlias(BindingDomain::TextIndirectArgs, "COUNTER_IN"));
  CHECK_FALSE(BindingRegistry::IsAlias(BindingDomain::TextIndirectArgs, "COMMAND_OUT"));

  CHECK_FALSE(BindingRegistry::HasDomainConflicts(BindingDomain::TextIndirectArgs));
  CHECK_FALSE(BindingRegistry::HasAnyConflicts());
}

} // namespace NoMoreDay::tests
