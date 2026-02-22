# Render Contract Matrix (Frozen)

Source of truth: `设计文档/特效和UI/BladeAscendant_SkillVFX_Design_v2.md` section 5.

| Pass | Read Resources | Write Resources | Owner | FBO0 Write | Notes |
|---|---|---|---|---|---|
| `VFXPass` | `SceneDepth` (optional) | `SceneHdrColor` | `VFX` | Forbidden | Skill VFX draw target stays offscreen/HDR path |
| `DistortionPass` | `PostProcessLdrColor` | `DistortionLdrColor` | `Distortion` | Forbidden | Distortion consumes post-process input, never writes default framebuffer |
| `CompositePass` | `DistortionLdrColor` or `PostProcessLdrColor` (fallback) | `BackBuffer` | `Composite` | Allowed | Final screen composition only |

## Hard Constraints

- Only `CompositePass` may write to default framebuffer (`FBO 0`).
- Skill VFX routes must not bypass RenderGraph pass order.
- Any ABI extension attempt must first reuse existing SSBO layouts and fields.
