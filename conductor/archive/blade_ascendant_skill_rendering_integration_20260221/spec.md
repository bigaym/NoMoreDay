# Blade Ascendant Skill Rendering Integration - Specification

> **Track ID**: `blade_ascendant_skill_rendering_integration_20260221`  
> **Status**: Pending  
> **Depends On**: `blade_ascendant_vfx_design_freeze_20260221`

---

## 0. Frozen Input Bundle (Must Use)

This track must consume the frozen package from T1.5:

- `设计文档/特效和UI/BladeAscendant_SkillVFX_Design_v2.md`
- `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/evidence/skill_vfx_matrix.md`
- `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/evidence/render_contract_matrix.md`
- `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/evidence/tier_fallback_matrix.md`

## 1. Goal

Integrate the frozen Blade Ascendant VFX design into runtime rendering with:

- executable `SkillVfxEvent` flow
- RenderGraph ownership/resource compliance
- deterministic tier fallback behavior

## 2. Event Model

```cpp
enum class SkillVfxEventType : uint8_t {
  CastStart,
  CastImpact,
  TriggerProc,
  EmpoweredConsume,
  BuffEnter,
  BuffExit
};

struct SkillVfxEvent {
  uint32_t skillId;
  uint64_t castId;
  SkillVfxEventType type;
  Vector2 origin;
  Vector2 target;
  Tag effectiveTags;
  uint32_t nodeRoleMask;
  uint8_t qualityTier;
  float intensity;
};
```

## 3. Render Contract

- `VFXPass`
  - Read: `SceneDepth` (optional)
  - Write: `SceneHdrColor`
  - Owner: `VFX`
- `DistortionPass`
  - Read: `PostProcessLdrColor`
  - Write: `DistortionLdrColor`
  - Owner: `Distortion`
- `CompositePass`
  - Final pass only that may write `FBO 0`

## 4. GPU Resource Policy

- Do not add new global SSBO bindings.
- Reuse existing skill/particle payload layouts first.
- If ABI change is unavoidable, follow governed ABI generation flow.

## 5. Tier and Fallback

Apply exactly the fallback order frozen by T1.5:

1. reduce particle emission
2. disable distortion
3. reduce trail sampling
4. disable secondary glow

## 6. Acceptance Criteria

- [ ] 9 skills implemented according to frozen matrix
- [ ] RenderGraph ownership/resource checks pass
- [ ] Low/Medium fallback keeps gameplay readability
- [ ] VFX budget aligns with frozen thresholds
- [ ] `build.bat` and relevant tests pass
