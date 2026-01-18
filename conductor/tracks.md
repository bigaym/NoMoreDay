# Project Tracks

This file tracks all major tracks for the project. Each track has its own detailed plan in its respective folder.

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

## [x] Track: Equipment Tooltip Upgrade

- **Folder:** [./conductor/archive/ui_tooltip_upgrade_20260118/](./conductor/archive/ui_tooltip_upgrade_20260118/)
- **Description:** 升级装备信息面板（Tips）。在 Tooltip 中增加大尺寸装备图标显示，并在底部动态显示镶嵌孔数量（Socket Count），使用指定的 `#D0EFE8` 主题色。 (Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** MEDIUM
- **Estimated Time:** 0.5 day
