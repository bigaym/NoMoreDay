# NoMoreDay - 核心开发路线图 V1.1 (2026-01-14)

## 📊 当前进度与代码现状分析

根据对 `src/` 源代码的深度审计，对比 `@设计文档`，当前项目的真实进度如下：

### ✅ 已稳固的基础设施 (Phase 1-9 & Core)
| 模块 | 状态 | 技术要点 |
|------|------|----------|
| **C++20 ECS 核心** | ✅ 完成 | EnTT + Taskflow 并发调度，稳定支持 10,000+ 实体 |
| **渲染与 GPGPU** | ✅ 完成 | Raylib 2D 渲染 + GPU 流场寻路 (SSBO) + GPU 粒子 (20万级) |
| **伤害流水线** | ✅ 完成 | **5步计算法** (Base->Convert->Inc->More->Settle)，SIMD 优化 |
| **维度拼接系统** | ✅ 完成 | 碎片掉落、3x3 拼接、属性共鸣与地图生成集成 |
| **基础 AI 行为** | ✅ 完成 | Support, Assassin, Tank, Fodder 等原型逻辑实现 |
| **存档与传家宝** | ✅ 完成 | 物品持久化、跨存档继承、属性动态压缩 |
| **传奇融合 (Legendary Merging)** | ✅ 完成 | Unique (LP) + Exalted 词缀继承逻辑，Ancient 稀有度实现 |

### 🔍 代码审计发现的缺失 (对比设计文档)
| 缺失项 | 现状 | 影响 |
|------|------|------|
| **符文语 (Runewords)** | ❌ 尚未实现 | 仅有基础插槽，缺乏 D2 式的序列加成，白装价值低 |
| **宿敌针对性进化** | 🔄 基础实现 | 目前仅支持单一主伤害类型抗性，缺乏对复杂 Build 的分析 |
| **高级精英词缀** | 🔄 数量不足 | 仅实现 SoulLink, Avenger；缺失 Molten, Mirror Image, Nullifier 等 |
| **第二职业 (Mage/Ranger)** | ❌ 尚未启动 | 目前仅有“剑修”职业及其分支 |

---

## 🚀 后续规划：完善游戏深度 (Refined Roadmap)

基于以上分析，我们将路线图重构为以下三个阶段，重点解决“玩法深度”和“系统闭环”问题。

### 📍 Phase 9: 终局装备深度 (Endgame Gear & Crafting)
**优先级：最高**。解决中后期刷宝动力不足的问题。

- [x] **实现材料存储系统 (Material Storage System)**
    - 零实体存储 (`MaterialBankComponent`)，UI 虚拟化列表支持 100+ 种材料展示。
    - 自动拾取、分类过滤与搜索功能。
- [x] **实现传奇融合系统 (Legendary Merging)**
    - 修改 `CraftingSystem::fuseItems`，实现 Unique (LP) + Exalted 的词缀抽取算法。
    - 实现融合 UI 与 视觉效果。
- [x] **实现符文语系统 (Runewords)**
    - 定义 33 种符文及其在不同底材上的序列组合。
    - 实现 `RunewordSystem`，在物品插槽填满时检查并应用特殊特效。
- [ ] **底材价值重构**
    - 增加“底材隐性属性 (Implicit)”系统，使同类武器的不同底材具有差异化。

### 📍 Phase 10: 敌人生态与宿敌进化 (Advanced AI & Nemesis)
**优先级：高**。增强战斗的挑战性与交互性。

- [ ] **扩充精英词缀库**
    - 实现 **Molten (熔火)**：路径伤害与死亡爆炸。
    - 实现 **Mirror Image (镜像)**：受击分裂。
    - 实现 **Nullifier (虚无)**：周期性驱散玩家 Buff。
- [ ] **宿敌系统进化闭环**
    - 强化 `NemesisGenerator` 的分析逻辑：统计玩家近 100 次击杀的伤害构成，而非仅仅主属性。
    - 使宿敌获得针对玩家高频技能的防御组件（如：频繁格挡投射物）。


### 📍 Phase 11: 战斗内容与职业扩展 (Class & Combat Expansion)
**优先级：中**。增加游戏横向可玩性。

- [x] **剑修视觉升级 (Blade Ascendant VFX)**
    - **Asset Pipeline**: 实现 SwordTrail, HoloBlade, Distortion Shaders 及 GPU 粒子配置。
- [ ] **System Integration**: 将剑意层数可视化（光环/发光），实现流云刺拖尾与万剑归宗的粒子流表现。
- [ ] **第二职业原型：灵术师 (Mage/Caster)**
    - 抽象 `ClassBase`，实现高额自然回蓝机制。
    - 设计 3 个核心法术（火球、冰环、奥术流）。
- [ ] **战斗手感优化 (Game Feel 2.0)**
    - 实现命中顿帧 (Hit Stop) 的精细化控制。
    - 增加基于 Tag 的动态打击音效系统。

### 📍 Phase 12: 打磨、UI 与 终局循环 (Polish & Loop)
**优先级：中**。提升整体完整度。

- [ ] **成就系统与教程系统**
- [ ] **无尽梦魇排行榜 UI 完善**
- [ ] **音频系统 (AudioSystem) 动态混音集成**

---

## 📝 开发准则
1. **安全第一**：所有新系统必须包含对应的 Unit Test。
2. **性能导向**：复杂 AI 词缀必须在 `Taskflow` 中并行，或利用 GPU 粒子表现。
3. **数据驱动**：新词缀、新符文语必须优先在 JSON 中定义，通过 `ItemFactory` 加载。