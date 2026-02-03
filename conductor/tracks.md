# Project Tracks

This file tracks all major tracks for the project. Each track has its own detailed plan in its respective folder.

---

## [x] Track: Void Astrolabe (Global Passive System)
- **Folder:** [./conductor/tracks/ui-astrolabe_20260201/](./conductor/tracks/ui-astrolabe_20260201/)
- **Description:** Implement a Grim Dawn-inspired "Void Astrolabe" UI for the global passive talent system. This involves an infinite canvas, star-map aesthetics, and a graph-based data structure that supports arbitrary positioning and constellation groupings. (Completed on 2026-02-01)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 3-4 days

---

## [ ] Track: Skill Specialization System
- **Folder:** [./conductor/archive/ui-skill-spec_20260201/](./conductor/archive/ui-skill-spec_20260201/)
- **Description:** 重构了技能专精 UI，实现了类似 Last Epoch 的“中心枢纽 + 4 分支”布局。引入了 `UISkillSpecRenderer` 进行渲染与逻辑分离，实现了基于 Tag 的视觉主题（火/冰/雷/虚空等），以及区分 Keystone (菱形)、Modifier (方形)、Passive (圆形) 的节点形状和通道式连接线。 (Completed on 2026-02-01)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 2-3 days

---

## [x] Track: Dimensional Level Selection & Fragment Failsafe
- **Folder:** [./conductor/archive/dimensional-level-select_20260131/](./conductor/archive/dimensional-level-select_20260131/)
- **Description:** 优化维度地图的进入体验。新增了等级选择界面，允许玩家在 (1 ~ 玩家等级+10) 范围内自定义难度。实现了默认碎片检测机制，防止空背包卡关。修复了城镇传送门触发逻辑，并对齐了传送门视觉素材与实际触发区域。 (Completed on 2026-01-31)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 0.5 day

---

## [x] Track: Lock & Concurrency Optimization
- **Folder:** [./conductor/archive/lock_optimization_20260128/](./conductor/archive/lock_optimization_20260128/)
- **Description:** 优化项目中的锁使用。引入 ThreadSafeRandom，重构 NemesisDataStore 以减少锁持有时间并消除字符串依赖，实现 GPUParticleSystem 的无锁化发射，并为 ResourceManager 添加读写锁保护 (Completed on 2026-01-28)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 2 days

---

## [x] Track: Static Variable & Global State Optimization
- **Folder:** [./conductor/archive/static_variable_optimization_20260128/](./conductor/archive/static_variable_optimization_20260128/)
- **Description:** 系统性重构项目中的静态变量。引入了 `std::shared_mutex` 解决 StatsCache 并发查询，利用 `thread_local` 修复了 RNG 线程安全问题，消除了 SkillSystem 热路径分配，并建立了统一的静态资源生命周期管理协议 (Completed on 2026-01-28)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 2 days

---

## [x] Track: Test Case Naming Normalization
- **Folder:** [./conductor/archive/test_normalization_20260126/](./conductor/archive/test_normalization_20260126/)
- **Description:** 统一 NoMoreDay 项目中所有 110 个测试用例 (`TEST_CASE`) 的命名格式。引入了 `[Unit]`, `[Integration]`, `[Functional]`, `[Performance]`, `[Tech]`, `[Bugfix]` 六大分类前缀，显著提升了测试过滤 (`doctest -tc`) 与结果分析的效率，并完成了全量性能基准测试验证 (Completed on 2026-01-26)
- **Status:** COMPLETED
- **Priority:** MEDIUM
- **Estimated Time:** 0.5 day

---

## [x] Track: Fix Dimensional Mosaic System & Performance Critical Recovery
- **Folder:** [./conductor/archive/fix-dimensional-mosaic-20260126/](./conductor/archive/fix-dimensional-mosaic-20260126/)
- **Description:** 修复维度图拼接界面的词缀显示逻辑，实现了数值聚合与描述模板系统。纠正了异界传送门的层级跳转逻辑，引入了碎片生命周期衰减机制，并完善了回城后场景状态的持久化恢复。特别修复了由词缀系统 O(N^2) 遍历导致的性能严重下降问题，通过 Spatial Grid 优化和 Staggered 生成逻辑将 FPS 从 40 恢复至 130+ (Completed on 2026-01-26)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1-2 days

---

## [x] Track: GPU Rendering Pipeline Refactor (Multi-Phase)
- **Folder:** [./conductor/archive/rendering_pipeline_refactor_20260126/](./conductor/archive/rendering_pipeline_refactor_20260126/)
- **Description:** 基于架构审计报告，系统性重构 GPU 渲染管线。消除魔法数字、统一 GL 抽象层、分解 GPUEntitySystem 职责、规范化 MDIRenderer，并解耦单例以提升可测试性。遵循 DOD 原则，目标实现代码一致性、性能提升、安全防护、高可用与低耦合。
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 5-6 weeks (52-60h total)
- **Risk Level:** 🔴 HIGH (涉及核心渲染路径重构)

