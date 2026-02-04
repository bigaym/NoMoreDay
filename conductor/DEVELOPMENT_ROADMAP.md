# NoMoreDay - 核心开发路线图 V1.2 (2026-01-27)

## 📊 当前进度与代码现状分析

根据对 `src/` 源代码的深度审计，对比 `@设计文档`，当前项目的真实进度如下：

### ✅ 已稳固的基础设施 (Phase 1-10 & Core)
| 模块 | 状态 | 技术要点 |
|------|------|----------|
| **C++20 ECS 核心** | ✅ 完成 | EnTT + Taskflow 并发调度，稳定支持 10,000+ 实体 |
| **渲染与 GPGPU** | ✅ 完成 | Raylib 2D + MDI Instancing + GPU 流场/粒子 (20万级) |
| **伤害流水线** | ✅ 完成 | **5步计算法**，SIMD 优化，支持复杂的伤害类型转换与 Tag 交互 |
| **维度拼接 (Mosaic)** | ✅ 完成 | 碎片掉落、3x3 地图拼接、属性共鸣与动态生成 |
| **物品与装备** | ✅ 完成 | 掉落、词缀、传奇融合 (LP)、符文之语、材料存储 |
| **怪物与宿敌** | ✅ 完成 | 动态等级成长、宿敌进化 (Evolution Tier)、高级精英词缀 |
| **基础职业 (剑修)** | ✅ 完成 | 核心技能 (流云刺, 裂空斩等) 及其基础技能树已实现 |
| **虚空星盘 (Astrolabe)** | ✅ 完成 | 账号级全局被动系统，支持六扇区布局、誓约机制与亲和度解锁 |
| **技能专精 (Spec)** | ✅ 完成 | 深度技能改造树，支持 Tag 视觉主题与节点形态区分 |
| **极致性能** | ✅ 完成 | 稳定 180 FPS (5.5ms 帧预算)，完成渲染管线重构与内存优化 |

### 🔍 代码审计发现的缺失 (对比设计文档)
| 缺失项 | 现状 | 影响 |
|------|------|------|
| **进阶专精 (Masteries)** | ❌ 未实现 | 剑修的三个进阶流派 (剑圣/天剑/魔剑) 及核心转化机制缺失 |
| **传家宝 (Heirloom)** | ❌ 未实现 | 跨存档装备传承机制尚未启动 |
| **终局循环 (Endgame)** | 🔄 部分 | 维度拼接已做，但“无尽梦魇”的腐化值 (Corruption) 与无限层逻辑未闭环 |
| **第二职业 (Mage)** | ❌ 未启动 | 仅有剑修单一职业，缺乏远程法系验证 |
| **战斗手感 2.0** | ❌ 未启动 | 缺乏顿帧 (Hit Stop)、动态音效混音等“打击感”核心要素 |

---

## 🚀 后续规划：从“原型”到“精品” (Refined Roadmap)

基于当前“系统完备但深度不足”的现状，我们将后续阶段调整为优先打磨核心战斗体验与职业深度，随后扩展 Meta 循环。

### 📍 Phase 11: 职业深度与战斗手感 (Class Depth & Game Feel)
**优先级：最高**。完善现有职业机制，确立标杆级的战斗体验。

- [ ] **Track: Core UI Asset Generation (Skill/Talent/HUD)**
    - **Skill & Talent**: 绘制星图/经络图背景，生成各级节点图标 (Stats/Notables/Keystones) 及流光连线纹理。
    - **Mastery UI**: 绘制专精选择祭坛 (Ascension Altar) 及剑圣/天剑/魔剑的插画。
    - **HUD**: 设计剑意量表 (Sword Gauge)、丹田血球、兵器架技能栏及 Buff/Debuff 边框。
