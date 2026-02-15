# Project Tracks

This file tracks all major active tracks for the project.
Completed tracks are archived in [./archive/tracks_archive.md](./archive/tracks_archive.md).

---

## [x] Track: 粒子与轨迹增强 (particle_trail_enhancement_20260213) [COMPLETED]

> **目标**: 升级 GPU 粒子系统支持纹理图集与动画，并实现基于 GPU 的动态轨迹渲染器 (TrailRenderer)。
> **文档**: [spec.md](./tracks/particle_trail_enhancement_20260213/spec.md) | [plan.md](./tracks/particle_trail_enhancement_20260213/plan.md)
> **状态**: COMPLETED (Archived in conductor/archive/)
> **Phase**: GPU 渲染系统 2.0 — Phase 3

---

## [x] Track: 打磨与高级特性 (polishing_advanced_features_20260213) [COMPLETED]

> **目标**: 完成 GPU 渲染系统 Phase 5，交付 Color Grading LUT、Ultra 体积光、Pass Profiler HUD 与 Shader 热重载能力。
> **文档**: [spec.md](./tracks/polishing_advanced_features_20260213/spec.md) | [plan.md](./tracks/polishing_advanced_features_20260213/plan.md)
> **状态**: COMPLETED (Archived in conductor/archive/)
> **Phase**: GPU 渲染系统 2.0 — Phase 5

---

## Status Update (2026-02-15)

- polishing_advanced_features_20260213: **COMPLETED & ARCHIVED**
  - delivered: Color Grading (LUT 16/32), Volumetric Light, Render Profiler HUD, Shader Hot Reload.
  - verified: 170/170 tests passed. Performance logs confirmed within budget.
- particle_trail_enhancement_20260213: **COMPLETED & ARCHIVED**
  - delivered: Textured particles, GPU Trails, Sub-emitters, Force fields.
- material_vfx_sequencer_20260213: **COMPLETED & ARCHIVED**
  - delivered: material system, vfx sequencer, distortion pass.
