# Feature Specification: Equipment Level System

## 1. Overview
引入装备等级（Item Level）系统，使物品属性随等级线性成长，并添加角色装备等级限制。这是构建 RPG 深度数值体系的基石。

## 2. Core Mechanics

### 2.1 Item Level (ilvl)
- **定义**: `ItemComponent` 新增字段 `int itemLevel`。
- **范围**: 1 ~ 100。
- **来源**: 
    - 掉落时由 `dropLevel` 决定（通常等于 `AreaLevel` 或 `MonsterLevel`，但不超过地图等级上限）。
    - 商店/打造时的目标等级。

### 2.2 Stat Scaling (数值成长)
- **基准**: 
    - Logically, "Base Item" stats defined in factory are for Level 1.
    - In practice, we update the factory to apply a multiplier upon creation.
- **公式**:
    $$ Multiplier = 1.0 + (ItemLevel - 1) \times \frac{1.5}{99} $$
    - Level 1: $1.0 + 0 = 1.0\times$ (Base)
    - Level 100: $1.0 + 99 \times 0.01515... = 2.5\times$ (Max)
- **受影响属性**:
    - `attack` (Weapon)
    - `defense` (Armor)
    - `value` (Gold Value)

### 2.3 Equip Requirements (装备限制)
- **规则**: `PlayerLevel >= ItemLevel`
- **行为**:
    - 当尝试装备（拖拽/右键）时，若等级不足，操作失败并提示 warning。
    - TODO: 已装备物品因某种原因导致等级不足（如洗点导致降级？暂无降级机制，忽略）时不强制卸下，但暂时只做装备时的入口检查。

### 2.4 UI / Tooltips
- **Tooltip Display**:
    - 新增一行: `物品等级: [Level]`
    - **Color Logic**:
        - IF `PlayerLevel >= ItemLevel`: 显示为绿色 (e.g., `GREEN` or `LIME`).
        - IF `PlayerLevel < ItemLevel`: 显示为红色 (`RED`)，提示无法装备。

## 3. Data Structures

### 3.1 Component Changes
**File**: `src/game/components/ItemComponent.hpp`

```cpp
struct ItemComponent {
    // ... existing fields ...
    int itemLevel = 1; // [NEW] Default to level 1
    // ...
};

// Update to_json / from_json manually!
```

## 4. Systems Integration

### 4.1 ItemFactory (`src/game/systems/item/ItemFactory.cpp`)
- Update `createRandomLoot` (and internal helpers) to:
    1. Accept target level.
    2. Assign `item.itemLevel`.
    3. Calculate and apply Scaling Multiplier to `attack`, `defense`, `value`.

### 4.2 InventorySystem (`src/game/systems/item/InventorySystem.cpp`)
- Update `equipItem`:
    - Get Player's `StatsComponent` (or level source).
    - Check `PlayerLevel >= Item.itemLevel`.
    - Return `false` if failed.

### 4.3 UIRenderer (`src/engine/render/UIRenderer.cpp`)
- Update `DrawTooltip`:
    - Retrieve Player Entity (passed via registry/context).
    - Compare levels.
    - Render formatted text.

## 5. Persistence
- Ensure `itemLevel` is saved/loaded in `ItemComponent` JSON serialization.
