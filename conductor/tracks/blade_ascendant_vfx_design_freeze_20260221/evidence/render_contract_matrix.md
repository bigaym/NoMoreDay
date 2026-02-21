# Render Contract Matrix

| Pass | Read Resources | Write Resources | Owner | FBO0 Write |
|---|---|---|---|---|
| VFXPass | TBD | TBD | TBD | Forbidden |
| DistortionPass | TBD | TBD | TBD | Forbidden |
| CompositePass | DistortionLdrColor | BackBuffer | Composite | Allowed |

## Notes

- Only `CompositePass` may write to default framebuffer (`FBO 0`).
- New/changed pass contracts must be reflected in RenderGraph validation.

