# Project Tracks

This file tracks all major tracks for the project. Each track has its own detailed plan in its respective folder.

---

## [x] Track: Combat Popup Polish & Fix
- **Folder:** [./conductor/archive/combat_popup_fix_20260122/](./conductor/archive/combat_popup_fix_20260122/)
- **Description:** 修复战斗伤害弹出数字显示为中文的问题。重构了 PopupRenderer 的索引解析逻辑，修正了 Shader 中的 UV 垂直偏移与采样行，并升级了字形生成脚本（引入原生描边与软阴影）以提升清晰度�?(Completed on 2026-01-22)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 0.5 day

---

## [x] Track: Combat Pipeline Refactor & Fix
- **Folder:** [./conductor/archive/combat_pipeline_fix_20260122/](./conductor/archive/combat_pipeline_fix_20260122/)
- **Description:** 修复 CombatBalanceTest 中的“零伤害”问题。重�?SkillRegistry 增加 Basic Attack (ID 0) 兜底逻辑，增�?DamagePipeline 的鲁棒性与日志诊断，并解决�?StatsSystem 静态缓存导致的测试环境污染�?(Completed on 2026-01-22)
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 0.5 day

---

## [x] Track: Monster Level Scaling System
- **Folder:** [./conductor/archive/monster_level_scaling_20260121/](./conductor/archive/monster_level_scaling_20260121/)
- **Description:** 实现怪物随地图等级动态成长的机制。包�?HP/伤害指数成长曲线、等级同�?(玩家等级-5)、D3风格经验公式�?00级后抗性递增、护甲线性减伤成长。参�?D2/D3/POE/Grim Dawn 的成长模型�?(Completed on 2026-01-21)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1 day

---

## [x] Track: Combat Formula Refactor (Level Scaling)
- **Folder:** [./conductor/archive/combat_formula_refactor_20260121/](./conductor/archive/combat_formula_refactor_20260121/)
- **Description:** 参�?Last Epoch 重构战斗公式，引入基于等级的动态缩放因�?(Level Factor)。实现了护甲减伤/负护甲增伤、基于评�?(Rating) 的渐进式闪避率计算、以及格挡效能缩放。同步更新了 DamagePipeline �?SIMD 路径、词缀系统（新增评级词缀）及 UI 属性面板的有效值显示。已通过单元测试与集成测试验证�?(Completed on 2026-01-21)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1 day

---

## [x] Track: Stash System (Personal & Shared)
- **Folder:** [./conductor/archive/stash_system_20260121/](./conductor/archive/stash_system_20260121/)
- **Description:** 实现完整的仓库系统，包含个人仓库与账号共享仓库。支持多标签页解锁、物品分类拖拽、Ctrl+Click 快速存取、带缓存的高性能搜索、一键整理以及批量存入功能。数据层通过 SharedStash 单例�?SaveManager 深度集成，确保跨存档与场景切换的稳定性。已完成城镇地图实体的交互落地与 77 项全量测试验证�?(Completed on 2026-01-21)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 3-4 days

---

