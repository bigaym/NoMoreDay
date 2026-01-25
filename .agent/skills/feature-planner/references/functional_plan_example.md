# Stash System 实施计划 (V1.1)

> **Track ID**: `stash_system_20260121`
> **依赖 Spec**: `spec.md` (V1.1 已确认)
> **预计工时**: 3-4 天

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|------|------|----------|------|
| **Phase 1** | 数据层 | 组件定义, DTO, 序列化逻辑 | ✅ 已完成 |
| **Phase 2** | 系统逻辑层 | StashSystem 核心 API | ✅ 已完成 |
| **Phase 3** | 持久化集成 | SaveManager 扩展 | ✅ 已完成 |
| **Phase 4** | UI 层 | UIStash 界面渲染与交互 | ✅ 已完成 |
| **Phase 5** | 城镇集成 | 仓库实体生成与交互 | ✅ 已完成 |
| **Phase 6** | 测试与打磨 | 单元测试, 集成测试, Bug 修复 | ✅ 已完成 |

---

## Phase 1: 数据层 (Data Layer)

### Task 1.1: 创建 StashTab 和 PersonalStashComponent
- [x] 定义 `StashTabType` 枚举
- [x] 定义 `StashTab` 结构体 (name, type, iconId, color, items[144])
- [x] 定义 `PersonalStashComponent` 结构体
- [x] 添加 `Common.hpp` 中的仓库配置常量 `StashConfig`

### Task 1.2: 创建 SharedStash 单例
- [x] 实现 `unlockNextTab(int& playerGold)` 方法
- [x] 实现基本的 `putItem`, `takeItem`, `getItem` 方法

### Task 1.3: 定义序列化 DTO
- [x] 定义 `SerializedStashSlot`, `SerializedStashTab`, `SerializedStash`

---

## Phase 2: 系统逻辑层 (System Logic Layer)

### Task 2.2: 实现物品转移逻辑
- [x] 实现 `transferItem()`: 边界检查、原子操作、类型检查
- [x] 实现 `canStoreItem()`: 拒绝 Material 类型
- [x] 实现 `quickDeposit()`: Ctrl+Click 快速存入（包含 Material 检查）

### Task 2.4: 实现批量操作和搜索
- [x] 实现 `sortTab()`: 稀有度/等级排序
- [x] 实现 `autoDeposit()`: 自动存入（跳过 Material）
- [x] 实现 `search()`: 关键词匹配，返回槽位列表

---

## Phase 3: 持久化集成 (Persistence Integration)

### Task 3.1: 扩展 SaveManager 快照逻辑
- [x] `CharacterSaveData` 添加 `personalStash`
- [x] `createSnapshot()` / `restoreFromSnapshot()` 适配

### Task 3.3: 实现 SharedStash 序列化
- [x] `global.json` 集成

---

## Phase 4: UI 层 (UI Layer)

### Task 4.3: 实现物品网格
- [x] 渲染 12x12 网格
- [x] 复用 `UITooltip`
- [x] 实现拖拽逻辑（调用 `StashSystem::transferItem`）

### Task 4.4: 实现快捷操作和搜索
- [x] Ctrl+Click 快速转移（Material 提示）
- [x] 搜索框功能集成

---

## Phase 5: 城镇集成 (Town Integration)

### Task 5.1: 城镇地图仓库实体生成
- [x] 在城镇地图固定位置生成仓库实体
- [x] 添加 `StashInteractableComponent`

### Task 5.3: 仓库交互逻辑
- [x] 距离检测与交互提示
- [x] 按 E 键或点击打开界面

---

## Phase 6: 测试与打磨 (Testing & Polish)

### Task 6.1: 单元测试
- [x] 覆盖转移、解锁、排序、Material 拒绝、搜索等核心逻辑

### Task 6.3: 性能基准
- [x] 验证界面打开延迟、标签页切换及满仓存盘耗时

---

*计划版本: 1.1*
*最后更新: 2026-01-21*