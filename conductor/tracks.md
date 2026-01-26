# Project Tracks

This file tracks all major tracks for the project. Each track has its own detailed plan in its respective folder.

---

## [x] Track: Combat Popup Polish & Fix
- **Folder:** [./conductor/archive/combat_popup_fix_20260122/](./conductor/archive/combat_popup_fix_20260122/)
- **Description:** 修复战斗伤害弹出数字显示为中文的问题。重构了 PopupRenderer 的索引解析逻辑，修正了 Shader 中的 UV 垂直偏移与采样行，并升级了字形生成脚本（引入原生描边与软阴影）以提升清晰度 (Completed on 2026-01-22)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 0.5 day

---

## [x] Track: Combat Pipeline Refactor & Fix
- **Folder:** [./conductor/archive/combat_pipeline_fix_20260122/](./conductor/archive/combat_pipeline_fix_20260122/)
- **Description:** 修复 CombatBalanceTest 中的“零伤害”问题。重构 SkillRegistry 增加 Basic Attack (ID 0) 兜底逻辑，增加 DamagePipeline 的鲁棒性与日志诊断，并解决了 StatsSystem 静态缓存导致的测试环境污染 (Completed on 2026-01-22)
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 0.5 day

---

## [x] Track: Monster Level Scaling System
- **Folder:** [./conductor/archive/monster_level_scaling_20260121/](./conductor/archive/monster_level_scaling_20260121/)
- **Description:** 实现怪物随地图等级动态成长的机制。包含 HP/伤害指数成长曲线、等级同步 (玩家等级-5)、D3风格经验公式、100级后抗性递增、护甲线性减伤成长。参考 D2/D3/POE/Grim Dawn 的成长模型 (Completed on 2026-01-21)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1 day

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
- **Description:** 实现传奇和独特词缀的基础设施。将 AffixType 升级为 uint16_t，引入语义化 ID 区间锚点，移除冗余布尔标志，并在 ItemFactory 中实现了基于 JSON 的动态词缀加载与过滤逻辑 (Completed on 2026-01-17)
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
- **Description:** 对现有的 30+ 个测试文件进行物理重构与逻辑分类。将乱堆在根目录的文件整理进 unit/、integration/、functional/、performance/ 与 tech/ 子目录，并同步更新测试运行器，以提升维护效率和编译清晰度 (Completed on 2026-01-18)
- **Status:** COMPLETED
- **Priority:** MEDIUM
- **Estimated Time:** 0.5 day

---

## [x] Track: Equipment Tooltip Upgrade
- **Folder:** [./conductor/archive/ui_tooltip_upgrade_20260118/](./conductor/archive/ui_tooltip_upgrade_20260118/)
- **Description:** 升级装备信息面板（Tips）。在 Tooltip 中增加大尺寸装备图标显示，并在底部动态显示镶嵌孔数量（Socket Count），使用指定的 `#D0EFE8` 主题色 (Completed on 2026-01-18)
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
- **Description:** 优化怪物词缀系统架构。消除了 Nemesis 生成中的字符串硬编码，实现基于 Evolution Tier 的词缀数值动态缩放，修复宿敌 AI 在 Dormancy 逻辑下的不一致性，并补全了 Homing 与 Phase Shield 等核心缺失机制 (Completed on 2026-01-20)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 3 days

---

## [x] Track: Runtime String Dependency Elimination
- **Folder:** [./conductor/archive/string_dependency_elimination_20260120/](./conductor/archive/string_dependency_elimination_20260120/)
- **Description:** 消除核心系统的运行时字符串依赖。将 Biomes、Material Categories 与 Astrolabe Traits 迁移至枚举（BiomeID, MaterialCategory, TraitID），同步更新 JSON 数据，并重构相关系统（LevelManager, RunewordSystem, EnemySpawnSystem）以使用静态 Map 查找代替字符串比较，显著提升性能与类型安全性 (Completed on 2026-01-20)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1.5 days

---

## [x] Track: Performance Extreme Optimization (Multi-Phase)
- **Folder:** [./conductor/archive/performance_optimization/](./conductor/archive/performance_optimization/)
- **Description:** 多阶段极致性能优化策略，目标锁定 180 FPS (5.5ms 帧预算)。包含 5 个独立 Phase：Phase 1 (GPU-Driven MDI Rendering)、Phase 2 (EnTT Group Memory Optimization)、Phase 3 (Triple-Buffered Persistent Mapping)、Phase 4 (SIMD SpatialGrid Query)、Phase 5 (Branchless Combat Logic)。采用工业级手段彻底解决 Draw Call、Cache Miss、CPU-GPU 同步、空间查询密度和分支预测失败等性能瓶颈。
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 10-12 days

