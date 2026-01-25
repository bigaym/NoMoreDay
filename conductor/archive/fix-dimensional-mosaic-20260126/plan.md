# Implementation Plan: Fix Dimensional Mosaic System

## Phase 1: 词缀引擎与聚合逻辑 (Affix Engine & Aggregation)
**Goal**: 实现词缀的数值合并显示，消除 UI 冗余。

- [x] **Task 1: 聚合逻辑开发**
  - 在 `MapAffixCalculator` 中实现 `AggregateAffixes` 静态方法。
  - 逻辑：按 `MapAffixType` 对 `explicitAffixes` 进行分组求和。
- [x] **Task 2: 描述模板系统**
  - 在 `MapAffixRegistry.hpp` 的 `MapAffixDefinition` 中添加 `descriptionTemplate` (如: "怪物密度: +{value}%")。
  - 实现 `FormatDescription` 方法处理数值百分比转换。
- [x] **Task 3: 字体与特殊字符修复**
  - 在 `UISystem.cpp` 中添加 `•` (0x2022) 等符号。

## Phase 2: UI 升级与体验优化 (UI & UX Polish)
**Goal**: 在编辑器和战斗内显示清晰的聚合信息。

- [x] **Task 4: 异界演化控制器 (Rift Evolution Controller)**
  - 在 `PortalSystem.cpp` 中实现 `AdvanceRiftLayer` 逻辑。
  - **Task 4.1: 碎片生命周期管理**：实现对 `remainingLayers` 的递减和自动移除（碎片的“破碎”逻辑）。
  - **Task 4.2: 属性实时重算**：在切换场景前，确保 `ActiveDimensionalState` 中的聚合词缀已根据残余碎片更新。
  - **Task 4.3: 场景重定向**：如果无可用碎片，将传送门目的地重定向回城镇。
- [x] **Task 5: 深度加成与 UI 反馈**
  - 实现深度倍率公式（Depth Scaling）。
  - 更新 Minimap 展示“异界 [层级]”和“总加成倍率”。
- [x] **Task 6: 快捷键优化**
  - 修正 ESC 键逻辑，确保在任何交互状态下都能正确退出。

## Phase 3: 持久化与场景恢复 (Persistence & Recovery)
**Goal**: 确保回城、重开游戏后，异界进度和数值不丢失。

- [x] **Task 6: 数据序列化补完**
  - 在 `ActiveDimensionalState.hpp` 中完善 JSON 序列化宏。
  - **Task 6.1**: 确保 `explicitAffixes` 和 `mosaicGrid` 被正确保存。
  - **Task 6.2**: 增加 `killCounter` 字段并同步至存档。
- [x] **Task 7: SceneManager 恢复逻辑修复**
  - 修改 `SceneManager::ApplyLoadedLevel`。
  - **修复逻辑**：进入异界地图时，优先检查 `ctx()` 是否已有 `ActiveDimensionalState`。如果有，则跳过初始化，直接应用现有词缀。
- [x] **Task 8: 击杀计数器同步**
  - 在 `EnemyDeathSystem` 中，如果当前处于异界状态，实时更新 `ActiveDimensionalState.killCounter`。
  - 确保该数值在 UI Overlay 中正确显示。

## Verification
- Run `tests/unit/MapAffixSystemTest.cpp` (if relevant or updated).
- Manual playtest:
  1. Open Mosaic Editor, check Tooltips.
  2. Generate Map, enter Rift.
  3. Clear Rift Layer 1, enter portal, check if it goes to Layer 2 directly.
  4. At Layer 2, teleport to Town, then return. Verify all stats are preserved.
