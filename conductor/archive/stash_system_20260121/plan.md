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

**目标**: 定义仓库相关的 ECS 组件和数据传输对象 (DTO)。

### Task 1.1: 创建 StashTab 和 PersonalStashComponent
**文件**: `src/game/components/StashComponent.hpp`

- [ ] 定义 `StashTabType` 枚举
- [ ] 定义 `StashTab` 结构体 (name, type, iconId, color, items[144])
- [ ] 定义 `PersonalStashComponent` 结构体 (unlockedTabs, tabs vector)
- [ ] 添加 `Common.hpp` 中的仓库配置常量 `StashConfig`

**代码骨架**:
```cpp
// src/game/components/StashComponent.hpp
#pragma once
#include <array>
#include <string>
#include <vector>
#include <entt/entt.hpp>

namespace NoMoreDay {

enum class StashTabType : uint8_t { Normal, Equipment, Material, Runeword, Custom };

struct StashTab {
    static constexpr int CAPACITY = 144;
    std::string name;
    StashTabType type = StashTabType::Normal;
    uint32_t iconId = 0;
    uint32_t color = 0xFFFFFFFF;
    std::array<entt::entity, CAPACITY> items;
    StashTab() { items.fill(entt::null); }
};

struct PersonalStashComponent {
    static constexpr int MAX_TABS = 10;
    static constexpr int INITIAL_UNLOCKED = 1;
    int unlockedTabs = INITIAL_UNLOCKED;
    std::vector<StashTab> tabs;
    PersonalStashComponent() { tabs.resize(INITIAL_UNLOCKED); tabs[0].name = "Stash 1"; }
};

} // namespace NoMoreDay
```

**验证**: 编译通过，无警告。

---

### Task 1.2: 创建 SharedStash 单例
**文件**: `src/engine/persistence/SharedStash.hpp`, `src/engine/persistence/SharedStash.cpp`

- [ ] 定义 `SharedStash` 单例类 (参考 `SaveManager`)
- [ ] 实现 `unlockNextTab(int& playerGold)` 方法
- [ ] 实现基本的 `putItem`, `takeItem`, `getItem` 方法
- [ ] 暂留 `toJson()` / `fromJson()` 接口 (Phase 3 实现)

**验证**: 单元测试 `SharedStashBasicTest` 验证解锁和物品操作。

---

### Task 1.3: 定义序列化 DTO
**文件**: `src/game/data/StashData.hpp`

- [ ] 定义 `SerializedStashSlot` 结构体 (slotIndex, item)
- [ ] 定义 `SerializedStashTab` 结构体 (name, type, iconId, color, items vector)
- [ ] 定义 `SerializedStash` 结构体 (unlockedTabs, tabs vector)
- [ ] 添加 nlohmann json 序列化宏

**验证**: 编译通过，JSON 往返测试通过。

---

### Task 1.4: 添加解锁费用配置
**文件**: `src/game/components/Common.hpp`

- [ ] 在 `namespace StashConfig` 中添加 `UNLOCK_COSTS` 常量数组
- [ ] 添加 `getUnlockCost(int tabIndex)` 辅助函数

### Task 1.5: 定义仓库交互组件 (城镇实体)
**文件**: `src/game/components/StashComponent.hpp`

- [ ] 定义 `StashInteractableComponent` 结构体 (type: StashType)
- [ ] 定义 `StashPlaceholderRender` 结构体 (临时占位渲染: 64x64 矩形, BROWN 颜色)

**代码骨架**:
```cpp
// 城镇仓库实体组件
struct StashInteractableComponent {
    StashType type = StashType::Personal;
};

// 临时占位渲染 (后续替换为 Sprite)
struct StashPlaceholderRender {
    static constexpr float WIDTH = 64.0f;
    static constexpr float HEIGHT = 64.0f;
    // 使用 Raylib 的 BROWN 颜色
};
```

---

## Phase 2: 系统逻辑层 (System Logic Layer)

**目标**: 实现仓库操作的核心逻辑。

### Task 2.1: 创建 StashSystem 基础框架
**文件**: `src/game/systems/item/StashSystem.hpp`, `src/game/systems/item/StashSystem.cpp`

- [ ] 定义 `StashType` 枚举 (Personal, Shared)
- [ ] 定义 `StashSortMode` 枚举
- [ ] 实现 `getTab(registry, type, tabIndex)` 辅助方法
- [ ] 实现 `getFreeSlotCount`, `getUnlockedTabCount`, `getNextUnlockCost` 查询方法

---

### Task 2.2: 实现物品转移逻辑
**文件**: `src/game/systems/item/StashSystem.cpp`

