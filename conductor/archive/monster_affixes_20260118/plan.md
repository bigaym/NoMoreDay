# Monster Affix Implementation Plan

## Status: COMPLETE ✅

## P0: Core Infrastructure ✅
- [x] `MonsterAffixRegistry.hpp` - 词缀类型枚举和静态注册表
- [x] `MonsterAffixComponent` - 运行时组件
- [x] 数据驱动的词缀定义 (constexpr 静态数组)

## P1: Stat-Based Affixes ✅
- [x] `EnemySpawnSystem::spawnEnemy` 词缀分配
- [x] 稀有度决定词缀数量 (Champion=1, Elite=2, Boss=4)
- [x] `StatModifier` 应用 (速度/护甲/伤害等)
- [x] 向后兼容 (`AvengerComponent`, `SoulLinkComponent`)

## P2: Mechanic Affixes ✅
- [x] `MonsterAffixSystem.hpp` 创建
- [x] **Molten (熔火)**: 每0.5s生成3s持续的火焰区域
- [x] **Teleporter (闪烁)**: 距离>300px时闪烁到玩家背后
- [x] **Berserker (狂暴)**: HP<50%时伤害x2，体型x1.5
- [x] **Frozen (极寒)**: 150px光环，玩家减速30%
- [x] **Nullifier (消魔)**: 命中时移除目标所有非debuff效果

## P3: Visuals & Polish ✅
- [x] 怪物血条下方显示词缀名称标签
- [x] 词缀使用特定颜色 (橙红=Molten, 淡蓝=Fast, 等)
- [x] **熔火区域视觉渲染** - 渐变圆+发光效果+外环
- [x] **熔火区域GPU粒子** - 每帧3%几率发射火焰粒子

## P4: Damage & Cleanup ✅
- [x] `DelayedDestroyComponent` 倒计时处理 (`EffectSystem`)
- [x] **熔火区域伤害逻辑** - 每0.25s对玩家造成15dps火焰伤害
- [x] 伤害飘字使用火焰颜色 (橙色)

## P5: Advanced AI (Optional/Later)
- [ ] **Shielding AI**: 给友军施加无敌护盾
- [ ] **Waller**: 生成阻挡墙壁

---

## Implementation Notes (2026-01-18)

### Files Created:
1. `src/game/data/MonsterAffixRegistry.hpp` - 词缀系统定义
2. `src/game/systems/combat/MonsterAffixSystem.hpp` - 机制词缀逻辑

### Files Modified:
1. `src/game/systems/world/EnemySpawnSystem.cpp` - 词缀分配
2. `src/game/systems/ui/MonsterHealthBarSystem.cpp` - 词缀UI
3. `src/game/states/GameplayState.cpp` - 系统初始化
4. `src/game/systems/combat/EffectSystem.cpp` - 熔火伤害+延迟销毁
5. `src/engine/render/RenderSystem.cpp` - 熔火视觉渲染

### Affix Visual Summary:
| Affix | Color | Visual Effect |
|-------|-------|--------------|
| Molten | 橙红 (255,80,0) | 火焰区域+粒子 |
| Frozen | 冰蓝 (100,200,255) | 减速Buff图标 |
| Fast | 淡蓝 (200,200,255) | 标签颜色 |
| Tanky | 灰色 (150,150,150) | 标签颜色 |
| Powerful | 浅红 (255,100,100) | 标签颜色 |
| Berserker | 红色 (255,50,50) | 体型放大+红色 |
| Teleporter | 淡紫 (200,150,255) | 位置瞬移 |

### Performance Notes:
- 熔火伤害检测使用简单距离检测 (O(玩家数 × 火焰区数))
- 粒子发射使用3%几率限流，避免过多粒子
- `DelayedDestroyComponent` 使用倒计时而非累加，符合自然销毁逻辑
