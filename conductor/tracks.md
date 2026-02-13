# Project Tracks

This file tracks all major active tracks for the project.
Completed tracks are archived in [./archive/tracks_archive.md](./archive/tracks_archive.md).

---

## [ ] Track: 粒子与轨迹增强 (particle_trail_enhancement_20260213)

> **目标**: 升级 GPU 粒子系统支持纹理图集与动画，并实现基于 GPU 的动态轨迹渲染器 (TrailRenderer)。
> **文档**: [spec.md](./tracks/particle_trail_enhancement_20260213/spec.md) | [plan.md](./tracks/particle_trail_enhancement_20260213/plan.md)
> **状态**: IN_PROGRESS (Code Complete, pending visual verification & archive)
> **Phase**: GPU 渲染系统 2.0 — Phase 3
> **预估工时**: 5~7 天

---

## [ ] Track: 打磨与高级特性 (polishing_advanced_features_20260213)

> **目标**: 完成 GPU 渲染系统 Phase 5，交付 Color Grading LUT、Ultra 体积光、Pass Profiler HUD 与 Shader 热重载能力。
> **文档**: [spec.md](./tracks/polishing_advanced_features_20260213/spec.md) | [plan.md](./tracks/polishing_advanced_features_20260213/plan.md)
> **状态**: IN_PROGRESS (Code complete, acceptance in progress)
> **Phase**: GPU 渲染系统 2.0 — Phase 5
> **预估工时**: 4~6 天

---

## Status Update (2026-02-14)

- material_vfx_sequencer_20260213: COMPLETED & ARCHIVED
  - delivered: material system, vfx sequencer, distortion pass, prefab library
  - fixed: critical particle system deadlock (BUG-20260213-001)
  - verified: full test suite & visual check
- polishing_advanced_features_20260213: IN ACCEPTANCE
  - delivered: Phase A-E implementation (Color Grading, Volumetric, Profiler HUD, Shader Hot Reload)
  - delivered: Phase F.2 benchmarks added in `tests/performance/RenderingBenchmark.cpp` and passing thresholds
  - pending: runtime visual evidence and hot-reload success/failure log capture