- [ ] 实现 `transferItem(srcType, srcTab, srcSlot, dstType, dstTab, dstSlot)`
  - 边界检查 (槽位有效性, 标签页解锁状态)
  - **物品类型检查**: 调用 `canStoreItem()` 拒绝 Material 类型
  - 原子操作 (先 take, 后 put; 失败时回滚)
  - 支持同类型内移动和跨类型移动

- [ ] 实现 `canStoreItem(registry, item)` - **新增**
  - 获取 `ItemComponent`
  - 检查 `item.type == ItemType::Material` 返回 false
  - 其他类型返回 true

- [ ] 实现 `quickDeposit(item, stashType)` - Ctrl+Click 快速存入
  - **先调用 `canStoreItem()`**, Material 物品返回 false
  - 自动查找第一个空槽位
  - 优先当前活动标签页，其次遍历已解锁页

- [ ] 实现 `quickWithdraw(stashType, tabIndex, slotIndex)` - Ctrl+Click 快速取出
  - 将物品放入背包第一个空槽位
  - 背包满时返回 false

**验证**: 单元测试 `StashTransferTest` 覆盖正常转移、边界情况、回滚场景。

---

### Task 2.3: 实现标签页管理
**文件**: `src/game/systems/item/StashSystem.cpp`

- [ ] 实现 `unlockTab(registry, stashType)`
  - 检查金币是否足够
  - 扣除金币 (从 `InventoryComponent::gold`)
  - 增加 `unlockedTabs` 计数
  - 创建新的空 `StashTab` 并初始化默认名称

- [ ] 实现 `renameTab`, `setTabIcon`, `setTabColor`
  - 名称校验 (长度 1-16, 无非法字符)

---

### Task 2.4: 实现批量操作和搜索
**文件**: `src/game/systems/item/StashSystem.cpp`

- [ ] 实现 `sortTab(registry, stashType, tabIndex, mode)`
  - 构建稀有度/等级/类型比较器 (**不支持按名称排序**)
  - 利用 `std::sort` 对 items 数组排序 (entt::null 移到末尾)

- [ ] 实现 `autoDeposit(registry, stashType, tabIndex)`
  - 遍历背包所有物品
  - **跳过 Material 类型物品** (`canStoreItem()` 检查)
  - 逐个调用 `quickDeposit`
  - 返回成功存入的物品数量

- [ ] 实现 `search(stashType, keyword)` - **新增**
  - 仅搜索 **当前 stashType** (不跨仓库)
  - 遍历所有 **已解锁且非空** 的标签页
  - 匹配物品名称、词缀名称、底材名称
  - 返回 `vector<pair<tabIndex, slotIndex>>`

---

## Phase 3: 持久化集成 (Persistence Integration)

**目标**: 将仓库数据集成到现有存档系统。

### Task 3.1: 扩展 SaveManager 快照逻辑
**文件**: `src/engine/persistence/SaveManager.cpp`

- [ ] 在 `CharacterSaveData` 结构体中添加 `std::optional<SerializedStash> personalStash`
- [ ] 修改 `createSnapshot()`:
  - 获取 `PersonalStashComponent`
  - 遍历 tabs, 遍历 items, 构建 `SerializedStash` DTO
  - 稀疏存储: 只序列化 `entity != entt::null` 的槽位

- [ ] 修改 `restoreFromSnapshot()`:
  - 检测 `personalStash.has_value()`
  - 如不存在 (旧存档), 创建默认空仓库
  - 如存在, 调用 `ItemFactory::restoreItem` 重建物品实体
  - 填充 `PersonalStashComponent`

---

### Task 3.2: 扩展全局存档
**文件**: `src/engine/persistence/GlobalSaveData.hpp` (新建或扩展)

- [ ] 定义 `GlobalSaveData` 结构体 (如果尚未存在)
- [ ] 添加 `SerializedStash sharedStash` 字段
- [ ] 在 `SaveManager` 或独立的 `GlobalSaveManager` 中实现:
  - `saveGlobal()`
  - `loadGlobal()`

---

### Task 3.3: 实现 SharedStash 序列化
**文件**: `src/engine/persistence/SharedStash.cpp`

- [ ] 实现 `toJson()`:
  - 遍历 tabs, 构建 JSON 对象
  - 物品使用 `SerializedItem` 的 `to_json`

- [ ] 实现 `fromJson(const nlohmann::json& j, entt::registry& registry)`:
  - 解析 JSON
  - 调用 `ItemFactory::restoreItem` 重建物品

---

### Task 3.4: 集成触发点
**文件**: `src/app/Game.cpp`, `src/game/states/MainMenuState.cpp`

