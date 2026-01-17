# 装备分解与词缀碎片系统 (Equipment Salvage & Affix Shards) - Optimized v2

## 1. 核心概念 (Core Concept)
允许玩家分解高稀有度的非独特(Non-Unique)装备，提取其核心力量，转化为与词缀类型一一对应的"词缀碎片"。
由于系统现已全面转向高性能 **枚举 (Enum-based)** 架构，本设计将充分利用 `AffixType` 实现零维护成本的材料映射。

## 2. 用户故事 (User Story)
- 玩家刷到了由于数值不佳而无法直接使用的传奇装备。
- 玩家一键分解所有非特定的优质传奇，回收碎片。
- 系统极速处理（支持批量），碎片直接进入 `MaterialBankComponent`。
- 碎片用于在 `CraftingSystem` 中将指定词缀强行提升至更高 Tier。

## 3. 性能优化规则 (Hardware-Accelerated Engineering)

### 3.1 零表映射 (Zero-Table Mapping)
摒弃繁琐的 JSON 查找表，使用确定性算法映射材料 ID：
- **公式**: `ShardMaterialID = 4000 + static_cast<uint16_t>(AffixType)`
- **优点**: 
    - 内存零开销，查找速度 $O(1)$。
    - 开发者添加新词缀时，无需手动同步材料表，系统自动支持。
    - 范围预留: `4000 - 4999`。

### 3.2 高效内存布局
- **预览模式**: `std::vector<MaterialEntry> PreviewSalvage(std::span<const Affix> affixes)`
- **处理路径**: 剔除 `std::string` 依赖，直接处理 POD 结构的 `Affix` 数据。

### 3.3 产出逻辑 (Deterministic Logic)
对物品的 `affixes` 列表（排除 `implicits`）进行处理：

| 词缀阶级 (Tier) | 碎片产出算法 (Shard Count) |
| :--- | :--- |
| **Tier 1 - 3** | `fast_rand(0, Tier)` |
| **Tier 4 - 7** | `fast_rand(Tier - 3, Tier)` |

*注：传奇词缀 (`IsLegendaryAffix() == true`) 产出特殊的 "Legendary Essence" (ID: 4999)。*

## 4. 数据契约 (Data Contract)

### 4.1 自动生成的 Material 定义
`assets/data/materials.json` 应包含以下模式的条目：
```json
{
    "id": 4000,
    "name": "Strength Shard",
    "description": "Powerful residue containing Strength.",
    "category": "Affix Shard",
    "rarity": "Magic",
    "icon": "icon_shard_0"
}
```
*提示: 图标应遵循 `icon_shard_{AffixType_int}` 命名规范或使用统一的着色器渲染。*

## 5. 系统集成 (System Integration)

### 5.1 `SalvageSystem` API
```cpp
namespace NoMoreDay::SalvageSystem {
    // 基础校验
    bool CanSalvage(const ItemComponent& item);
    
    // 极速预览 (不产生 side effects)
    struct SalvageResult { uint32_t materialId; int count; };
    std::vector<SalvageResult> CalculateYield(const ItemComponent& item);
    
    // 执行分解
    void Execute(entt::registry& registry, entt::entity itemEntity, entt::entity playerEntity);
}
```

### 5.2 批量分解支持
- 系统应支持传入 `std::vector<entt::entity>` 进行并发无关的批量销毁。

## 6. 视觉与反馈 (UX & Feedback)
- **着色器驱动**: 分解时，物品图标应通过 `ItemDissolve` Shader 产生“化为碎片”的动态效果。
- **音效**: 使用 $f_c$ 高频破碎音效。
- **UI**: 批量分解后汇总显示获得的碎片总数。

## 7. 边缘情况 (Edge Cases)
- **锁定物品**: 无法分解 (`item.isLocked`)。
- **独特物品**: `Rarity::Legendary` 且具有 `ItemType == Unique` (或 `legendaryPotential > 0`) 的物品默认禁止分解，除非有特殊配方。
- **满溢**: 材料银行 `MaterialBankComponent` 无上限限制。

