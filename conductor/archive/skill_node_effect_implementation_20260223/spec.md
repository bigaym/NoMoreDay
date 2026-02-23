# Skill Node Effects Implementation Spec

> Track ID: `skill_node_effect_implementation_20260223`  
> Type: feature/behavior-implementation  
> Priority: P0  
> Depends on: `skill_tree_doc_alignment_20260223`, `skill_spec_safety_ui_hardening_20260223`

---

## 1. Problem Statement

Design and config are structurally aligned (node count/contract role), but many newly aligned nodes are still config-only and do not provide complete gameplay behavior, feedback loops, or visual signaling at runtime.

This creates a semantic gap:

- players can allocate nodes whose expected effects are partial or absent;
- trigger/synergy/transmuter chains are not consistently expressed in behavior systems;
- visual readability for some key nodes is not guaranteed.

---

## 2. Goal

Implement runtime effects for newly aligned Blade Ascendant node sets (`skills 1..9`) so that each key node class is behavior-complete:

- Trigger effects: dispatch, cooldown, cost, and anti-loop consistency;
- Synergy effects: cross-skill interaction and scoped amplification;
- Transmuter effects: conversion, mutex, and tag/stat propagation;
- Keystone/passive effects: expected stat/behavior deltas reflected in combat loop.

---

## 3. Scope

In scope:

- `src/game/systems/skill/behaviors/*.cpp` for skills `1..9`
- `src/game/systems/skill/SkillSystem.cpp/.hpp` contract-driven orchestration where needed
- `assets/data/skills.json` only for behavior metadata corrections required by implementation
- tests:
  - `tests/unit/SkillBehaviorGuardTests.cpp`
  - `tests/integration/SkillSystemTests.cpp`
  - `tests/integration/SkillContractRegistryTests.cpp`

Out of scope:

- global combat rebalance unrelated to newly aligned nodes;
- redesign of ECS data model;
- unrelated render pipeline architectural changes.

---

## 4. Design Constraints

- ECS/EnTT architecture must remain intact.
- No string-driven hot-path branching in per-frame/per-hit critical paths.
- Keep trigger depth/cooldown/mutex guards effective (no new trigger loop surface).
- Preserve frame-order and RenderGraph safety constraints.
- Visual additions must remain tier-compatible and avoid budget spikes.

---

## 5. Performance & Visual Acceptance

Performance:

- no additional unbounded allocations in hit/cast hot paths;
- no measurable regressions in existing `unit/integration/ci` gates;
- maintain deterministic fallback behavior for missing runtime state.

Visual:

- key trigger/synergy/transmuter states must have readable feedback;
- no scissor/FBO leakage or UI regression from new node UI/tooltip effects;
- Low Tier fallback path remains available.

---

## 6. Acceptance Criteria

- [ ] Newly aligned key nodes have concrete runtime behavior coverage across skills `1..9`.
- [ ] Trigger/synergy/transmuter semantics are test-covered and pass.
- [ ] No regression in existing skill safety/hardening tests.
- [ ] Build and required CTest labels pass (`unit`, `integration`, `ci`).
- [ ] Validation evidence includes per-skill implemented-node matrix and remaining debt (if any).

---

## 7. Risks & Mitigations

- Risk: behavior implementation drifts from config contract.
  - Mitigation: contract-first checks + per-skill matrix in validation.
- Risk: cross-skill synergy introduces hidden loops.
  - Mitigation: retain trigger depth and cooldown guards; add focused tests.
- Risk: visual effects inflate GPU/CPU budget.
  - Mitigation: gate by quality tier and reuse existing VFX pathways.