- [ ] 在游戏初始化时加载 `global.json` 并初始化 `SharedStash`
- [ ] 在角色加载后触发 `PersonalStashComponent` 恢复
- [ ] 在游戏退出/回城时保存 `SharedStash` 到 `global.json`

---

## Phase 4: UI 层 (UI Layer)

**目标**: 实现仓库界面的渲染和交互。

### Task 4.1: 创建 UIStash 基础框架
**文件**: `src/game/systems/ui/UIStash.hpp`, `src/game/systems/ui/UIStash.cpp`

- [ ] 定义静态变量: `m_isVisible`, `m_activeStashType`, `m_activeTabIndex`
- [ ] 实现 `Toggle()`, `IsVisible()`, `SetStashType()`, `SetTab()`
- [ ] 实现 `Update(registry)` - 处理输入 (快捷键, 点击)
- [ ] 实现 `Draw(registry)` - 渲染主窗口框架

---

### Task 4.2: 实现标签页选择器
**文件**: `src/game/systems/ui/UIStash.cpp`

- [ ] 渲染标签页按钮行 (已解锁页 + 锁定页)
- [ ] 锁定页显示 🔒 图标, 点击弹出解锁确认
- [ ] 当前选中页高亮样式
- [ ] 右键标签页打开上下文菜单 (重命名/图标/颜色)

---

### Task 4.3: 实现物品网格
**文件**: `src/game/systems/ui/UIStash.cpp`

- [ ] 渲染 12x12 网格 (每格 48x48 像素)
- [ ] 遍历 `StashTab::items`, 绘制物品图标
- [ ] 复用 `UITooltip` 显示物品悬停提示
- [ ] 实现拖拽逻辑:
  - 开始拖拽: 记录源槽位, 显示拖拽物品
  - 拖拽中: 高亮目标槽位
  - 释放: 调用 `StashSystem::transferItem`

---

### Task 4.4: 实现快捷操作和搜索
**文件**: `src/game/systems/ui/UIStash.cpp`

- [ ] 实现 Ctrl+Click 快速转移
  - 调用 `canStoreItem()` 检查
  - Material 物品显示 "**材料不能存入仓库**" 提示
  - 调用 `quickDeposit`/`quickWithdraw`

- [ ] 实现 "整理" 按钮 (调用 `sortTab`)
- [ ] 实现 "全部存入" 按钮 (调用 `autoDeposit`, 自动跳过 Material)

- [ ] 实现搜索功能 - **新增**
  - 搜索框输入
  - 调用 `StashSystem::search(currentStashType, keyword)`
  - 高亮匹配物品, 非匹配物品半透明
  - 点击搜索结果跳转到对应标签页

- [ ] 显示:
  - 已用/总容量: `48/144`
  - 解锁下一页费用: `💰 解锁下一页: 5,000`

---

### Task 4.5: 集成到游戏状态 (仅城镇)
**文件**: `src/game/states/GameplayState.cpp`

- [ ] **移除** 快捷键 `I` 打开仓库 (通过城镇实体交互)
- [ ] 在 `Update()` 中检测仓库交互 (StashInteractableComponent)
- [ ] 在 `Render()` 中调用 `UIStash::Draw`
- [ ] 确保仓库界面与背包界面不冲突 (同时打开或互斥)

---

## Phase 5: 城镇集成 (Town Integration) - **新增**

**目标**: 在城镇地图中生成可交互的仓库实体。

### Task 5.1: 城镇地图仓库实体生成
**文件**: `src/game/systems/world/TownMapSystem.cpp` (或相关地图生成文件)

- [ ] 在城镇地图初始化时创建 **个人仓库** 实体
  - 添加 `PositionComponent` (城镇固定位置, 如 x=200, y=300)
  - 添加 `StashInteractableComponent { .type = StashType::Personal }`
  - 添加 `StashPlaceholderRender` (临时矩形渲染)

- [ ] 在城镇地图初始化时创建 **共享仓库** 实体
  - 位置与个人仓库相邻 (如 x=280, y=300)
  - 添加 `StashInteractableComponent { .type = StashType::Shared }`

### Task 5.2: 仓库实体渲染
**文件**: `src/engine/render/RenderSystem.cpp` (或相关渲染逻辑)

- [ ] 检测 `StashPlaceholderRender` 组件
- [ ] 渲染 64x64 棕色矩形 (临时占位符)
- [ ] 可选: 渲染文字标签 "Stash" / "Shared"

### Task 5.3: 仓库交互逻辑
**文件**: `src/game/systems/InteractionSystem.cpp` (或 `GameplayState.cpp`)

