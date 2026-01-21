# NoMoreDay - 核心开发路线图 V1.1 (2026-01-14)

## 📊 当前进度与代码现状分析

根据对 `src/` 源代码的深度审计，对比 `@设计文档`，当前项目的真实进度如下：

### ✅ 已稳固的基础设施 (Phase 1-9 & Core)
| 模块 | 状态 | 技术要点 |
|------|------|----------|
| **C++20 ECS 核心** | ✅ 完成 | EnTT + Taskflow 并发调度，稳定支持 10,000+ 实体 |
| **混合护盾系统** | ✅ 完成 | **ES + Ward** 模式，支持回充延迟、动态衰减与智力维持加成 |
| **渲染与 GPGPU** | ✅ 完成 | Raylib 2D 渲染 + GPU 流场寻路 (SSBO) + GPU 粒子 (20万级) |
| **伤害流水线** | ✅ 完成 | **5步计算法** (Base->Convert->Inc->More->Settle)，SIMD 优化 |
| **维度拼接系统** | ✅ 完成 | 碎片掉落、3x3 拼接、属性共鸣与地图生成集成 |
| **极致性能优化** | ✅ 完成 | **MDI Rendering** + **EnTT Group** + **SIMD SpatialGrid** + **Branchless Combat**，稳定 180 FPS |
| **基础 AI 行为** | ✅ 完成 | Support, Assassin, Tank, Fodder 等原型逻辑实现 |
| **存档与传家宝** | ✅ 完成 | 物品持久化、跨存档继承、属性动态压缩 |
| **传奇融合 (Legendary Merging)** | ✅ 完成 | Unique (LP) + Exalted 词缀继承逻辑，Ancient 稀有度实现 |
| **基础 AI 行为** | 🔄 部分完成 | Fodder/Tank/Assassin 原型已出，Support (支援者) 逻辑尚为空 |

### 🔍 代码审计发现的缺失 (对比设计文档)
| 缺失项 | 现状 | 影响 |
|------|------|------|
| **符文语 (Runewords)** | ✅ 完成 | 33 种符文、底材匹配序列、激活逻辑与位点集成 |
| **传奇词缀基础设施** | ✅ 完成 | uint16 ID 空间、语义化锚点、JSON 动态描述加载回调 |
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
- [x] **底材价值重构**
    - 增加“底材隐性属性 (Implicit)”系统，使同类武器的不同底材具有差异化。
- [x] **实现传奇词缀基础设施 (Legendary Affix Infra)**
    - 将 AffixType 升级为 uint16_t (0-65535)，定义 Normal (0-999) 和 Legendary (1000-1999) 锚点。
    - 实现 `GetAffixNameLookup` 回调接口，支持从 `legendary_affixes.json` 动态加载词缀描述。

    - **Code Risk Mitigation**: [All Phase 9 Fixes Applied]
        - **SkillSystem**: Fixed UAF vulnerability and resolved critical test failures.
        - **GPUFlowFieldSystem**: Fixed resource leaks.
        - **Infrastructure**: Stabilized integration test suite.

### 📍 Phase 10: 敌人生态与宿敌进化 (Advanced AI & Nemesis)
**优先级：高**。增强战斗的挑战性与交互性。

- [x] **扩充精英词缀库**
    - [x] **Molten (熔火)**：路径伤害与死亡爆炸。(Completed)
    - [x] **Mirror Image (镜像)**：受击分裂。(Completed)
    - [ ] **Nullifier (虚无)**：周期性驱散玩家 Buff。
    - [x] **Teleporter (闪烁)**：受到攻击或定时间隔瞬移至玩家身后。(Completed)
    - [x] **Shielding (护盾)**：周期性为周围友军提供无敌护盾。(Completed)
    - [x] **Environmental & Hazard**: Frozen, Toxic, Void Zone, Storm Strider. (Completed)
    - [x] **Physics & CC**: Vortex, Waller, Entangler. (Completed)
    - [x] **Advanced Mechanics**: Soul Eater, Suppressor, Mana Siphon. (Completed)
