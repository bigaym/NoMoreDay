# Blade Ascendant Skill Contracts Refactor - Implementation Plan

> **Track ID**: `blade_ascendant_skill_contracts_20260221`  
> **Dependency**: none (foundation track)

---

## Phase Overview

| Phase | Name | Core Deliverable | Status |
|---|---|---|---|
| Phase 1 | Contract Types & Registry | Contract model + registry query surface | [x] |
| Phase 2 | Config Migration | `skills.json` contract blocks generated from compact config | [x] |
| Phase 3 | Runtime Integration | SkillSystem/Combat scope + runtime state wiring | [x] |
| Phase 4 | UI/Save Alignment | UI badges/tooltips + save/load roundtrip + validation | [x] |

---

## Phase 1: Contract Types & Registry

- [x] Task 1.1: Add `SkillContract`/`NodeContractData` model
- [x] Task 1.2: Add contract registry querying by `skill_id/node_id`
- [x] Task 1.3: Add base tests for mapping/loading/validation behavior
- [x] Task 1.4: Add static asserts and contract version constants

## Phase 2: Config Migration

- [x] Task 2.1: Add contract fields for skills 1-9
- [x] Task 2.2: Add contract validity checks (node range/mutual exclusivity/trigger cap)
- [x] Task 2.3: Validate resist model A-E coverage consistency
- [x] Task 2.4: Add config regression test coverage

## Phase 3: Runtime Integration

- [x] Task 3.1: Enforce Trigger/Transmuter constraints before cast
- [x] Task 3.2: Add `ScopePolicy` filtering in combat/stat paths
- [x] Task 3.3: Add trigger internal cooldown runtime state
- [x] Task 3.4: Add runtime query for sword-intent/sword-step contract flags
- [x] Task 3.5: Keep hot-path allocation behavior stable (thread-local scratch reuse)

## Phase 4: UI/Save Alignment

- [x] Task 4.1: UI node badges and scope declaration rendering
- [x] Task 4.2: SaveManager serialization for `skill_contract_runtime`
- [x] Task 4.3: Load restore + roundtrip regression test
- [x] Task 4.4: Document sync for contract fields and generator workflow
- [x] Task 4.5: Run `build.bat` and related verification