### Sub-Tracks:
| Track | Name | Priority | Status | Est. Time | Risk |
|-------|------|----------|--------|-----------|------|
| T1 | `render_constants` - 绑定常量标准化 | P0 | 🟢 已完成 | 4h | 低 |
| T2 | `gpu_utils_unification` - GPU 工具层统一 | P1 | 🟢 已完成 | 8h | 中 |
| T3 | `gpuEntitySystem_decomposition` - 职责分解 | P0 | 🟢 已完成 | 16-24h | 高 |
| T4 | `mdi_renderer_refactor` - MDI 渲染器规范化 | P1 | 🟢 已完成 | 8h | 中 |
| T5 | `singleton_decoupling` - 单例解耦与依赖注入 | P2 | 🟢 已完成 | 16h | 高 |

---

## [x] Track: GPU Sprite Offload (MDI Texture Array)
- **Folder:** [./conductor/archive/gpu-sprite-offload_20260125/](./conductor/archive/gpu-sprite-offload_20260125/)
- **Description:** 将 CPU 精灵渲染卸载至 GPU。完成了 `GL_TEXTURE_2D_ARRAY` 与 MDI 管线的整合，实现了状态效果 (冰冻/燃烧) 的 Shader 级叠加，并清理了遗留的单体 Draw Call 路径。成功将 Draw Calls 降低至个位数，渲染效率大幅提升。(Completed on 2026-01-25)
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 2 days

---

## [x] Track: Render Layer Batching Optimization (Instanced UI Labels)
- **Folder:** [./conductor/archive/render_layer_batching_20260125/](./conductor/archive/render_layer_batching_20260125/)
- **Description:** 极致优化海量掉落物标签渲染性能。引入了 **GPU Instancing + SDF** 渲染方案，将数百个背景框合并为 1 次 Draw Call，并实现了 **Buffer Orphaning** 消除 GPU 同步阻塞。将渲染管线重构为分层批量模式（Geometry Pass & Text Pass），在 500+ 可见物体下将渲染开销从 10ms+ 降低至 < 0.5ms，稳定维持 400+ FPS。(Completed on 2026-01-25)
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 0.5 day

---

## [x] Track: Render Items CPU Optimization (Beam & Cache)
- **Folder:** [./conductor/archive/render-items-cpu-optimization_20260125/](./conductor/archive/render-items-cpu-optimization_20260125/)
- **Description:** 优化掉落物品的 CPU 侧性能。通过 `VisibleItemCache` 消除 UISystem 中的冗余 $O(N)$ 遍历，并实现了光束 (Beam) 的 Instanced Rendering，显著降低了 Driver Overhead 和 Draw Calls。(Completed on 2026-01-25)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 0.5 day

---

## [x] Track: Equipment Level System
- **Folder:** [./conductor/archive/equipment-level-system/](./conductor/archive/equipment-level-system/)
- **Description:** 实现装备等级系统。包含物品等级的基础数据实现（ItemComponent）、基于地图等级的动态掉落逻辑（ItemFactory）、装备等级限制的强制校验（InventorySystem）以及 UI 和 Tooltip 的视觉呈现。提取了 Constants::Items 常量并优化了 UISystem::State.playerEntity 的渲染性能 (Completed on 2026-01-24)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 0.5 day

---

## [x] Track: Map Affix & Persistence System
- **Folder:** [./conductor/archive/map-affix-system_20260125/](./conductor/archive/map-affix-system_20260125/)
- **Description:** Implement Map Affix Registry, Difficulty Score (DS) calculation, and reward scaling. Integrated dimensional state into DropSystem, ItemFactory, and EnemySpawnSystem. Added TAB overlay for active modifiers. (Completed on 2026-01-25)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1.5 days

---

## [x] Track: Comprehensive Code Audit (Physics & Rendering)
- **Folder:** [./conductor/archive/code-audit-20260124/](./conductor/archive/code-audit-20260124/)
- **Description:** 对物理引擎与渲染管线进行深度审计。识别了 SSBO 结构体不匹配 (48 vs 64 bytes)、受力场逻辑回归、硬编码魔数以及 AI 休眠位置残影等关键风险点 (Completed on 2026-01-24)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 0.5 day

---

## [x] Track: Physics & Rendering Audit Fixes
- **Folder:** [./conductor/archive/fix-physics-rendering-audit-20260124/](./conductor/archive/fix-physics-rendering-audit-20260124/)
- **Description:** 修复审计发现的核心风险。恢复了 ForceField 逻辑，统一了 GPUInstanceData 与 GPUEntity 为 64 字节对齐，并完成了物理常量的集中提取与魔数清理 (Completed on 2026-01-24)
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 0.5 day