### Sub-Tracks:
| Phase | Track ID | Priority | Status |
|-------|----------|----------|--------|
| Phase 1 | `phase1_mdi_rendering` | P0 | Done |
| Phase 2 | `phase2_entt_group` | P1 | Done |
| Phase 3 | `phase3_triple_buffer` | P2 | Done |
| Phase 4 | `phase4_simd_spatial` | P2 | Done |
| Phase 5 | `phase5_branchless` | P3 | Done |

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
- **Description:** 移除 AttributePipeline 中废弃的 PhaseX 接口与其内部实现，并重构单元测试直接验证统一的 Calculate 逻辑，提升系统简洁性与可维护性 (Completed on 2026-01-23)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 2 hours

---

## [x] Track: Rendering Performance Validation
- **Folder:** [./conductor/archive/rendering_performance_validation/](./conductor/archive/rendering_performance_validation/)
- **Description:** Validation, benchmarking, and final audit of the rendering performance and sync optimizations. (Completed on 2026-01-23)
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

## [x] Track: Talent Constellation Fix
- **Folder:** [./conductor/archive/talent_constellation_fix_20260123/](./conductor/archive/talent_constellation_fix_20260123/)
- **Description:** Fixed AttributePipeline support for talent/constellation modifiers and activated BehaviorInjectionRegistry. (Completed on 2026-01-23)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 4.5 hours

---

## [x] Track: Talent Behavior Implementation
- **Folder:** [./conductor/archive/talent_behavior_implementation_20260123/](./conductor/archive/talent_behavior_implementation_20260123/)
- **Description:** Implemented core talent behaviors for Skill 1 (Flowing Thrust), Skill 2 (Rending Wave), and Skill 3 (Blade Formation), including projectile splitting, hovering hazards, and melee orbit modes. (Completed on 2026-01-23)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 8.5 hours

---

## [x] Track: AI FlowField Integration
- **Folder:** [./conductor/archive/ai_flowfield_integration_20260123/](./conductor/archive/ai_flowfield_integration_20260123/)
- **Description:** 统一了 GPU 流场与 AI 索敌机制。现在只有在 `CHASE` 等特定攻击状态下，Entity 才会利用 GPU 计算流场速度；`IDLE`/`PATROL` 等状态由 CPU 逻辑或摩擦力控制。消除了“越权索敌”问题，并定义了规范的 `GPUFlags::AIState` (Bit 8-15) 通信协议，移除了 CPU `applyFlowFieldCheck` 冗余计算 (Completed on 2026-01-23)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 4.5 hours

---

## [x] Track: Enemy Rendering Refactor (CPU Authority)
- **Folder:** [./conductor/archive/enemy-rendering-refactor/](./conductor/archive/enemy-rendering-refactor/)
- **Description:** 将敌人渲染架构由“GPU物理更新+SyncBack”重构为“CPU权威+GPU加速渲染”。解决了 N-2 帧同步延迟导致的视觉抖动与幽灵位移，确立了 CPU 为单一事实来源，提升了系统的稳定性与可维护性 (Completed on 2026-01-24)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1 day

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

## [x] Track: Inventory & Stash UX Fixes
- **Folder:** [./conductor/archive/fix-inventory-stash-ux/](./conductor/archive/fix-inventory-stash-ux/)
- **Description:** 修复了背包与仓库的交互体验问题。实现了个人仓库的正确初始化，解决了输入焦点泄露导致的误操作，并新增了仓库至背包的指定槽位拖拽与交换功能 (Completed on 2026-01-24)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 0.2 day

---

## [x] Track: Legendary Core Stash Unblock
- **Folder:** [./conductor/archive/fix-legendary-core-stash/](./conductor/archive/fix-legendary-core-stash/)
- **Description:** 解除仓库中 Material 类型物品（如传奇核心）的存储限制，允许玩家自由管理稀y材料 (Completed on 2026-01-24)
- **Status:** COMPLETED
- **Priority:** MEDIUM
- **Estimated Time:** 0.1 day

---

## [x] Track: Legendary Core Usage Logic
- **Folder:** [./conductor/archive/fix-legendary-core-usage/](./conductor/archive/fix-legendary-core-usage/)
- **Description:** 修复传奇核心的交互逻辑。将其类型修正为 Consumable，实现了右键“使用”直接打开传奇融合面板的功能，并适配了拖拽入孔逻辑 (Completed on 2026-01-24)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 0.1 day

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

## [x] Track: Rendering Layer Batching Optimization (Instanced UI Labels)
- **Folder:** [./conductor/archive/render_layer_batching_20260125/](./conductor/archive/render_layer_batching_20260125/)
- **Description:** 极致优化海量掉落物标签渲染性能。引入了 **GPU Instancing + SDF** 渲染方案，将数百个背景框合并为 1 次 Draw Call，并实现了 **Buffer Orphaning** 消除 GPU 同步阻塞。将渲染管线重构为分层批量模式（Geometry Pass & Text Pass），在 500+ 可见物体下将渲染开销从 10ms+ 降低至 < 0.5ms，稳定维持 400+ FPS。(Completed on 2026-01-25)
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 0.5 day

