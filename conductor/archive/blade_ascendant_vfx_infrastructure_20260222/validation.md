# Validation - blade_ascendant_vfx_infrastructure_20260222

## Scope

- Track: `blade_ascendant_vfx_infrastructure_20260222`
- Goal: provide executable VFX infrastructure for Blade Ascendant VFX V3 (event contract + recipe skeleton + tier hooks).

## Verification Commands

| Command | Result |
|---|---|
| `python scripts/check_blade_vfx_recipe.py --check` | PASS |
| `.\build.bat notest` | PASS |
| `.\build.bat` | PASS |
| `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` | PASS |
| `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` | PASS |
| `.\build.bat analyze` | PASS (existing static-analysis warnings only, none introduced as blocker by this track) |
| `ctest --test-dir build -C Release -L performance --output-on-failure` | PASS |

## Evidence Checklist

- [x] `SkillVfxEvent` extended with compatible defaults.
- [x] Recipe-driven skeleton exists (smoke-tested on at least 1 skill).
- [x] No new global SSBO binding introduced; RenderGraph ownership respected.
- [x] Tier fallback knobs mapped (documented).

## Task Evidence Mapping

| Plan Task | Evidence |
|---|---|
| 1.1 / 1.2 / 1.3 | Context audit against `BladeAscendant_VFX_Design_v3.md` §6-§9 and `GPU_Rendering_Quick_Reference.md` constraints; implementation keeps output in RenderGraph path (`VFXPass` + queued `DistortionPass`), no FBO0 direct write |
| 1.4 | Existing assets/scripts audited: `scripts/gen_blade_vfx_assets.py`, `scripts/gen_vfx_textures.py`, `assets/shaders/vfx/*` |
| 1.5 | Schema draft added: `assets/data/vfx/blade_ascendant_v3.schema.json` |
| 2.1 / 2.2 | `src/game/components/SkillVfxEvent.hpp` adds `TransmuterSwitch/KeystoneActivate`, `elementType`, `resistDebuffType`, role mask constants |
| 2.3 | `src/game/systems/skill/SkillSystem.hpp` + `src/game/systems/skill/SkillSystem.cpp` fill `node_role_mask/element_type/resist_debuff_type`; emit `TransmuterSwitch` and `KeystoneActivate` on cast start when role masks match |
| 2.4 | `src/engine/render/GPUSkillEffectSystem.cpp` consumes new fields with compatibility fallback (`ResolveLegacyElementType`) |
| 2.5 | `tests/unit/SkillVfxEventContractTest.cpp` adds enum/default/mask contract assertions |
| 2.6 | `.\build.bat notest` PASS |
| 3.1 | Recipe model + selector priority introduced in `src/engine/render/GPUSkillEffectSystem.hpp/.cpp` (`SkillVfxRecipe*`, wildcard + priority + role mask matching) |
| 3.2 | Runtime recipe load from `assets/data/vfx/blade_ascendant_v3.json` with builtin fallback |
| 3.3 | Event flow refactored to `EmitRecipeDrivenVisual -> EmitLegacySkillEventVisual` |
| 3.4 | Action adapters implemented: Overlay(SSBO6), ParticleBurst(`GPUParticleSystem`), TrailStroke(`GPUTrailRenderer`), DistortionPulse(queue to `DistortionPass`), ResistOverlay queue |
| 3.5 | Added `assets/shaders/vfx/vfx_element_switch.glslinc` |
| 3.6 | Overlay entry reserved and wired in `src/engine/render/RenderSystem.cpp` via `DrainResistOverlayRequests` draw path (no new pass) |
| 3.7 | Skill 1 smoke recipe covers `CastStart/CastImpact/TriggerProc/EmpoweredConsume` in `assets/data/vfx/blade_ascendant_v3.json`; unit recipe coverage assertion added |
| 3.8 | `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS |
| 4.1 / 4.2 | Fallback policy mapping implemented in `BuildVfxFallbackPolicy`: particle emission scale -> distortion disable -> trail stride degrade -> secondary glow disable -> environment disable -> afterimage simplification hook; driven by `RenderConfig.vfxSequenceDetail`, `RenderConfig.distortionEnabled`, and `QualityTierManager::GetAutoDegradeLevel()` |
| 4.3 | Tier acceptance checklist recorded below for gate-track reuse |

## Tier Acceptance Checklist

- [x] Low/Medium 保留技能判读主信号（Cast/Impact/Trigger/Consume 仍发事件并可出图）。
- [x] 回退顺序映射明确：粒子倍率下降 -> Distortion 关闭 -> Trail stride 增大 -> 次级 glow 关闭 -> 环境粒子关闭 -> 残影简化钩子。
- [x] Distortion 由 `RenderConfig.distortionEnabled` 与 AutoDegrade 共同门控。
- [x] 配方匹配支持字段优先（`elementType/resistDebuffType`）+ `nodeRoleMask` 约束。
- [x] 无新增全局 SSBO binding（仅复用已有路径）。

## Notes

- This track is foundational; runtime visual completeness is verified in downstream feature tracks and the final validation gate.
