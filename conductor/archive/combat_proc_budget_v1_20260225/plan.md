# Combat Proc Budget V1 — Implementation Plan

> Track ID: `combat_proc_budget_v1_20260225` | Series: CS-M2-04  
> Depends on: CS-M1-01, CS-M1-03

---

## Phase 1: Budget Data Model

- [x] Define `ProcBudgetConfig` struct with 5 budget dimensions.
- [x] Define `ProcBudgetRuntime` per-entity runtime state.
- [x] Load default budget config from configuration file.

## Phase 2: Budget Manager Implementation

- [x] Implement `ProcBudgetManager::RequestProc` — deduct budget, return allow/deny.
- [x] Implement per-frame reset / per-second sliding window.
- [x] Implement downsampling strategy (deterministic).
- [x] Add budget logging (downsample rate per frame).

## Phase 3: Integration

- [x] Hook into `SkillSystem` trigger dispatch.
- [x] Hook into `DamagePipeline` life/mana on hit.
- [x] Hook into `AilmentEngine` (if available) or EffectSystem.

## Phase 4: Testing & Gate

- [x] Unit: budget enforcement at threshold.
- [x] Unit: downsampling correctness.
- [x] Integration: high-speed + multi-summon stress test.
- [x] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.

## Deliverables

- ProcBudgetManager subsystem.
- Budget configuration infrastructure.
- Stress test suite.