## [x] Track: Monster Affix System Implementation
- **Folder:** [./conductor/archive/monster_affixes_20260118/](./conductor/archive/monster_affixes_20260118/)
- **Description:** 实现数据驱动的怪物词缀系统。包括运行时组件 MonsterAffixComponent、机制处理器 MonsterAffixSystem（处理熔火、闪烁、狂暴、极寒、消魔等效果）、基�?StatsSystem 的属性持久化修复，以及配套的视觉特效�?UI 标签表现�?(Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1 day

---

## [x] Track: Hybrid Barrier System (ES + Ward)
- **Folder:** [./conductor/archive/hybrid_barrier_20260118/](./conductor/archive/hybrid_barrier_20260118/)
- **Description:** 实现混合护盾系统 (Energy Shield + Ward)。支持受击延迟回�?(ES 模式) 与超出上限自动衰�?(Ward 模式)。包�?CombatStats 属性聚合、CombatSystem 伤害吸收逻辑、以�?UI (PlayerHUD/MonsterBar) 的青色遮罩脉冲表现�?(Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1 day

---

## [x] Track: Performance Hardening & Logic Fixes
- **Folder:** [./conductor/archive/performance_hardening_20260117/](./conductor/archive/performance_hardening_20260117/)
- **Description:** 解决万级实体目标下的关键性能瓶颈与逻辑死锁。包括修�?StatsCache 内存泄漏、SkillSystem 零分配循环重构，整合 Hash-based 查找以及 Sword Intent 属性实时刷新逻辑�?(Completed on 2026-01-17)
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 2 days

---

## [x] Track: Dropped Item Performance Optimization
- **Folder:** [./conductor/archive/dropped_item_optimization_20260118/](./conductor/archive/dropped_item_optimization_20260118/)
- **Description:** 极速优化万级掉落物性能。通过 LabelCache 消除文字测量开销，实�?5-Stage 多阶段批次渲染极大化渲染效率，并利用 EnTT 内存过滤和空间网格分离技术稳�?180 FPS�?(Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 1 day

---

## [x] Track: Item and Drop System

- **Folder:** conductor/archive/item_drop_system_20251229
- **Description:** Implement a comprehensive item and drop system including affixes, refinement, and loot filters. (Completed on 2025-12-30)

---

## [x] Track: UI Polish & Interaction Enhancement

- **Folder:** conductor/archive/ui_polish_20251230
- **Description:** Implement high-DPI scaling, unified theme, visual effects for items/combat, and UX layout refinements. (Completed on 2025-12-30)

---

## [x] Track: Test Case Reorganization

- **Folder:** conductor/archive/test_reorganization_20251231
- **Description:** Consolidate multiple test executables into a single unified test runner using a multi-header, single-source structure. (Completed on 2025-12-31)

---

## [x] Track: Skill System Infrastructure

- **Folder:** conductor/archive/skill_system_infra_20260101
- **Description:** Core skill pipeline implementation including SkillRegistry, base skill components, and basic damage calculations. (Completed on 2026-01-01)

---

## [x] Track: Map System & Persistence

- **Folder:** conductor/archive/map_system_20260105
- **Description:** Implement map generation, tile-based systems, and save/load persistence for world state. (Completed on 2026-01-05)

---

## [x] Track: Enemy AI & Spawning

- **Folder:** conductor/archive/enemy_ai_20260110
- **Description:** Basic behavior trees, spawning logic, and entity management for enemies. (Completed on 2026-01-10)

---

## [x] Track: Crafting System & Merging

- **Folder:** conductor/archive/crafting_system_20260115
- **Description:** Implement item crafting, merging logic, and unique item generation. (Completed on 2026-01-15)

---

## [x] Track: Inventory UI Overhaul

- **Folder:** [./conductor/archive/inventory_ui_overhaul_20260117/](./conductor/archive/inventory_ui_overhaul_20260117/)
- **Description:** Refactor UIInventory to support draggable panels, tabs (Inventory, Materials), search/filter functionality, and context menus for item management. (Completed on 2026-01-17)
- **Status:** COMPLETED
- **Priority:** MEDIUM
- **Estimated Time:** 1 day

---

## [x] Track: Legendary Affix System Infrastructure
- **Folder:** [./conductor/archive/legendary_affix_system_20260117/](./conductor/archive/legendary_affix_system_20260117/)
- **Description:** 实现传奇和独特词缀的基础设施。将 AffixType 升级�?uint16_t，引入语义化 ID 区间锚点，移除冗余布尔标志，并在 ItemFactory 中实现了基于 JSON 的动态词缀加载与过滤逻辑�?(Completed on 2026-01-17)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1 day

---

## [x] Track: Equipment Salvage & Affix Shards
- **Folder:** [./conductor/archive/salvage_system_20260117/](./conductor/archive/salvage_system_20260117/)
- **Description:** Optimized equipment decomposition system with "Zero-Table Mapping" for affix shards. Features include deterministic yield logic, batch processing with safety locks, Context Menu integration, and full UI implementation. (Completed on 2026-01-17)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 2 days

---

## [x] Track: UI Visual Polish & Salvage UX
- **Folder:** [./conductor/archive/ui_visual_polish_20260117/](./conductor/archive/ui_visual_polish_20260117/)
- **Description:** Enhance global UI aesthetics (texture, borders), implement Ghost Icons for equipment, and completely redesign the Salvage Panel with Altar metaphor and Output Preview. (Completed on 2026-01-17)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 2 days

---

## [x] Track: Rune Inlay System (Refinement)
- **Folder:** [./conductor/archive/rune_inlay_system_20260117/](./conductor/archive/rune_inlay_system_20260117/)
- **Description:** Implement UI drag-and-drop for rune socketing, stack splitting logic, and visual socket indicators. Audit and verify runeword activation and ensure real-time attribute updates via StatsDirty integration. (Completed on 2026-01-17)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1 day

---

## [x] Track: Test Suite Organization & Categorization
- **Folder:** [./conductor/archive/test_suite_organization_20260118/](./conductor/archive/test_suite_organization_20260118/)
- **Description:** 对现有的 30+ 个测试文件进行物理重构与逻辑分类。将乱堆在根目录的文件整理进 unit/、integration/、functional/、performance/ �?tech/ 子目录，并同步更新测试运行器，以提升维护效率和编译清晰度�?(Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** MEDIUM
- **Estimated Time:** 0.5 day

---

## [x] Track: Equipment Tooltip Upgrade

- **Folder:** [./conductor/archive/ui_tooltip_upgrade_20260118/](./conductor/archive/ui_tooltip_upgrade_20260118/)
- **Description:** 升级装备信息面板（Tips）。在 Tooltip 中增加大尺寸装备图标显示，并在底部动态显示镶嵌孔数量（Socket Count），使用指定�?`#D0EFE8` 主题色�?(Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** MEDIUM
- **Estimated Time:** 0.5 day

---

## [x] Track: Skill System Fixes & Blade Ward Implementation
- **Folder:** [./conductor/archive/skill_fixes_20260118/](./conductor/archive/skill_fixes_20260118/)
- **Description:** Implement Blade Ward projectile interception logic and resolve persistent test failures in SkillSystem, GameplaySystems (Astrolabe), and Tech tests. (Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 0.5 day

---

## [x] Track: Constant Centralization & Refactoring
- **Folder:** [./conductor/archive/const_refactor_20260118/](./conductor/archive/const_refactor_20260118/)
- **Description:** Centralize gameplay and rendering constants to eliminate magic numbers. Created `Common.hpp` as SSOT for logic and `GPUData.hpp` for visual themes. Standardized physics epsilon, CDR caps, and particle limits. (Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** MEDIUM
- **Estimated Time:** 1 day

---

## [x] Track: Monster Affix V2 - Part 1: Environmental & Hazard
- **Folder:** [./conductor/archive/monster_affixes_hazards_20260119/](./conductor/archive/monster_affixes_hazards_20260119/)
- **Description:** Implement high-impact environmental hazard affixes (Frozen, Toxic, Void Zone, Storm Strider) using a generic HazardSystem and GPU particles. (Completed on 2026-01-19)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 2 days

---

## [x] Track: Monster Affix V2 - Part 2: Physics & Crowd Control
- **Folder:** [./conductor/archive/monster_affixes_physics_20260119/](./conductor/archive/monster_affixes_physics_20260119/)
- **Description:** Implement physics-based and CC affixes (Vortex, Waller, Entangler) by extending the PhysicsSystem with Force Fields and dynamic terrain. (Completed on 2026-01-20)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 2 days

---

## [x] Track: Monster Affix V2 - Part 3: Advanced Combat Mechanics
- **Folder:** [./conductor/archive/monster_affixes_mechanics_20260119/](./conductor/archive/monster_affixes_mechanics_20260119/)
- **Description:** Implement complex entity interactions: Mirror Image (cloning), Shielding (links), Soul Eater (scaling), Suppressor (proximity check), and Mana Siphon. (Completed on 2026-01-19)
- **Status:** COMPLETED
- **Priority:** MEDIUM
- **Estimated Time:** 3 days

---

## [x] Track: Monster Affix V2 - Part 4: Nemesis Evolution
- **Folder:** [./conductor/archive/nemesis_evolution_20260119/](./conductor/archive/nemesis_evolution_20260119/)
- **Description:** Implement the Nemesis Evolution system that adapts the boss's affixes based on the player's recorded combat history (damage types, engagement distance). (Completed on 2026-01-20)
- **Status:** COMPLETED
- **Priority:** LOW
- **Estimated Time:** 1.5 days


---

## [x] Track: Monster System Refinement & Nemesis Evolution
- **Folder:** [./conductor/archive/monster_system_refinement_20260120/](./conductor/archive/monster_system_refinement_20260120/)
- **Description:** 优化怪物词缀系统架构。消�?Nemesis 生成中的字符串硬编码，实现基�?Evolution Tier 的词缀数值动态缩放，修复宿敌 AI �?Dormancy 逻辑下的不一致性，并补�?Homing �?Phase Shield 等核心缺失机制�?(Completed on 2026-01-20)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 3 days

---

## [x] Track: Runtime String Dependency Elimination
- **Folder:** [./conductor/archive/string_dependency_elimination_20260120/](./conductor/archive/string_dependency_elimination_20260120/)
- **Description:** 消除核心系统的运行时字符串依赖。将 Biomes、Material Categories �?Astrolabe Traits 迁移至枚举（BiomeID, MaterialCategory, TraitID），同步更新 JSON 数据，并重构相关系统（LevelManager, RunewordSystem, EnemySpawnSystem）以使用静�?Map 查找代替字符串比较，显著提升性能与类型安全性�?(Completed on 2026-01-20)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1.5 days

---

## [x] Track: Performance Extreme Optimization (Multi-Phase)
- **Folder:** [./conductor/archive/performance_optimization/](./conductor/archive/performance_optimization/)
- **Description:** 多阶段极致性能优化策略，目标锁�?180 FPS (5.5ms 帧预�?。包�?5 个独�?Phase：Phase 1 (GPU-Driven MDI Rendering)、Phase 2 (EnTT Group Memory Optimization)、Phase 3 (Triple-Buffered Persistent Mapping)、Phase 4 (SIMD SpatialGrid Query)、Phase 5 (Branchless Combat Logic)。采用工业级手段彻底解决 Draw Call、Cache Miss、CPU-GPU 同步、空间查询密度和分支预测失败等性能瓶颈�?
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 10-12 days

### Sub-Tracks:
| Phase | Track ID | Priority | Status |
|-------|----------|----------|--------|
| Phase 1 | `phase1_mdi_rendering` | P0 | �?Completed |
| Phase 2 | `phase2_entt_group` | P1 | �?Completed |
| Phase 3 | `phase3_triple_buffer` | P2 | �?Completed |
| Phase 4 | `phase4_simd_spatial` | P2 | �?Completed |
| Phase 5 | `phase5_branchless` | P3 | �?Completed |
\
---

## [x] Track: Attribute Calculation & Visualization Pipeline
- **Folder:** [./conductor/archive/attribute_pipeline_design_20260123/](./conductor/archive/attribute_pipeline_design_20260123/)
- **Description:** Design and implement a high-performance attribute calculation pipeline with support for complex modifiers, conversions, and direct GPU visual feedback.
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 4.5 days

---

## [x] Track: Attribute Pipeline Cleanup
- **Folder:** [./conductor/archive/cleanup_attribute_pipeline_20260123/](./conductor/archive/cleanup_attribute_pipeline_20260123/)
- **Description:** 移除 AttributePipeline 中废弃的 PhaseX 接口与其内部实现，并重构单元测试直接验证统一的 Calculate 逻辑，提升系统简洁性与可维护性。(Completed on 2026-01-23)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 2 hours

---

## [ ] Track: Rendering Performance Validation
- **Folder:** [./conductor/tracks/rendering_performance_validation/](./conductor/tracks/rendering_performance_validation/)
- **Description:** Validation, benchmarking, and final audit of the rendering performance and sync optimizations.
- **Status:** PLANNED
- **Priority:** HIGH
- **Estimated Time:** 1 day

---

## [x] Track: Core Risk Remediation
- **Folder:** [./conductor/archive/core_risk_remediation_20260122/](./conductor/archive/core_risk_remediation_20260122/)
- **Description:** Comprehensive remediation of core risks identified in the code audit. Implemented safe iterative Damage Conversion (no loops), Assassin AI teleport safety checks, Triple-Buffered GPU Physics to fix race conditions, and Runeword subtype validation. (Completed on 2026-01-23)
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 4-5 days

\
