# Tier Fallback Matrix (Frozen)

Source of truth: `设计文档/特效和UI/BladeAscendant_SkillVFX_Design_v2.md` sections 7 and 8.

| Feature Axis | Low | Medium | High | Ultra | Degrade Order |
|---|---|---|---|---|---:|
| Particle emission | Keep key readability only, strongly reduced density | Moderate density and lifetime | Full density | Full density + headroom | 1 |
| Distortion | Off | Off or partial (scene dependent) | On | On HQ | 2 |
| Trail sampling | Minimal samples | Basic samples | Full samples | Full HQ samples | 3 |
| Secondary glow layers | Off | Partial | On | On HQ | 4 |

## Degrade Sequence

1. Reduce particle emission rate.
2. Disable distortion path.
3. Reduce trail sampling points.
4. Disable secondary glow layer.

## Budget Anchors

| Budget Item | Normal Budget | Stress Budget |
|---|---:|---:|
| Sword Intent VFX | `<= 0.15ms` | `<= 0.25ms` |
| Per-skill average VFX | `<= 0.20ms` | `<= 0.35ms` |
| `VFXPass` total | `<= 0.80ms` | `<= 1.10ms` |
| `DistortionPass` (when enabled) | `<= 0.20ms` | `<= 0.35ms` |
