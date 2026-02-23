# Skill Specialization Key Node Test Matrix Spec

> Track ID: `skill_specialization_keynode_test_matrix_20260223`  
> Type: quality/test-expansion  
> Priority: P0  
> Scope: Expand specialization test coverage with emphasis on key-node effect validation.

---

## 1. Overview

This track introduces a large, contract-aligned test matrix for Blade Ascendant specialization nodes.  
Target is to verify key-node effects across skills `1..9` at unit and integration levels, with deterministic checks for runtime state, trigger semantics, transmuter behavior, and critical combat/visual signal hooks.

Primary goals:

1. Close key-node test gaps by covering all contract key nodes.
2. Ensure key-node behavior is verified both on cast and on runtime updates/events.
3. Prevent regressions in trigger cooldown/depth guards and transmuter mutex rules.
4. Keep test runtime stable and maintainable through fixture-driven expectations.

---

## 2. Key Node Scope (Contract-Aligned)

- Skill 1: `113, 114, 130, 152, 170, 171`
- Skill 2: `230, 233, 250, 252, 270`
- Skill 3: `330, 352, 370, 371, 373`
- Skill 4: `430, 451, 452, 470, 471`
- Skill 5: `530, 533, 552, 570, 571`
- Skill 6: `630, 633, 652, 670, 671`
- Skill 7: `713, 730, 750, 752, 770`
- Skill 8: `813, 830, 831, 852, 870, 871`
- Skill 9: `913, 930, 950, 951, 952, 970`

Total key nodes in scope: `48`.

---

## 3. Constraints

- ECS remains EnTT-based; no gameplay ownership moved into components.
- No changes to contract ABI/versioning in this track.
- Tests must be deterministic and avoid wall-clock dependencies.
- Render safety constraints must remain unchanged:
  - no non-composite pass writes to `FBO 0`
  - tier fallback paths remain valid.
- Windows + MSVC toolchain only.

---

## 4. Test Data Model

Test matrix is defined by fixture-like expectations (C++ + optional JSON fixture).

```cpp
enum class KeyNodeCheckLayer : uint8_t {
  CastState,        // Component/state shape after DoCast
  RuntimeTick,      // Behavior during SkillSystem::Update / ProjectileSystem::Update
  CombatEvent,      // TakeDamage/SkillHit driven effects
  TagConversion,    // Effective tags and transmuter conversion semantics
  GuardPolicy,      // Trigger cooldown/depth/scope policy guards
  VisualSignalGuard // Deterministic non-pixel visual signaling hooks
};

struct KeyNodeExpectation {
  uint32_t skillId;
  uint32_t nodeId;
  const char* label;
  KeyNodeCheckLayer layer;
  bool requiresRuntimeTransmuterSelection;
};
```

Authoritative runtime components/systems under test:

- Components:
  - `ActiveSkillsComponent`
  - `SkillContractRuntimeComponent`
  - `SkillExecution`
  - `BladeWardComponent`
  - `ChannelingComponent`
  - `SwordArrayComponent`
  - `PhantomFlashComponent`
  - `Projectile`
  - `SkillComponent`
- Systems:
  - `SkillSystem`
  - `ProjectileSystem`
  - `SkillBehaviorRegistry`
  - `CombatEventDispatcher`

---

## 5. Fixture Persistence (Test Asset)

Add fixture file (test-only):

- `tests/fixtures/skill_specialization_keynodes.json`

Example schema:

```json
{
  "version": 1,
  "skills": [
    {
      "skill_id": 5,
      "key_nodes": [530, 533, 552, 570, 571],
      "expected_layers": ["CastState", "RuntimeTick", "TagConversion"]
    }
  ]
}
```

Fixture is used to drive parametrized test generation and reduce duplicated hard-coded node lists.

---

## 6. Test Strategy

### 6.1 Unit Matrix

- Contract-key-node to runtime-state mapping for all `48` key nodes.
- Trigger guard verification (`cooldown`, `depth`) for trigger nodes.
- Transmuter mutex + effective-tag checks for all transmuter node pairs.
- Scope policy checks for specialization-dependent global/skill-only behavior.

### 6.2 Integration Matrix

- Per-skill integration scenarios (`1..9`) validating at least one runtime effect path per key node group.
- Cross-skill interaction scenarios:
  - trigger chains
  - channeling + conversion
  - interception/counter loops
  - resource/cooldown return interactions.
- Deterministic visual-signal guards via runtime counters/state markers (not screenshot-only checks).

### 6.3 Regression and Stability

- Existing specialization tests must remain green.
- No new flaky tests (run twice locally in the same configuration for smoke stability).

---

## 7. Implementation Targets

- `tests/unit/SkillBehaviorGuardTests.cpp` (expand)
- `tests/integration/SkillSystemTests.cpp` (expand)
- `tests/unit/SkillKeyNodeMatrixTests.cpp` (new)
- `tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp` (new)
- `tests/fixtures/skill_specialization_keynodes.json` (new)
- `CMakeLists.txt` / test registration locations (if required)

---

## 8. Acceptance Criteria

- [ ] All 48 key nodes are covered by at least one explicit positive effect assertion.
- [ ] Each skill (`1..9`) has at least one integration-level key-node scenario.
- [ ] All trigger nodes include guard-path checks (`ICD` + depth).
- [ ] All transmuter pairs include mutex + effective-tag checks.
- [ ] At least 12 cross-skill scenarios are added (trigger/synergy/transmuter interactions).
- [ ] Test suite remains deterministic (no intermittent fail across two consecutive local runs).
- [ ] `build.bat`, `ctest -L unit`, `ctest -L integration`, `ctest -L ci` pass.

---

## 9. Risks & Mitigations

- Risk: Test bloat increases maintenance and runtime.
  - Mitigation: fixture-driven parametrization and shared helper builders.
- Risk: Over-coupling tests to implementation details.
  - Mitigation: prioritize behavior outcomes over private field internals.
- Risk: Visual checks become unstable.
  - Mitigation: assert deterministic signal-state/counter hooks, not frame-pixel output.
