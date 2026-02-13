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

## [ ] Track: 材质与 VFX 序列器 (material_vfx_sequencer_20260213)

> **目标**: 实现数据驱动的材质系统（constexpr预设+JSON+SSBO）、VFX序列器（JSON时间线→多层视觉编排）和屏幕扭曲通路（DistortionPass），使新技能特效可纯数据驱动创建。
> **文档**: [spec.md](./tracks/material_vfx_sequencer_20260213/spec.md) | [plan.md](./tracks/material_vfx_sequencer_20260213/plan.md)
> **状态**: IN_PROGRESS (A/B 完成；C/D/I/K 部分完成；总体 43/118 tasks)
> **Phase**: GPU 渲染系统 2.0 — Phase 4
> **前置依赖**: Phase 1 ✅ + Phase 3 ✅
> **预估工时**: 5~7 天