---

## [x] Track: Enemy Rendering Refactor (CPU Authority)
- **Folder:** [./conductor/archive/enemy-rendering-refactor/](./conductor/archive/enemy-rendering-refactor/)
- **Description:** 将敌人渲染架构由“GPU物理更新+SyncBack”重构为“CPU权威+GPU加速渲染”。解决了 N-2 帧同步延迟导致的视觉抖动与幽灵位移，确立了 CPU 为单一事实来源，提升了系统的稳定性与可维护性 (Completed on 2026-01-24)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1 day

---

## [x] Track: Core Risk Remediation
- **Folder:** [./conductor/archive/core_risk_remediation_20260122/](./conductor/archive/core_risk_remediation_20260122/)
- **Description:** Comprehensive remediation of core risks identified in the code audit. Implemented safe iterative Damage Conversion (no loops), Assassin AI teleport safety checks, Triple-Buffered GPU Physics to fix race conditions, and Runeword subtype validation. (Completed on 2026-01-23)
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 4-5 days

---

## [x] Track: Attribute Calculation & Visualization Pipeline
- **Folder:** [./conductor/archive/attribute_pipeline_design_20260123/](./conductor/archive/attribute_pipeline_design_20260123/)
- **Description:** Design and implement a high-performance attribute calculation pipeline with support for complex modifiers, conversions, and direct GPU visual feedback. (Completed on 2026-01-23)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 4.5 days

---

## [x] Track: Talent Behavior Implementation
- **Folder:** [./conductor/archive/talent_behavior_implementation_20260123/](./conductor/archive/talent_behavior_implementation_20260123/)
- **Description:** Implemented core talent behaviors for Skill 1 (Flowing Thrust), Skill 2 (Rending Wave), and Skill 3 (Blade Formation), including projectile splitting, hovering hazards, and melee orbit modes. (Completed on 2026-01-23)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 8.5 hours

---

## [x] Track: Combat Popup Polish & Fix
- **Folder:** [./conductor/archive/combat_popup_fix_20260122/](./conductor/archive/combat_popup_fix_20260122/)
- **Description:** 修复战斗伤害弹出数字显示为中文的问题。重构了 PopupRenderer 的索引解析逻辑，修正了 Shader 中的 UV 垂直偏移与采样行，并升级了字形生成脚本（引入原生描边与软阴影）以提升清晰度 (Completed on 2026-01-22)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 0.5 day

---

## [x] Track: Combat Formula Refactor (Level Scaling)
- **Folder:** [./conductor/archive/combat_formula_refactor_20260121/](./conductor/archive/combat_formula_refactor_20260121/)
- **Description:** 参考 Last Epoch 重构战斗公式，引入基于等级的动态缩放因子 (Level Factor)。实现了护甲减伤/负护甲增伤、基于评级 (Rating) 的渐进式闪避率计算、以及格挡效能缩放。同步更新了 DamagePipeline、SIMD 路径、词缀系统（新增评级词缀）及 UI 属性面板的有效值显示。已通过单元测试与集成测试验证 (Completed on 2026-01-21)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1 day

---

## [x] Track: Stash System (Personal & Shared)
- **Folder:** [./conductor/archive/stash_system_20260121/](./conductor/archive/stash_system_20260121/)
- **Description:** 实现完整的仓库系统，包含个人仓库与账号共享仓库。支持多标签页解锁、物品分类拖拽、Ctrl+Click 快速存取、带缓存的高性能搜索、一键整理以及批量存入功能。数据层通过 SharedStash 单例与 SaveManager 深度集成，确保跨存档与场景切换的稳定性。已完成城镇地图实体的交互落地与 77 项全量测试验证 (Completed on 2026-01-21)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 3-4 days

---