- [ ] **Track: Sword Cultivator Masteries (剑修进阶专精)**
    - **UI 实现**: 50级专精选择界面 (Ascension Altar)。
    - **核心机制转化**:
        - **剑圣 (Sword Saint)**: 剑意 -> 剑流 (Crit/Speed)，实现 `七星斩` (Omnislash)。
        - **天剑 (Sky Sword)**: 剑意 -> 灵剑实体 (Turrets)，物理 -> 元素转化，实现 `天剑降临`。
        - **魔剑 (Demon Blade)**: 剑意 -> 嗜血 (Dmg/Vuln)，法力 -> 生命消耗，实现 `血海`。
- [ ] **Track: Game Feel 2.0 (战斗手感升级)**
    - **Hit Stop System**: 基于伤害阈值的全局时间冻结 (Global Time Dilation) 与实体级顿帧。
    - **Dynamic Audio**: 基于 Soloud 的动态混音，根据怪物密度/伤害量调整音效优先级与 Pitch/Volume。
    - **Screen Shake V2**: 引入 Trauma-based 摄像机震动，支持方向性震动 (Directional Shake)。

### 📍 Phase 12: 局外成长与终局循环 (Meta & Endgame)
**优先级：高**。构建长线游玩的驱动力。

- [ ] **Track: Mosaic & Environment Assets**
    - **Mosaic UI**: 绘制 3x3 拼接台 (The Loom) 底座、Tetris 形状的地图碎片 (石板/符咒) 及词缀卡牌。
    - **Environment**: 生成水墨风格的无缝地块 (Void Stone, Ink Grass, Dark Water) 及墙体/装饰物 (枯树, 石碑)，解决视觉缺失。
- [ ] **Track: Heirloom System (传家宝)**
    - 实现装备的“传家宝化”标记与属性动态压缩算法 (Level Scaling)。
    - 实现“跨存档仓库” (Heirloom Vault) 用于新角色继承。
- [ ] **Track: Eternal Nightmare (无尽梦魇)**
    - 实现 **腐化值 (Corruption)** 系统：怪物属性指数成长 vs 掉落品质提升。
    - 实现无限层生成算法与排行榜数据记录。

### 📍 Phase 13: 内容扩展 (Content Expansion)
**优先级：中**。验证架构的灵活性。

- [x] **Track: Void Astrolabe (虚空星盘) [COMPLETED]**
    - 实现账号级共享存档 (`GlobalSave`)。
    - 设计星盘 UI 与节点解锁逻辑 (使用星尘 Stardust)。
    - 实现核心机制修复（誓约确认、解锁反馈、单元测试）。
- [ ] **Track: Class Prototype - Mage (灵术师)**
    - 实现法师基础资源：**法力过载 (Mana Overload)**。
    - 实现 3 个核心技能：
        - **Fireball (火球)**: 投射物/爆炸。
        - **Frost Nova (冰环)**: 范围控制/护盾。
        - **Arcane Beam (奥术射线)**: 引导/高频伤害。
    - 验证 AttributePipeline 对法术伤害的扩展性。
- [ ] **Track: Audio System Integration**
    - 全面集成音效资源，为技能、界面、环境添加音效。

### 📍 Phase 14: 引导与发布准备 (Polish & Release)
**优先级：低**。

- [ ] **Track: Main Menu & Polish Assets**
    - **Visuals**: 绘制动态主菜单背景 (孤峰/巨剑)、书法风格 Logo 及魂灯样式的存档位。
    - **Polish**: 统一全 UI 交互音效与动效。
- [ ] **Track: Tutorial System**: 动态按键提示与机制引导。
- [ ] **Track: Localization**: 中英文文本抽离与切换。
- [ ] **Track: Release Build Optimization**: 最终包体瘦身与加密。

---

## 📝 立即执行计划 (Next Steps)

建议立即启动 **Phase 11**，优先完成 **剑修进阶专精**。这不仅能补全核心职业设计，还能验证复杂的“机制转化”逻辑是否在现有架构中跑通。

1.  **激活 `feature-planner`** 细化 `Sword Cultivator Masteries` 的 Spec。
2.  **激活 `feature-developer`** 开始实现专精选择 UI 与 数据层变更。
