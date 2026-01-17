# Spec: Legendary Affix System (Infrastructure & Implementation)

## 核心概念
实现传奇和独特词缀的基础设施。这些词缀具有改变游戏机制的效果，且不会在普通的随机装备掉落中出现，仅预留给传奇 (Legendary) 或独特 (Unique) 装备。

## 技术实现

### 1. 数据结构 (Data Structure)
- **AffixType 升级**: 底层类型提升为 `uint16_t`。
- **语义化区间 (Range Markers)**:
  - `Normal_Start = 0` / `Normal_End = 999` (普通随机池)。
  - `Legendary_Start = 1000` / `Legendary_End = 1999` (传奇独占区间)。
- **判断辅助函数**:
  - `IsRandomRollableAffix(type)`: 基于 ID 区间判断是否可随机掉落。

### 2. 系统逻辑 (System Logic)
- **加载器**: `ItemFactory::initialize` 增加对 `assets/data/legendary_affixes.json` 的追加加载。
- **过滤逻辑**: 随机生成逻辑通过 `IsRandomRollableAffix` 语义化过滤高 ID 词缀。

### 3. 数据生成
- 存储 ID 采用 `1000 + 设计ID` 的偏移规则。
- 职业限定词缀自动关联 `blade_ascendant` 标签。
