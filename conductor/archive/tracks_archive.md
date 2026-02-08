# Archived Project Tracks

This file contains completed and archived tracks.

---

## [x] Track: TODO Closure Core Loop (2026-02-08)
- **Folder:** [./conductor/archive/todo-closure-core-loop_20260208/](./conductor/archive/todo-closure-core-loop_20260208/)
- **Description:** 闭环修复核心循环中的遗留 TODO。包括 SkillSystem 反击伤害接入 DamagePipeline，以及 SaveManager 存档头信息（玩家名、游玩时长）的动态化实现。
- **Status:** COMPLETED (2026-02-08)
- **Priority:** P0
- **Estimated Time:** 8-10 hours

---

## [x] Track: Astrolabe Logic Completion & Combat Integration
- **Folder:** [./conductor/archive/astrolabe-logic-completion_20260205/](./conductor/archive/astrolabe-logic-completion_20260205/)
- **Description:** 解决天赋数值缩放不匹配、核心机制组件（如图/剑意/剑心）挂载失效及特殊转换效果缺失的问题。确保天赋星盘逻辑与战斗系统深度耦合。
- **Status:** COMPLETED (2026-02-08)
- **Priority:** HIGH
- **Estimated Time:** 2-3 days

---

## [x] Track: Astrolabe VFX & Architecture Polish
- **Folder:** [./conductor/archive/astrolabe-vfx-polish_20260204/](./conductor/archive/astrolabe-vfx-polish_20260204/)
- **Description:** 补全星盘系统的视觉与架构优化。包括实现高级 GPU 节点着色器 (talent_node.fs)、能量流动粒子反馈、超新星点满特效。同时修复 CMake 构建系统无法识别新测试的问题，并对 UIAstrolabe 进行职责解耦重构。
- **Status:** COMPLETED (2026-02-05)
- **Priority:** HIGH
- **Estimated Time:** 1.5 days

---

## [x] Track: Astrolabe Audit Polish (Post-Refactor Fixes)
- **Folder:** [./conductor/archive/astrolabe-polish_20260204/](./conductor/archive/astrolabe-polish_20260204/)
- **Description:** 修复星系天赋系统重构后的审计缺陷。包括：(FIX-1) 誓约二次确认机制、(FIX-2) 测试数据 ID 一致性、(FIX-3) AstrolabeSystem 单元测试覆盖、(FIX-4) 解锁失败 UI 反馈、(FIX-5) GPU 节点特效 Shader、(FIX-6) 冗余 API 清理。
- **Status:** COMPLETED (2026-02-04)
- **Priority:** HIGH
- **Estimated Time:** 10-12 hours

---

## [x] Track: Astrolabe System Refactor (Six-Sector Layout)
- **Folder:** [./conductor/archive/astrolabe-refactor_20260204/](./conductor/archive/astrolabe-refactor_20260204/)
- **Description:** 将天赋系统从星座依赖模型重构为六扇区同源布局。引入了动态坐标计算、职业亲和度解锁机制以及主修誓约限制。
- **Status:** COMPLETED (2026-02-04)
- **Priority:** HIGH
- **Estimated Time:** 3-4 days

---

## [x] Track: Void Astrolabe (Initial Implementation)
- **Folder:** [./conductor/archive/ui-astrolabe_20260201/](./conductor/archive/ui-astrolabe_20260201/)
- **Description:** Implement a Grim Dawn-inspired "Void Astrolabe" UI. This involves an infinite canvas, star-map aesthetics, and a graph-based data structure.
- **Status:** COMPLETED (2026-02-01)
- **Priority:** HIGH
- **Estimated Time:** 3-4 days

---

## [x] Track: Skill Specialization System
- **Folder:** [./conductor/archive/ui-skill-spec_20260201/](./conductor/archive/ui-skill-spec_20260201/)
- **Description:** 重构了技能专精 UI，实现了类似 Last Epoch 的“中心枢纽 + 4 分支”布局。
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 2-3 days

---

## [x] Track: Dimensional Level Selection & Fragment Failsafe
- **Folder:** [./conductor/archive/dimensional-level-select_20260131/](./conductor/archive/dimensional-level-select_20260131/)
- **Description:** 优化维度地图的进入体验。新增了等级选择界面，实现了默认碎片检测机制。
- **Status:** COMPLETED
- **Priority:** HIGH

---

## [x] Track: Lock & Concurrency Optimization
- **Folder:** [./conductor/archive/lock_optimization_20260128/](./conductor/archive/lock_optimization_20260128/)
- **Description:** 优化项目中的锁使用。引入 ThreadSafeRandom，实现 GPUParticleSystem 的无锁化发射。
- **Status:** COMPLETED

---

## [x] Track: Static Variable & Global State Optimization
- **Folder:** [./conductor/archive/static_variable_optimization_20260128/](./conductor/archive/static_variable_optimization_20260128/)
- **Description:** 系统性重构项目中的静态变量。引入了 `std::shared_mutex` 解决 StatsCache 并发查询。
- **Status:** COMPLETED

---

## [x] Track: Test Case Naming Normalization
- **Folder:** [./conductor/archive/test_normalization_20260126/](./conductor/archive/test_normalization_20260126/)
- **Description:** 统一 NoMoreDay 项目中所有测试用例的命名格式。
- **Status:** COMPLETED

---

## [x] Track: GPU Rendering Pipeline Refactor
- **Folder:** [./conductor/archive/rendering_pipeline_refactor_20260126/](./conductor/archive/rendering_pipeline_refactor_20260126/)
- **Description:** 基于架构审计报告，系统性重构 GPU 渲染管线。
- **Status:** COMPLETED