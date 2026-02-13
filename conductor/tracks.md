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

## Status Update (2026-02-13)

- material_vfx_sequencer_20260213: COMPLETED & ARCHIVED
  - delivered: material system, vfx sequencer, distortion pass, prefab library
  - fixed: critical particle system deadlock (BUG-20260213-001)
  - verified: full test suite & visual check
