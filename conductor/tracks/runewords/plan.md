# Track Plan: 符文语系统 (Runewords System)

## 1. 目标 (Goal)
实现类似 Diablo 2 的完整符文语系统，作为 Phase 9 (终局装备深度) 的核心内容。系统由 33 个基础符文和特定的符文组合 (Runewords) 构成，提供深度的物品构建与收集玩法。

## 2. 技术规格 (Tech Specs)
- **符文定义**: 
    - 使用 `assets/data/runes.json` 定义 1-33 号符文的基础属性。
    - 每颗符文根据镶嵌部位 (Weapon/Armor/Shield) 提供不同的 Affix。
- **符文语定义**:
    - 使用 `assets/data/runewords.json` 定义配方序列、底材要求和结果属性。
- **插槽系统**: `ItemComponent.sockets` 存储已镶嵌的符文实体。需要确保序列化时能正确保存和恢复这些实体。
- **镶嵌逻辑**: 
    - `CraftingSystem::socketRune` 处理镶嵌操作。
    - `RunewordSystem` (新系统) 专门负责监听镶嵌事件，检测符文语是否激活，并应用/移除符文语特有的属性与 Tag。
- **数据结构**:
    - 符文物品本身是特殊的 `ItemType::Material` (或 `Rune`)，但具有 `ItemStats`。
    - 符文语状态通过 ItemComponent 的 `rarity` (变为 Legendary/Runeword) 和名称变化来体现。

## 3. 任务清单 (Tasks)

### Phase 1: 数据定义与资产 (Data & Assets)
- [x] **Task 1.1**: 创建 `assets/data/runes.json`，根据设计文档录入 1-33 号符文的数据 (包括 Weapon/Armor/Shield 的不同效果)。
- [x] **Task 1.2**: 创建 `assets/data/runewords.json`，录入首批经典的符文语配方 (如 Stealth, Spirit, Grief, Enigma)。
- [x] **Task 1.3**: 更新 `MaterialRegistry` 或 `ItemFactory` 以支持加载和生成这些特殊的符文物品。

### Phase 2: 核心逻辑实现 (Core Logic)
- [x] **Task 2.1**: 实现 `RunewordSystem` 的基础架构。
    - 定义 `checkRuneword(ItemComponent& item)` 函数，用于比对当前孔内符文序列。
    - 实现 `applyRuneword` 和 `removeRuneword` 逻辑。
- [x] **Task 2.2**: 升级 `CraftingSystem` 的镶嵌逻辑。
    - 确保符文可以从 Inventory 镶嵌到 Item。
    - 镶嵌后触发 `RunewordSystem` 的检查。
- [x] **Task 2.3**: 处理属性叠加与冲突。
    - 确保符文的基础属性正确应用到装备上。
    - 确保符文语被激活时，额外的属性正确叠加。

### Phase 3: UI 与可视化 (UI & Visualization)
- [ ] **Task 3.1**: 更新物品 Tooltip UI。
    - 显示孔数和已镶嵌的符文 (显示符文名称，如 "Tal Eth")。
    - 如果激活了符文语，显示金色的符文语名称 (如 "Stealth") 而非原物品名。
- [ ] **Task 3.2**: 符文物品的图标与渲染支持。

### Phase 4: 测试与验证 (Validation)
- [x] **Task 4.1**: 编写 `TestRunewordSystem.cpp`。
    - 测试用例：正确顺序激活符文语。
    - 测试用例：错误顺序不激活。
    - 测试用例：底材类型不匹配不激活。
    - 测试用例：移除符文/破坏物品时的状态重置。

## 4. 定义完成 (Definition of Done)
- [x] 玩家获得 1-33 号符文，且每个符文都有正确的 Tips 说明其镶嵌效果。
- [x] 将 "Tal" 和 "Eth" 按顺序放入 2 孔甲，物品名称变为 "Stealth"，并获得对应属性。
- [x] 将 "Tal" 和 "Eth" 放入 2 孔剑，**不**激活 "Stealth" (底材不符)。
- [x] 符文语装备的数据能正确地 Save/Load。
