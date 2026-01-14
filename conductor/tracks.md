# Project Tracks

This file tracks all major tracks for the project. Each track has its own detailed plan in its respective folder.

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
- **Description:** Implement a tag-driven damage calculation pipeline with automated tag registry and zero-allocation optimization. (Completed on 2026-01-01)

---

## [x] Track: Astrolabe Foundation

- **Folder:** conductor/archive/astrolabe_foundation_20260101
- **Description:** Implement a data-driven passive talent tree foundation including JSON parsing, activation logic, and stats integration. (Completed on 2026-01-01)

---

## [x] Track: Sword Cultivator Skill System

- **Folder:** conductor/archive/skill_system_sword_cultivator_20260102
- **Description:** Implement active skills, tag-aware scaling, sword intent, shadow casting, and Sword Heart integration. (Completed on 2026-01-02)

---

## [x] Track: Skill UI & Buff System

- **Folder:** conductor/archive/skill_ui_buff_system_20260102
- **Description:** Implement skill hotbar with cooldowns, dynamic tooltips with Astrolabe integration, and a comprehensive Buff/Debuff display system. (Completed on 2026-01-02)   

---

## [x] Track: Astrolabe UI

- **Folder:** conductor/archive/astrolabe_ui_20260101
- **Description:** Implement the interactive concentric UI for the talent tree, including zooming, panning, and planning mode. (Completed on 2026-01-02)

---

## [x] Track: Crafting System UI

- **Folder:** conductor/archive/crafting_ui_20260103
- **Description:** Implement the user interface for the Crafting System, allowing players to upgrade, add, chaos, and refine affixes on items. (Completed on 2026-01-03)

---

## [x] Track: Skill Management & Specialization System

- **Folder:** conductor/archive/skill_specialization_20260103
- **Description:** Implement a comprehensive Skill Management Interface (Hotkey: 'S') including 5 specialization slots, unique talent trees for skills, and point allocation. (Completed on 2026-01-03)

---

- [x] Skill Specialization Trees (Flowing Thrust & Rending Wave) (skill_spec_trees_20260103) - *Completed 2026-01-03*

---

## [x] Track: Blade Ascendant Skill Specialization (Remaining 7 Skills)

- **Folder:** conductor/archive/blade_ascendant_specialization_20260103
- **Description:** Implement specialization talent trees and unique mechanics for the remaining 7 Blade Ascendant skills, including Area, Automation, and Channeling logic. (Completed on 2026-01-03)

---

## [x] Track: GPU-Driven System Optimization (Compute Shaders)

- **Folder:** conductor/archive/gpu_optimization_20260104
- **Description:** Optimize the game engine to support 10,000+ entities by offloading particles, physics, collision, and pathfinding to the GPU using OpenGL 4.3 Compute Shaders. (Completed on 2026-01-04)

---

## [x] Track: Skill System Optimization, UI Polish, and GPU VFX

- **Folder:** conductor/archive/skill_vfx_gpu_ui_20260105

- **Description:** Implement advanced skill mechanics, deep UI feedback (Drag-and-Drop, FCT), and high-performance GPU visual effects. (Completed on 2026-01-07)

---

## [x] Track: 技能特效的 GPU 粒子化 (GPU Skill VFX) (水墨修仙风)

- **Folder:** [./conductor/tracks/gpu_skill_vfx_20260108/](./conductor/tracks/gpu_skill_vfx_20260108/)
- **Description:** 利用 GPU 粒子系统为 9 个核心技能实现淡雅水墨修仙风格的视觉特效，并包含强化版的“金韵墨痕”表现。

---

- [x] Track: Fix 'Increased' Damage Multiplier Bug & Re-tune Scaling
- **Folder:** conductor/archive/fix_increased_damage_20260109/
- **Description:** Fix the exponential damage scaling bug by making 'Increased' modifiers additive, and re-tune data values. (Completed on 2026-01-09)

---

## Phase 6: 终局玩法构建 (Endgame Construction)

### [x] Track: 维度拼接系统 (Dimensional Mosaic)

