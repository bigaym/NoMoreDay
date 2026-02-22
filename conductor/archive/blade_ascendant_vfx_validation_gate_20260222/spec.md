# Blade Ascendant VFX Validation Gate (V3) - Specification

> **Track ID**: `blade_ascendant_vfx_validation_gate_20260222`  
> **Input Design**: `设计文档/特效和UI/BladeAscendant_VFX_Design_v3.md` §12.1  
> **Status**: Completed (Conditional GO)

---

## 1. Goals

Establish final acceptance for Blade Ascendant VFX V3:

- Functional completeness across Base/Transmutation/Keystone/Trigger/Global systems.
- Contract correctness for RenderGraph ownership, FBO0 write constraints, and SSBO governance.
- Usable fallback on Low/Medium with readable signals.
- Budget posture captured through `VFXPass`/`DistortionPass` and performance suite outputs.

---

## 2. DoD

- [x] V3 design checklist (§12.1) is covered by implemented tracks and gate evidence.
- [x] `build.bat` + `ctest -L ci|unit|integration` passed.
- [x] `ctest -C Release -L performance` executed and results recorded.
- [x] Non-blocking performance failure (if any) is explicitly linked to `conductor/bug_registry.md`.

---
_Updated by Feature Developer._
