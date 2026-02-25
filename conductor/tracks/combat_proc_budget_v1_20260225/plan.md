# Combat Proc Budget V1 — Implementation Plan

> Track ID: `combat_proc_budget_v1_20260225` | Series: CS-M2-04  
> Depends on: CS-M1-01, CS-M1-03

---

## Phase 1: Budget Data Model

- [ ] Define `ProcBudgetConfig` struct with 5 budget dimensions.
- [ ] Define `ProcBudgetRuntime` per-entity runtime state.
- [ ] Load default budget config from configuration file.

## Phase 2: Budget Manager Implementation

- [ ] Implement `ProcBudgetManager::RequestProc` — deduct budget, return allow/deny.
- [ ] Implement per-frame reset / per-second sliding window.
- [ ] Implement downsampling strategy (deterministic).
- [ ] Add budget logging (downsample rate per frame).

## Phase 3: Integration

- [ ] Hook into `SkillSystem` trigger dispatch.
- [ ] Hook into `DamagePipeline` life/mana on hit.
- [ ] Hook into `AilmentEngine` (if available) or EffectSystem.

## Phase 4: Testing & Gate

- [ ] Unit: budget enforcement at threshold.
- [ ] Unit: downsampling correctness.
- [ ] Integration: high-speed + multi-summon stress test.
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.

## Deliverables

- ProcBudgetManager subsystem.
- Budget configuration infrastructure.
- Stress test suite.