## [x] Track: Monster Affix System Implementation
- **Folder:** [./conductor/archive/monster_affixes_20260118/](./conductor/archive/monster_affixes_20260118/)
- **Description:** 实现数据驱动的怪物词缀系统。包括运行时组件 MonsterAffixComponent、机制处理器 MonsterAffixSystem（处理熔火、闪烁、狂暴、极寒、消魔等效果）、基于 StatsSystem 的属性持久化修复，以及配套的视觉特效与 UI 标签表现 (Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1 day

---

## [x] Track: Hybrid Barrier System (ES + Ward)
- **Folder:** [./conductor/archive/hybrid_barrier_20260118/](./conductor/archive/hybrid_barrier_20260118/)
- **Description:** 实现混合护盾系统 (Energy Shield + Ward)。支持受击延迟回复 (ES 模式) 与超出上限自动衰减 (Ward 模式)。包含 CombatStats 属性聚合、CombatSystem 伤害吸收逻辑、以及 UI (PlayerHUD/MonsterBar) 的青色遮罩脉冲表现 (Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1 day

---

## [x] Track: Performance Hardening & Logic Fixes
- **Folder:** [./conductor/archive/performance_hardening_20260117/](./conductor/archive/performance_hardening_20260117/)
- **Description:** 解决万级实体目标下的关键性能瓶颈与逻辑死锁。包括修复 StatsCache 内存泄漏、SkillSystem 零分配循环重构，整合 Hash-based 查找以及 Sword Intent 属性实时刷新逻辑 (Completed on 2026-01-17)
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 2 days

---

## [x] Track: Dropped Item Performance Optimization
- **Folder:** [./conductor/archive/dropped_item_optimization_20260118/](./conductor/archive/dropped_item_optimization_20260118/)
- **Description:** 极速优化万级掉落物性能。通过 LabelCache 消除文字测量开销，实现 5-Stage 多阶段批次渲染极大化渲染效率，并利用 EnTT 内存过滤和空间网格分离技术稳定 180 FPS (Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 1 day

---

## [x] Track: Performance Hardening V2 (AZDO Particle & MDI)
- **Folder:** [./conductor/archive/performance-hardening-v2_20260131/](./conductor/archive/performance-hardening-v2_20260131/)
- **Description:** 极致优化大批量物体渲染。通过无锁环形缓冲区重构粒子发射，引入稀疏更新 (Sparse Update) 机制优化 MDI 实体状态同步，实现了粒子系统动态调度缩放与掉落物标签空间查询优化。 (Completed on 2026-01-31)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 2-3 days

---

## [x] Track: Loot Text GPU Instancing & Layout Optimization
- **Folder:** [./conductor/archive/loot-text-gpu_20260131/](./conductor/archive/loot-text-gpu_20260131/)
- **Description:** 极致优化战利品标签渲染。通过 GPU 字形实例化（Instancing）彻底消除海量中文文本渲染带来的 CPU 提交瓶颈，并实现标签防重叠布局算法，确保 100+ 掉落物下稳定维持 400+ FPS。 (Completed on 2026-01-31)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1-2 days

---

## [x] Track: UI Button Visual Polish
- **Folder:** [./conductor/archive/ui-button-polish_20260131/](./conductor/archive/ui-button-polish_20260131/)
- **Description:** 将新增加的按钮美术资源（主菜单、冰蓝色风格等）整合进现有的 UI 系统，建立统一的按钮渲染标准并重构主菜单、仓库、技能专精等界面。 (Completed on 2026-01-31)
- **Status:** COMPLETED
- **Priority:** MEDIUM
- **Estimated Time:** 0.5 day

---

## [x] Track: Astrolabe Refactor (Universal Origin Layout)
- **Folder:** [./conductor/tracks/astrolabe-refactor_20260204/](./conductor/tracks/astrolabe-refactor_20260204/)
- **Description:** 将现有的"星座-连接"天赋树模型重构为"宇宙同源 (Universal Origin)"六扇区布局。核心变更包括：以黑洞为中心的 6 职业扇区向心布局、纯亲和度点数解锁机制（无前置节点依赖）、以及"职业誓约 (The Vow)"系统区分主修/辅修。**无需存档兼容，可直接重写数据格式。**
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 3-4 days (28-36h)
- **Risk Level:** 🟢 LOW (无需存档兼容，可放开手脚)

### Sub-Tracks:
| Track | Name | Priority | Status | Est. Time | Risk |
|-------|------|----------|--------|-----------|------|
| T1 | `data-model` - 数据模型重构 | P0 | 🔵 待开始 | 4h | 低 |
| T2 | `layout-service` - 布局服务 | P1 | 🔵 待开始 | 3h | 低 |
| T3 | `talent-loader` - 加载器重写 | P1 | 🔵 待开始 | 3h | 低 |
| T4 | `system-logic` - 核心系统逻辑 | P0 | 🔵 待开始 | 5h | 中 |
| T5 | `renderer` - UI 渲染 + GPU 特效 | P1 | 🔵 待开始 | 8h | 中 |
| T6 | `interaction` - UI 交互重构 | P1 | 🔵 待开始 | 5h | 中 |
| T7 | `testing` - 测试与集成 | P2 | 🔵 待开始 | 4h | 低 |

---

## [ ] Track: Next Milestone (TBD)
- **Folder:** [./conductor/tracks/next-track/](./conductor/tracks/next-track/)
- **Description:** 规划下一个功能模块或优化方向。
- **Status:** PLANNED
- **Priority:** MEDIUM
- **Estimated Time:** TBD