- [ ] 检测玩家与 `StashInteractableComponent` 实体的距离
- [ ] 距离 < 100 像素时显示交互提示 ("Press E to open stash")
- [ ] 按下 E 键或点击时:
  - 获取 `StashInteractableComponent.type`
  - 调用 `UIStash::Open(type)`

**验证**: 能在城镇看到两个棕色矩形, 点击可打开对应仓库。

---

## Phase 6: 测试与打磨 (Testing & Polish)

**目标**: 确保系统稳定、无 Bug、性能达标。

### Task 6.1: 单元测试
**文件**: `tests/unit/StashSystemTest.hpp`

- [ ] `TransferItem_BasicMove_Success`
- [ ] `TransferItem_CrossStash_Success`
- [ ] `TransferItem_LockedTab_Failure`
- [ ] `TransferItem_FullSlot_Swap`
- [ ] **`TransferItem_Material_Rejected`** - Material 物品被拒绝
- [ ] **`CanStoreItem_Material_ReturnsFalse`** - 新增
- [ ] `UnlockTab_InsufficientGold_Failure`
- [ ] `UnlockTab_MaxTabs_Failure`
- [ ] `SortTab_ByRarity_CorrectOrder`
- [ ] `AutoDeposit_SkipsMaterial_Success` - **新增**
- [ ] **`Search_CurrentStashOnly_Success`** - 新增

---

### Task 6.2: 持久化测试
**文件**: `tests/integration/StashPersistenceTest.hpp`

- [ ] `SaveLoad_PersonalStash_RoundTrip`
- [ ] `SaveLoad_SharedStash_RoundTrip`
- [ ] `SaveLoad_LegacySave_CreatesDefaultStash`
- [ ] `SaveLoad_ItemIntegrity_NoLoss`

---

### Task 6.3: 性能基准
**文件**: `tests/performance/StashBenchmark.hpp`

- [ ] `OpenStash_Latency` - 目标 < 50ms
- [ ] `SwitchTab_Latency` - 目标 < 16ms
- [ ] `FullStashSave_Duration` - 目标 < 500ms (异步)
- [ ] `FullStashLoad_Duration` - 目标 < 200ms

---

### Task 6.4: Bug 修复和 UI 打磨

- [ ] 修复测试中发现的问题
- [ ] 添加动画效果 (标签页切换滑动, 物品放入反馈)
- [ ] 音效集成 (打开仓库, 放入物品, 解锁标签页)

---

## 文件清单 (Deliverables)

| 文件路径 | 类型 | Phase |
|----------|------|-------|
| `src/game/components/StashComponent.hpp` | 新建 | 1 |
| `src/engine/persistence/SharedStash.hpp` | 新建 | 1 |
| `src/engine/persistence/SharedStash.cpp` | 新建 | 1, 3 |
| `src/game/data/StashData.hpp` | 新建 | 1 |
| `src/game/components/Common.hpp` | 修改 | 1 |
| `src/game/systems/item/StashSystem.hpp` | 新建 | 2 |
| `src/game/systems/item/StashSystem.cpp` | 新建 | 2 |
| `src/game/data/SaveData.hpp` | 修改 | 3 |
| `src/engine/persistence/SaveManager.cpp` | 修改 | 3 |
| `src/game/systems/ui/UIStash.hpp` | 新建 | 4 |
| `src/game/systems/ui/UIStash.cpp` | 新建 | 4 |
| `src/game/states/GameplayState.cpp` | 修改 | 4, 5 |
| `src/game/systems/world/TownMapSystem.cpp` | 修改 | 5 |
| `src/engine/render/RenderSystem.cpp` | 修改 | 5 |
| `tests/unit/StashSystemTest.hpp` | 新建 | 6 |
| `tests/integration/StashPersistenceTest.hpp` | 新建 | 6 |
| `tests/performance/StashBenchmark.hpp` | 新建 | 6 |

---

## 依赖关系图

```
Phase 1 ──┬──> Phase 2 ──┬──> Phase 4 ──┬──> Phase 5
          │              │              │
          └──> Phase 3 ──┘              │
                                        │
                                        ▼
                                    Phase 6
```

- **Phase 2** 依赖 **Phase 1** (需要组件定义)
- **Phase 3** 依赖 **Phase 1** (需要 DTO 定义)
- **Phase 4** 依赖 **Phase 2** 和 **Phase 3** (需要系统 API 和持久化逻辑)
- **Phase 5** 依赖 **Phase 1** 和 **Phase 4** (需要仓库组件和 UI)
- **Phase 6** 在所有开发阶段完成后进行

---

*计划版本: 1.1*
*最后更新: 2026-01-21*
