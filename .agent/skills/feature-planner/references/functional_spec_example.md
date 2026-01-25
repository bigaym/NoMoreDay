# Stash System 规格说明书 (V1.1)

> **Track ID**: `stash_system_20260121`
> **设计参考**: Grim Dawn 仓库系统
> **状态**: ✅ 已确认

---

## 1. 概述 (Overview)

为 NoMoreDay 设计一个多标签页仓库系统，支持角色专属仓库和跨角色共享仓库。玩家通过消耗金币解锁额外的仓库页，最大化存储空间管理效率。

### 1.1 核心特性
| 特性 | 规格 |
|------|------|
| **页面容量** | 每页 144 格 (12x12 网格，单格物品) |
| **最大标签页** | 10 个 / 仓库类型 |
| **仓库类型** | 角色专属仓库 (Personal) + 全局共享仓库 (Shared) |
| **解锁机制** | 消耗金币解锁新标签页 |
| **默认页数** | 初始解锁 1 页 (角色) + 1 页 (共享) |

### 1.2 设计目标
1. **扩展性**: 支持未来增加仓库类型 (如赛季仓库、公会仓库)。
2. **高性能**: 仓库数据采用按需加载策略，避免一次性读取全部物品。
3. **安全性**: 仓库操作需要原子性保证，防止物品复制/丢失。
4. **UI 友好性**: 支持标签页重命名、图标/颜色自定义。

### 1.3 物品存储限制
| 物品类型 | 可存入仓库 | 说明 |
|----------|-----------|------|
| 装备 (Equipment) | ✅ 是 | 武器、护甲、饰品等 |
| 消耗品 (Consumable) | ✅ 是 | 药水、卷轴等 |
| 符文 (Rune) | ✅ 是 | 用于符文语 |
| **材料 (Material)** | ❌ 否 | 存储在专用材料袋，不占用仓库空间 |
| 其他 | ✅ 是 | 背包容器、任务物品等 |

---

## 2. 数据模型 (Data Model)

### 2.1 核心数据结构

```cpp
// ============================================================
// StashComponent.hpp - 角色专属仓库组件 (挂载到 PlayerEntity)
// ============================================================
namespace NoMoreDay {

// 仓库标签页类型
enum class StashTabType : uint8_t {
    Normal = 0,       // 常规物品页
    Equipment,        // 装备专用页
    Material,         // 材料专用页
    Runeword,         // 符文/符文语专用页
    Custom            // 玩家自定义用途
};

// 单个标签页
struct StashTab {
    static constexpr int CAPACITY = 144;  // 12x12 网格

    std::string name;                     // 标签页名称 (可自定义, 最长 16 字符)
    StashTabType type = StashTabType::Normal;
    uint32_t iconId = 0;                  // 图标 ID (来自 icon_atlas)
    uint32_t color = 0xFFFFFFFF;          // RGBA 边框颜色
    
    std::array<entt::entity, CAPACITY> items;  // 物品槽位 (entt::null = 空)
    
    StashTab() { items.fill(entt::null); }
};

// 仓库组件 (角色专属, 挂载到 PlayerEntity)
struct PersonalStashComponent {
    static constexpr int MAX_TABS = 10;
    static constexpr int INITIAL_UNLOCKED = 1;
    
    int unlockedTabs = INITIAL_UNLOCKED;  // 已解锁页数
    std::vector<StashTab> tabs;           // 标签页列表 (size <= unlockedTabs)
    
    PersonalStashComponent() {
        tabs.resize(INITIAL_UNLOCKED);
        tabs[0].name = "Stash 1";
    }
};

} // namespace NoMoreDay
```

### 2.2 全局共享仓库

```cpp
// ============================================================
// SharedStash.hpp - 全局共享仓库 (账号级别, 非 ECS 组件)
// ============================================================
namespace NoMoreDay {

// 共享仓库单例 (与 SaveManager 类似的设计)
class SharedStash {
public:
    static constexpr int MAX_TABS = 10;
    static constexpr int INITIAL_UNLOCKED = 1;
    
    static SharedStash& Get() {
        static SharedStash instance;
        return instance;
    }
    
    // 解锁状态
    int unlockedTabs = INITIAL_UNLOCKED;
    std::vector<StashTab> tabs;
    
    // 操作接口
    bool unlockNextTab(int& playerGold);  // 尝试解锁下一页, 扣除金币
    entt::entity getItem(int tabIndex, int slotIndex);
    bool putItem(entt::registry& registry, int tabIndex, int slotIndex, entt::entity item);
    entt::entity takeItem(entt::registry& registry, int tabIndex, int slotIndex);
    
    // 持久化
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j, entt::registry& registry);
    
private:
    SharedStash() {
        tabs.resize(INITIAL_UNLOCKED);
        tabs[0].name = "Shared 1";
    }
};

} // namespace NoMoreDay
```

