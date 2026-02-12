# Project Tracks

This file tracks all major active tracks for the project.
Completed tracks are archived in [./archive/tracks_archive.md](./archive/tracks_archive.md).

---

## [x] Track: Rendering Foundation Migration (rendering_foundation_migration_20260212)

> **目标**: 建立 RenderGraph 架构，拆解 RenderSystem 巨型函数，引入 FBO 池化与画质分级管理，为 HDR/后处理铺平道路。
> **文档**: [spec.md](./archive/rendering_foundation_migration_20260212/spec.md) | [plan.md](./archive/rendering_foundation_migration_20260212/plan.md)
> **状态**: COMPLETED (2026-02-12), 已归档

## [ ] Track: HDR + 后处理管线 (hdr_postprocess_pipeline_20260212)

> **目标**: 将 LDR 直出管线升级为 HDR → Bloom → Tonemap → FXAA → Vignette → LDR 完整后处理链路，为视觉特效提供物理正确的发光基础。
> **文档**: [spec.md](./tracks/hdr_postprocess_pipeline_20260212/spec.md) | [plan.md](./tracks/hdr_postprocess_pipeline_20260212/plan.md)
> **状态**: IN_PROGRESS (开发完成，验收中)
> **Phase**: GPU 渲染系统 2.0 — Phase 1
> **预估工时**: 5~6 天

