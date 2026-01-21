# Monster Level Scaling System - Implementation Plan

## Overview
实现怪物随地图等级动态成长的机制，参考 D2/D3/POE 的成长曲线。

## Design Decisions (User Confirmed)

### 1. 成长曲线 (Multiplicative)
- **HP**: `BaseHP * (1 + HPGrowth)^(Lv-1)` - 激进曲线 (~10-12%/级)
- **Damage**: `BaseDmg * (1 + DmgGrowth)^(Lv-1)` - 激进曲线 (~8-10%/级)
- **直接乘算种族基础属性**

### 2. 区域等级
- 来源: `MapFragmentComponent.level` 或 `LevelManager` 当前地图等级
- 边界: 等级为 0 时修正为 1

### 3. 怪物等级同步
- 公式: `MonsterLevel = max(1, max(AreaLevel, PlayerLevel - 5))`
- 边界: 当 PlayerLevel <= 5 时，最低怪物等级为 1

### 4. 经验公式 (D3 Style)
```cpp
// D3 风格经验公式
float levelDiff = abs(playerLevel - monsterLevel);
float xpMult = 1.0f;
if (levelDiff > 5) {
    xpMult = max(0.1f, 1.0f - (levelDiff - 5) * 0.1f);
}
```

### 5. 抗性成长
- **Lv 1-100**: 使用种族基础抗性 (不变)
- **Lv 100+**: 每级增加抗性 (按稀有度)
  - Normal: +0.2% / 级
  - Champion: +0.4% / 级
  - Elite: +0.6% / 级
  - Boss: +0.8% / 级
  - Nemesis: +1.0% / 级
- **Hard Cap**: 75%

### 6. 护甲成长 (线性 → 20% 减伤)
- 目标: Lv 1 = 0% 减伤, Lv 100 = 20% 减伤
- 已有公式: `DR = Armor / (Armor + LevelFactor * ARMOR_BASE)`
- 反推 Armor 值保证线性 DR 增长

---

## Implementation Checklist

### Phase 2.1: Constants
- [x] Add `Constants::Scaling::Monster` namespace to `Common.hpp`

### Phase 2.2: MonsterScaling Utility
- [x] Create `src/game/utils/MonsterScaling.hpp`
- [x] Create `src/game/utils/MonsterScaling.cpp`
- [x] Implement `Calculate()`, `GetXPMultiplier()`, `SyncLevel()`

### Phase 2.3: EnemySpawnSystem Integration
- [x] Modify `spawnEnemy()` to use `MonsterScaling::Calculate()`
- [x] Apply level sync logic

### Phase 2.4: StatsSystem Integration
- [x] Modify `Recalculate()` to apply resistance growth for Lv 100+
- [x] Apply armor scaling

### Phase 2.5: XPAwardingSystem Integration
- [x] Apply D3-style XP multiplier

### Phase 2.6: LevelManager Integration
- [x] Expose current area level
- [x] Handle edge case (level = 0 → 1)

### Phase 2.7: Unit Tests
- [x] `tests/unit/MonsterScalingTest.cpp`

### Phase 2.8: Build & Validate
- [x] Run `.\build.bat`
- [x] Run tests

---

## Status
- Created: 2026-01-21
- Status: COMPLETED