### 2.3 解锁费用表

标签页解锁采用递增式费用，鼓励玩家做出策略性选择。

```cpp
// Common.hpp 或 StashConfig.hpp
namespace StashConfig {
    // 第 N 页的解锁费用 (从 0 开始计数, 0=第一页=免费)
    // Index: 1    2     3      4       5       6       7        8        9
    // Cost:  1k   5k    20k    50k     100k    200k    500k     1M       2M
    constexpr std::array<int, 10> UNLOCK_COSTS = {
        0,         // Tab 0 (默认解锁)
        1'000,     // Tab 1
        5'000,     // Tab 2
        20'000,    // Tab 3
        50'000,    // Tab 4
        100'000,   // Tab 5
        200'000,   // Tab 6
        500'000,   // Tab 7
        1'000'000, // Tab 8
        2'000'000  // Tab 9
    };
    
    inline int getUnlockCost(int tabIndex) {
        if (tabIndex < 0 || tabIndex >= static_cast<int>(UNLOCK_COSTS.size())) return -1;
        return UNLOCK_COSTS[tabIndex];
    }
}
```

---

## 3. 系统架构 (Architecture)

### 3.1 系统层级

```
┌─────────────────────────────────────────────────────────────┐
│                        UI 层                                │
│   ┌─────────────────┐    ┌─────────────────┐               │
│   │   UIStash.cpp   │    │  UIStashTab.cpp │               │
│   │  (主界面渲染)   │    │ (标签页操作UI) │               │
│   └────────┬────────┘    └────────┬────────┘               │
│            │                      │                         │
├────────────┴──────────────────────┴─────────────────────────┤
│                     系统逻辑层                              │
│   ┌─────────────────────────────────────────────────────┐  │
│   │                  StashSystem.hpp                     │  │
│   │  - transferItem(src, srcSlot, dst, dstSlot)         │  │
│   │  - sortTab(tabIndex, SortMode)                      │  │
│   │  - unlockTab(StashType, playerGold&)                │  │
│   │  - autoDeposit(registry, LootFilter)                │  │
│   └─────────────────────────────────────────────────────┘  │
│                               │                             │
├───────────────────────────────┼─────────────────────────────┤
│                     数据/组件层                             │
│   ┌─────────────────┐    ┌─────────────────┐               │
│   │ PersonalStash   │    │  SharedStash    │               │
│   │   Component     │    │   (Singleton)   │               │
│   └────────┬────────┘    └────────┬────────┘               │
│            │                      │                         │
├────────────┴──────────────────────┴─────────────────────────┤
│                     持久化层                                │
│   ┌─────────────────────────────────────────────────────┐  │
│   │                  SaveManager                         │  │
│   │  - createSnapshot() 包含 PersonalStash               │  │
│   │  - global.json 包含 SharedStash 数据                 │  │
│   └─────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 核心操作 API

```cpp
// ============================================================
// StashSystem.hpp
// ============================================================
namespace NoMoreDay {

enum class StashType : uint8_t {
    Personal,
    Shared
};

enum class StashSortMode : uint8_t {
    ByRarity,
    ByLevel,
    ByType
};

class StashSystem {
public:
    // --- 物品转移 ---
    static bool transferItem(
        entt::registry& registry,
        StashType srcType, int srcTabIndex, int srcSlotIndex,
        StashType dstType, int dstTabIndex, int dstSlotIndex
    );
    
    // 背包 <-> 仓库快捷转移 (Ctrl+Click)
    static bool quickDeposit(entt::registry& registry, entt::entity item, StashType stashType);
    static bool quickWithdraw(entt::registry& registry, StashType stashType, int tabIndex, int slotIndex);
    
    // --- 标签页管理 ---
    static bool unlockTab(entt::registry& registry, StashType stashType); // 自动扣金币
    static bool renameTab(StashType stashType, int tabIndex, const std::string& newName);
    static bool setTabIcon(StashType stashType, int tabIndex, uint32_t iconId);
    static bool setTabColor(StashType stashType, int tabIndex, uint32_t color);
    
