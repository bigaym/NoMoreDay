# Track: GPU RenderGraph and Resource Foundation

**ID:** gpu_rendergraph_resource_foundation_20260726
**Status:** In Progress (remediation; production NO-GO)
**Type:** refactor/foundation
**Priority:** P0
**Milestone:** GPU Production Closure M0

## 核心文档

- [技术规格书](./spec.md)
- [实施计划](./plan.md)
- [验证记录](./validation.md)
- [整改债务登记](./debt_register.md)

## 进度概览

- **Phases**: 0/5 accepted
- **Tasks**: 0/25 accepted

## 全量接口迁移（旧接口收敛路线图）

> 目标：把绕过 typed RenderGraph 契约的全部旧接口/手工路径（shadow/cluster typed 声明、手工 barrier、V2 光照 fallback、RenderSystem 条件链、registry 缺口、reload/capability 双轨）收敛到最新引擎接口，关闭 RG-1~RG-6。生产 NO-GO 不变，仅 M0-C 游戏二进制 artifact 可改变。

- [技术规格设计](../../../docs/designs/2026-08-03-render-engine-interface-migration-design.md)
- [Phase G 计划：资源注册表补齐](../../../docs/plans/2026-08-03-migration-phase-g-registry-plan.md)
- [Phase B 计划：5 个空 Setup pass typed 迁移](../../../docs/plans/2026-08-03-migration-phase-b-typed-passes-plan.md)
- [Phase C 计划：手工 barrier 收敛](../../../docs/plans/2026-08-03-migration-phase-c-manual-barriers-plan.md)
- [Phase E 计划：旧光照 fallback 收敛](../../../docs/plans/2026-08-03-migration-phase-e-lighting-fallback-plan.md)
- [Phase D 计划：RenderSystem 手工条件链收敛](../../../docs/plans/2026-08-03-migration-phase-d-rendersystem-plan.md)
- [Phase F 计划：reload/capability 单一路径](../../../docs/plans/2026-08-03-migration-phase-f-reload-capability-plan.md)

> 先前完成记录已被 [生产整改 Track 集成审查](../../../docs/reviews/2026-07-26-gpu-production-remediation-tracks-review.md) 否决。仅当规格验收与可复现硬件证据同时满足时才恢复完成状态。

## 快速链接

- [返回 Tracks 列表](../../tracks.md)
- [V5 主控规格书](../../specs/rendering_engine_v5_master_spec.md)
- [GPU 渲染引擎架构审查](../../../docs/reviews/2026-07-26-gpu-rendering-engine-audit-review.md)
- [前序生产路径 Track](../gpu_production_hdr_gi_closure_20260726/index.md)
