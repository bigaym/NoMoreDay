# Skill VFX Matrix (Frozen)

Source of truth: `设计文档/特效和UI/BladeAscendant_SkillVFX_Design_v2.md`.

| Skill | Main VFX | Trigger / Empowered Feedback | Concurrency Cap | Low Fallback |
|---|---|---|---:|---|
| 3.1 Flowing Thrust | Dash trail + start/end impulse ring | TriggerProc adds thin sword-flash cut | `trail instances <= 24` | Keep end impulse ring only, remove trail ribbons |
| 3.2 Rending Wave | Crescent sword-wave mesh + emissive edge | Echo slash with lower luminance | `projectile vfx <= 32` | Disable distortion, keep edge glow only |
| 3.3 Blade Formation | Instanced spirit-sword orbit + launch tail | Giant-sword proc adds short ring pulse | `spirit swords visual <= 16` | Halve visual sword count, disable tails |
| 3.4 Blade Ward | Tri-direction rotating sword ring + block spark | On-block hit spark | `ward ring emitters <= 6` | Simplified ring + hit flash only |
| 3.5 Infinite Blades | Area warning decal + sword-rain particles | Empowered consume accent pulse | `rain particles <= 4096` | Keep warning decal, clamp particles to `<= 1024` |
| 3.6 Sword Array | Ground array texture + perimeter sword pillars | In-array random slash flash | `active arrays <= 4` | Disable random slash flashes, keep array ring |
| 3.7 Mind Blade | High-frequency narrow beam + impact convergence spark | TriggerProc beam pulse | `beam instances <= 64` | Fixed beam width, no flow texture |
| 3.8 Blade Boomerang | Spinning boomerang + return spiral trail | Return-point impulse ring | `boomerang trail <= 20` | Keep core boomerang body, lower trail sampling |
| 3.9 Phantom Trance | Entry dissolve + exit burst + phantom edge pulse | TriggerProc in counter/phantom window | `phantom overlays <= 4` | Disable full-screen darkening, keep character rim light |

## Checkpoints

- Every skill has explicit `Main VFX` and `Trigger/Empowered` feedback.
- Every skill defines numeric concurrency cap.
- Every skill defines Low-tier readable fallback.
