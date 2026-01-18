# Project Tracks

This file tracks all major tracks for the project. Each track has its own detailed plan in its respective folder.

---

## [x] Track: Monster Affix System Implementation
- **Folder:** [./conductor/archive/monster_affixes_20260118/](./conductor/archive/monster_affixes_20260118/)
- **Description:** 实现数据驱动的怪物词缀系统。包括运行时组件 MonsterAffixComponent、机制处理器 MonsterAffixSystem（处理熔火、闪烁、狂暴、极寒、消魔等效果）、基于 StatsSystem 的属性持久化修复，以及配套的视觉特效与 UI 标签表现。 (Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1 day

---

## [x] Track: Hybrid Barrier System (ES + Ward)
- **Folder:** [./conductor/archive/hybrid_barrier_20260118/](./conductor/archive/hybrid_barrier_20260118/)
- **Description:** 实现混合护盾系统 (Energy Shield + Ward)。支持受击延迟回复 (ES 模式) 与超出上限自动衰减 (Ward 模式)。包括 CombatStats 属性聚合、CombatSystem 伤害吸收逻辑、以及 UI (PlayerHUD/MonsterBar) 的青色遮罩脉冲表现。 (Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1 day

---

## [x] Track: Performance Hardening & Logic Fixes
- **Folder:** [./conductor/archive/performance_hardening_20260117/](./conductor/archive/performance_hardening_20260117/)
- **Description:** 解决万级实体目标下的关键性能瓶颈与逻辑死锁。包括修复 StatsCache 内存泄漏、SkillSystem 零分配循环重构，整合 Hash-based 查找以及 Sword Intent 属性实时刷新逻辑。 (Completed on 2026-01-17)
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 2 days

---

## [x] Track: Dropped Item Performance Optimization
- **Folder:** [./conductor/archive/dropped_item_optimization_20260118/](./conductor/archive/dropped_item_optimization_20260118/)
- **Description:** 极速优化万级掉落物性能。通过 LabelCache 消除文字测量开销，实现 5-Stage 多阶段批次渲染极大化渲染效率，并利用 EnTT 内存过滤和空间网格分离技术稳定 180 FPS。 (Completed on 2026-01-18)
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
- **Description:** 实现传奇和独特词缀的基础设施。将 AffixType 升级为 uint16_t，引入语义化 ID 区间锚点，移除冗余布尔标志，并在 ItemFactory 中实现了基于 JSON 的动态词缀加载与过滤逻辑。 (Completed on 2026-01-17)
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
- **Description:** 对现有的 30+ 个测试文件进行物理重构与逻辑分类。将乱堆在根目录的文件整理进 unit/、integration/、functional/、performance/ 和 tech/ 子目录，并同步更新测试运行器，以提升维护效率和编译清晰度。 (Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** MEDIUM
- **Estimated Time:** 0.5 day

---

## [x] Track: Equipment Tooltip Upgrade

- **Folder:** [./conductor/archive/ui_tooltip_upgrade_20260118/](./conductor/archive/ui_tooltip_upgrade_20260118/)
- **Description:** 升级装备信息面板（Tips）。在 Tooltip 中增加大尺寸装备图标显示，并在底部动态显示镶嵌孔数量（Socket Count），使用指定的 `#D0EFE8` 主题色。 (Completed on 2026-01-18)
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
- **Folder:** [./conductor/tracks/const_refactor/](./conductor/tracks/const_refactor/)
- **Description:** Centralize gameplay and rendering constants to eliminate magic numbers. Created `Common.hpp` as SSOT for logic and `GPUData.hpp` for visual themes. Standardized physics epsilon, CDR caps, and particle limits. (Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** MEDIUM
- **Estimated Time:** 1 day


