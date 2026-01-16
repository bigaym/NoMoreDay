# Code Risk Mitigation Plan

## Phase 1: Critical Fixes

### Task 1: Fix UAF in SkillSystem
- [x] Refactor `SkillSystem::UpdateStates` to use the Collect-then-Process pattern.
- [x] Ensure `SkillExecution` component is re-fetched or safely handled around hook calls.
- [x] Verify that `s_pre_cast_hooks` and `s_post_cast_hooks` interactions are safe.

### Task 2: Fix GPUFlowFieldSystem Resource Management
- [x] Add missing `Release()` calls for `m_integrationBuffer2` and `m_densityBuffer` in `GPUFlowFieldSystem::Shutdown`.
- [x] Promote temporary vector `costInt` to member variable `m_costCache` to eliminate per-frame allocation.

### Task 3: Verification
- [x] Run `CombatSystemTests` (or relevant skill tests) to ensure no regressions in skill execution.
- [x] Manual check: Run the game, use skills, ensuring no crashes.