- [ ] **AI 行为树补完**
    - 实现 **Support (支援者)** 逻辑：`Flee` + `CastBuff` (Shield/Frenzy)。
    - 优化 **Tank (坦克)** 逻辑：主动阻挡视线 (`BlockLineOfSight`)。
- [x] **宿敌系统进化闭环**
    - [x] 强化 `NemesisGenerator` 的分析逻辑：统计玩家近 50 次击杀的伤害构成。(Completed)
    - [x] **逻辑挂载**：实装 `MonsterAffixComponent` 动态缩放与 Evolution Tier。(Completed)
    - [x] **针对性进化**：根据玩家历史 hurt type 动态调整抗性与词缀。(Completed)


### 📍 Phase 11: 战斗内容与职业扩展 (Class & Combat Expansion)
**优先级：中**。增加游戏横向可玩性。

- [x] **剑修视觉升级 (Blade Ascendant VFX)**
    - **Asset Pipeline**: 实现 SwordTrail, HoloBlade, Distortion Shaders 及 GPU 粒子配置。
- [x] **System Integration**: 完成剑意可视化、流云刺拖尾与万剑归宗的粒子流表现。(Completed)
- [x] **性能与逻辑修复**: 修复 SkillSystem UAF, 优化 GPU 粒子生命周期。(Completed)
- [ ] **第二职业原型：灵术师 (Mage/Caster)**
    - 抽象 `ClassBase`，实现高额自然回蓝机制。
    - 设计 3 个核心法术（火球、冰环、奥术流）。
- [ ] **战斗手感优化 (Game Feel 2.0)**
    - 实现命中顿帧 (Hit Stop) 的精细化控制。
    - 增加基于 Tag 的动态打击音效系统。

### 📍 Phase 12: 打磨、UI 与 终局循环 (Polish & Loop)
**优先级：中**。提升整体完整度。

- [x] **UI 视觉打磨与分解 UX 重构**
    - [x] 实现祭坛式分解界面、产出预览、批量过滤器。(Completed)
    - [x] 增加装备槽位 Ghost Icons 和动态面板拖拽。(Completed)
        - [x] **Inventory UI Overhaul**: 分页拖拽、右键菜单、搜索过滤。(Completed)
    - [x] **Tooltip Upgrade**: 大图标预览与镶嵌孔显示。(Completed)
    - [x] **实现仓库系统 (Stash System)**: 个人与账号共享仓库、10页标签解锁、带缓存的搜索高亮与一键整理功能。(Completed)
- [ ] **成就系统与教程系统**
- [ ] **无尽梦魇排行榜 UI 完善**
- [ ] **音频系统 (AudioSystem) 动态混音集成**

### 📍 Phase 13: 极致性能优化 (Performance Extreme Optimization)
**优先级：已完成**。重构核心架构以支持万级实体流畅运行。

- [x] **Phase 1: MDI Rendering**: GPU 驱动的 Multi-Draw Indirect 渲染管线。
- [x] **Phase 2: EnTT Group**: 内存布局优化，通过 Group 预排序提升遍历速度。
- [x] **Phase 3: Triple-Buffer**: 持久化映射 (Persistent Mapping) 消除 GPU 同步等待。
- [x] **Phase 4: SIMD SpatialGrid**: 向量化加速的空间划分查询。
- [x] **Phase 5: Branchless Combat**: 消除热点代码分支预测失败。
- [x] **String Dependency Elimination**: 全局移除运行时字符串比较，迁移至 Enum/ID。

---

## 📝 开发准则
1. **安全第一**：所有新系统必须包含对应的 Unit Test。
2. **性能导向**：复杂 AI 词缀必须在 `Taskflow` 中并行，或利用 GPU 粒子表现。
3. **数据驱动**：新词缀、新符文语必须优先在 JSON 中定义，通过 `ItemFactory` 加载。