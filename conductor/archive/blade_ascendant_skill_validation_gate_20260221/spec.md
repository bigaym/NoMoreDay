# Blade Ascendant Skill Validation Gate - Specification

> **Track ID**: `blade_ascendant_skill_validation_gate_20260221`  
> **Status**: Pending  
> **Depends On**: `blade_ascendant_skill_rendering_integration_20260221`

---

## 0. Gate Input Baseline (Frozen)

Validation must be executed against the frozen T1.5 package:

- `设计文档/特效和UI/BladeAscendant_SkillVFX_Design_v2.md`
- `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/evidence/skill_vfx_matrix.md`
- `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/evidence/render_contract_matrix.md`
- `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/evidence/tier_fallback_matrix.md`

## 1. Goal

Provide a five-dimension gate for Blade Ascendant rollout:

- functional correctness
- contract compliance
- performance
- stability
- fallback behavior

## 2. Gate Dimensions

- Functional gate: 9 skills and key branches execute correctly.
- Contract gate: Trigger/transmuter/scope and render ownership are valid.
- Performance gate: no critical regressions against active baseline.
- Stability gate: high-frequency cast/tier switch/resize remains safe.
- Fallback gate: Low-tier paths remain readable and deterministic.

## 3. Test Matrix

- Unit: contract guards and runtime behavior guards.
- Integration: behavior registry and render-ownership paths.
- Performance: `ctest --test-dir build -C Release -L performance --output-on-failure`.

## 4. Bug Linkage Rule

- Any new issue must be registered in `conductor/bug_registry.md`.
- Performance failures unrelated to the current track must be marked
  `non-blocking for this track` with explicit bug linkage and metrics.

## 5. Acceptance Criteria

- [ ] `build.bat` passes
- [ ] RelWithDebInfo unit label passes
- [ ] RelWithDebInfo integration label passes
- [ ] Contract checks have no blocking errors
- [ ] Fallback validation has explicit evidence
