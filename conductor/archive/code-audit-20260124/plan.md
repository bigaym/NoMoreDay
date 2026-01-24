# Implementation Plan: Comprehensive Code Audit

## Step 1: Static Analysis & Logic Audit (Physics)
- [ ] Audit PhysicsSystem.cpp/hpp for potential overflow/precision issues.
- [ ] Review GameplayState::UpdatePhysics Taskflow implementation for thread-safety.
- [ ] Verify entity lifecycle during parallel iteration (avoid structural changes).

## Step 2: Static Analysis & Logic Audit (Rendering)
- [ ] Audit GPUEntitySystem Slot Management (FreeList implementation).
- [ ] Verify OnGPUIndexDestroyed signal connection coverage across all entity types.
- [ ] Check SSBO alignment and struct GPUEntity padding for GPU compatibility.

## Step 3: Performance & Risk Profiling
- [ ] Run benchmark with 20,000 active entities.
- [ ] Monitor CPU/GPU synchronization wait times.
- [ ] Audit MDIRenderer for redundant state changes or buffer uploads.

## Step 4: Final Hardening
- [ ] Extract any remaining magic numbers to constants.
- [ ] Ensure all debug logging is appropriately throttled or removed.