    // --- 批量操作 ---
    static void sortTab(entt::registry& registry, StashType stashType, int tabIndex, StashSortMode mode);
    static int autoDeposit(entt::registry& registry, StashType stashType, int tabIndex); // 自动存入背包所有非材料物品, 返回存入数量
    
    // --- 搜索 (仅限当前仓库类型的所有已解锁非空标签页) ---
    static std::vector<std::pair<int, int>> search(StashType stashType, const std::string& keyword); // 返回 (tabIndex, slotIndex) 列表
    
    // --- 物品检查 ---
    static bool canStoreItem(entt::registry& registry, entt::entity item); // 检查物品是否可存入仓库 (Material返回false)
    
    // --- 查询 ---
    static int getFreeSlotCount(StashType stashType, int tabIndex);
    static int getUnlockedTabCount(StashType stashType);
    static int getNextUnlockCost(StashType stashType);
    
private:
    static StashTab* getTab(entt::registry& registry, StashType type, int tabIndex);
};

} // namespace NoMoreDay
```

---

## 4. 持久化契约 (Persistence Contract)

### 4.1 角色存档扩展 (`slot_X.json`)

在现有 `CharacterSaveData` 中新增 `personal_stash` 字段：

```json
{
  "version": 2,
  "personal_stash": {
    "unlocked_tabs": 3,
    "tabs": [
      {
        "name": "Gear",
        "type": 1,
        "icon_id": 5,
        "color": 4294901760,
        "items": [
          { "slot": 0, "item": { /* SerializedItem */ } },
          { "slot": 15, "item": { /* SerializedItem */ } }
        ]
      }
    ]
  }
}
```

### 4.2 全局存档扩展 (`global.json`)

在现有全局存档中新增 `shared_stash` 字段：

```json
{
  "version": 2,
  "shared_stash": {
    "unlocked_tabs": 2,
    "tabs": [
      { "name": "Twink Gear", "type": 0, "icon_id": 0, "color": 4294967295, "items": [ ... ] }
    ]
  }
}
```

---

## 5. UI 规格 (UI Specification)

### 5.1 入口与交互

| 触发方式 | 行为 |
|----------|------|
| **点击仓库实体** | 在城镇地图中点击仓库箱子打开界面 |
| **Ctrl + 点击物品** | 在背包/仓库间快速转移物品 (Material 物品无效) |
| **右键标签页** | 打开标签页设置菜单 (重命名/图标/颜色) |

### 5.2 界面布局

```
┌─────────────────────────────────────────────────────────────────┐
│ [个人仓库] [共享仓库]              🔍 搜索     ⚙️ 设置    ❌  │
├─────────────────────────────────────────────────────────────────┤
│ ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬─────────────────────────┐ │
│ │ 1 │ 2 │ 3 │🔒│🔒│🔒│🔒│🔒│🔒│🔒│  ← 标签页选择器          │ │
│ └───┴───┴───┴───┴───┴───┴───┴───┴───┴─────────────────────────┘ │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │   ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐   12 列           │ │
│ │   │  │  │  │  │  │  │  │  │  │  │  │  │                   │ │
│ │   :  :  :  :  :  :  :  :  :  :  :  :  :       12 行       │ │
│ │   └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘                   │ │
│ └─────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│  [整理]  [全部存入]        已用: 48/144     💰 解锁下一页: 5000 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6. 风险与缓解 (Risks & Mitigations)

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| **物品复制** | 玩家利用崩溃复制物品 | 仓库操作采用"移除后再添加"的原子流程 |
| **存档膨胀** | 每个标签页最多存储 144 个物品 | 采用稀疏存储 (仅序列化非空槽位) |

---

## 7. 验收标准 (Acceptance Criteria)

- [x] 玩家可在城镇地图中与仓库实体交互打开仓库界面
- [x] 玩家可以通过金币解锁新的标签页 (费用按表收取)
- [x] 玩家可以拖拽物品在 背包 <-> 仓库 之间移动
- [x] **Material 类型物品无法存入仓库**，尝试时显示提示
- [x] Ctrl+点击 实现快速存取 (Material 物品除外)
- [x] **搜索功能** 可搜索当前仓库类型的所有已解锁非空标签页
- [x] 共享仓库数据在多角色之间持久化

---

## 8. 已确认设计决策 (Confirmed Decisions)

| 问题 | 决策 |
|------|------|
| **物品类型限制** | Material 类型物品 **不能** 存入仓库 |
| **访问方式** | 通过城镇地图中的仓库实体交互，而非快捷键 |

---

*规格版本: 1.1*
*最后更新: 2026-01-21*