- **Folder:** conductor/archive/dimensional_mosaic_20260113/
- **Description:** 实现地图碎片掉落、3x3 拼图编辑器和共鸣机制。通过 Command Buffer 确保线程安全，并优化了地图生成的通用性。玩家通过拼接碎片构建下一层的挑战。 (Completed on 2026-01-13)

---

### [x] Track: 宿敌系统完善 (Nemesis Enhancements)

- **Folder:** conductor/archive/nemesis_enhancements_20260113/
- **Description:** 实现宿敌 Hunter AI、进化逻辑闭环、线程安全保护以及针对性抗性进化。 (Completed on 2026-01-13)

---

### [x] Track: 传家宝系统 (Heirloom Vault)

- **Folder:** conductor/tracks/heirloom_vault/
- **Description:** 实现跨轮回装备继承系统，包括传家宝标记、宝库 UI 和属性动态压缩。 (Completed on 2026-01-13)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 0.5 week

---

### [x] Track: 无尽梦魇与腐化系统 (Eternal Nightmare & Corruption)

- **Folder:** conductor/tracks/eternal_nightmare/
- **Description:** 实现腐化系统 (CorruptionSystem) 用于动态难度调整，以及排行榜系统 (LeaderboardSystem) 用于记录挑战成绩。 (Completed on 2026-01-13)
- **Status:** COMPLETED
- **Priority:** MEDIUM
- **Estimated Time:** 1 week

---

### [x] Track: 传送门系统 (Portal System)

- **Folder:** conductor/archive/portal_system_20260113/
- **Description:** 实现回城门 (Town Portal) 和关卡出口机制。包含纯代码渲染的旋涡特效、施法吟唱逻辑、双向传送以及跨场景位置保存。 (Completed on 2026-01-13)

---

### [x] Track: 怪物 AI 行为扩展 (Monster AI Expansion)

- **Folder:** conductor/archive/monster_ai_expansion_20260113/
- **Description:** 实现 Support、Assassin、Tank 等怪物原型，以及 SoulLink、Avenger 等精英词缀，并完成 GameplayState 集成。 (Completed on 2026-01-13)

---

### [ ] Track: 游戏体验优化 (Polish & UX)

- **Folder:** conductor/tracks/polish_ux/
- **Description:** 实现音频系统 (AudioSystem)、成就系统 (AchievementSystem) 和新手引导 (TutorialSystem)。修复了背包掉落物品的标签持久化问题，以及城镇地图的视觉与边界安全问题。
- **Status:** IN PROGRESS
- **Priority:** MEDIUM
- **Estimated Time:** 1 week

---

### [ ] Track: 第二职业原型 (Second Class Prototype)

- **Folder:** conductor/tracks/second_class/
- **Description:** 抽象职业框架，设计并实现第二职业（如符咒师或炼体修士）。
- **Status:** PLANNED
- **Priority:** MEDIUM
- **Estimated Time:** 2 weeks

---

## [x] Track: GPU Flow Field Integration
- **Folder:** [./conductor/archive/gpu_flow_integration_20260113/](./conductor/archive/gpu_flow_integration_20260113/)
- **Description:** 将 GPU 流场 (SSBO) 集成到怪物寻路 AI 中。实现了 CPU 影子缓冲区同步、AI 分段降频更新以及基于实体池的休眠系统 (Dormancy System)，支持万级实体高性能寻路。 (Completed on 2026-01-13)

---

## [x] Track: 技能专精树重构 (Skill Spec Tree Refactoring)
- **Folder:** [./conductor/archive/skill_spec_tree_refactor_20260113/](./conductor/archive/skill_spec_tree_refactor_20260113/)
- **Description:** 重构技能专精树系统，引入 Behavior Injection 机制支持复杂的逻辑注入，并使用 bitset 优化了专精节点的激活状态追踪。 (Completed on 2026-01-14)

---

## [x] Track: Legendary Merging (传奇融合)
- **Folder:** [./conductor/archive/legendary_merging_20260114/](./conductor/archive/legendary_merging_20260114/)
- **Description:** 实现传奇融合系统 (Legendary Merging)，允许玩家将暗金装备与崇高装备融合以获得红色属性的传奇装备。 (Completed on 2026-01-14)