---

## [x] Track: Fix Dimensional Mosaic System & Performance Critical Recovery
- **Folder:** [./conductor/archive/fix-dimensional-mosaic-20260126/](./conductor/archive/fix-dimensional-mosaic-20260126/)
- **Description:** 修复维度图拼接界面的词缀显示逻辑，实现了数值聚合与描述模板系统。纠正了异界传送门的层级跳转逻辑，引入了碎片生命周期衰减机制，并完善了回城后场景状态的持久化恢复。特别修复了由词缀系统 O(N^2) 遍历导致的性能严重下降问题，通过 Spatial Grid 优化和 Staggered 生成逻辑将 FPS 从 40 恢复至 130+ (Completed on 2026-01-26)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 1-2 days

---

## [ ] Track: GPU Rendering Pipeline Refactor (Multi-Phase)
- **Folder:** [./conductor/tracks/rendering_pipeline_refactor/](./conductor/tracks/rendering_pipeline_refactor/)
- **Description:** 基于架构审计报告，系统性重构 GPU 渲染管线。消除魔法数字、统一 GL 抽象层、分解 GPUEntitySystem 职责、规范化 MDIRenderer，并解耦单例以提升可测试性。遵循 DOD 原则，目标实现代码一致性、性能提升、安全防护、高可用与低耦合。
- **Status:** IN_PROGRESS
- **Priority:** HIGH
- **Estimated Time:** 5-6 weeks (52-60h total)
- **Risk Level:** 🔴 HIGH (涉及核心渲染路径重构)

### Sub-Tracks:
| Track | Name | Priority | Status | Est. Time | Risk |
|-------|------|----------|--------|-----------|------|
| T1 | `render_constants` - 绑定常量标准化 | P0 | 🟢 已完成 | 4h | 低 |
| T2 | `gpu_utils_unification` - GPU 工具层统一 | P1 | 🟢 已完成 | 8h | 中 |
| T3 | `gpuEntitySystem_decomposition` - 职责分解 | P0 | 🟢 已完成 | 16-24h | 高 |
| T4 | `mdi_renderer_refactor` - MDI 渲染器规范化 | P1 | 🔵 待开始 | 8h | 中 |
| T5 | `singleton_decoupling` - 单例解耦与依赖注入 | P2 | 🔵 待开始 | 16h | 高 |

### 依赖关系:
```
T1 ──┬── T2 ──┬── T3 ──┬── T5
     │        │        │
     └────────┴── T4 ──┘
```

### 核心目标:
1. **代码一致性**: 统一 Buffer Binding 常量、GL 抽象层
2. **性能优化**: DOD 合规，20k 实体 Update < 2ms
3. **安全健壮性**: 消除魔法数字，类型安全绑定
4. **可测试性**: 支持 Mock 替换，单元测试覆盖
5. **可扩展性**: 模块化设计，为未来特性预留接口

---

## [ ] Track: Next Milestone (TBD)
- **Folder:** [./conductor/tracks/next-track/](./conductor/tracks/next-track/)
- **Description:** 规划下一个功能模块或优化方向。
- **Status:** PLANNED
- **Priority:** MEDIUM
- **Estimated Time:** TBD

---

## [x] Track: GPU Sprite Offload (MDI Texture Array)
- **Folder:** [./conductor/archive/gpu-sprite-offload_20260125/](./conductor/archive/gpu-sprite-offload_20260125/)
- **Description:** 将 CPU 精灵渲染卸载至 GPU。完成了 `GL_TEXTURE_2D_ARRAY` 与 MDI 管线的整合，实现了状态效果 (冰冻/燃烧) 的 Shader 级叠加，并清理了遗留的单体 Draw Call 路径。成功将 Draw Calls 降低至个位数，渲染效率大幅提升。(Completed on 2026-01-25)
- **Status:** COMPLETED
- **Priority:** CRITICAL
- **Estimated Time:** 2 days

---

## [x] Track: Render Items CPU Optimization (Beam & Cache)
- **Folder:** [./conductor/archive/render-items-cpu-optimization_20260125/](./conductor/archive/render-items-cpu-optimization_20260125/)
- **Description:** 优化掉落物品的 CPU 侧性能。通过 `VisibleItemCache` 消除 UISystem 中的冗余 $O(N)$ 遍历，并实现了光束 (Beam) 的 Instanced Rendering，显著降低了 Driver Overhead 和 Draw Calls。(Completed on 2026-01-25)
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 0.5 day