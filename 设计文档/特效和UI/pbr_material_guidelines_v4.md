# V4 2D PBR Material Guide

## Scope
- Track: `v4_pbr_material_pipeline_20260219`
- Target: Player / Monster / Environment sprite materials.

## Texture Contract
- Albedo: `RGBA8`, no baked lighting.
- Normal: `RGB8` tangent-space, +Z outward.
- Mask: `RGBA8`
- `R`: roughness
- `G`: metallic
- `B`: AO or height-derived AO
- `A`: emission
- Detail (optional): `RG/RGBA` normal-detail map for Ultra tier.

## Offline Toolchain
- Build height:
```powershell
python scripts/pbr_build_height.py <albedo.png> <height.png>
```
- Build normal:
```powershell
python scripts/pbr_build_normal.py <height.png> <normal.png>
```
- Build AO:
```powershell
python scripts/pbr_build_ao.py <height.png> <ao.png>
```
- Pack mask:
```powershell
python scripts/pbr_pack_mask.py --roughness <r.png> --metallic <m.png> --blue <ao_or_h.png> --emission <e.png> --output <mask.png>
```
- CI one-shot:
```powershell
python scripts/pbr_pipeline_ci.py --albedo <albedo.png> --out-dir <dir> --name <prefix>
```

## Sample Assets
- `assets/textures/pbr_v4/player/*`
- `assets/textures/pbr_v4/monster/*`
- `assets/textures/pbr_v4/environment/*`
- Material config: `assets/data/materials_pbr_v4.json`

## Runtime Notes
- Material schema v3 with v2 auto-mapping defaults is supported.
- GPU ABI version is `4`; material payload uses `GPUMaterialDataV3` (128B).
- Tier policy:
- Low: albedo only.
- Medium: +normal diffuse.
- High/Ultra: BRDF-Lite (`GGX + Schlick-GGX + Schlick Fresnel`), with Ultra detail normal.
