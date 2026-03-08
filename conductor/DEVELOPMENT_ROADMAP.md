# NoMoreDay - 核心开发路线图 V1.3 (2026-03-06)

## 📊 当前进度与代码现状分析

根据最近的路线追踪、缺陷登记、验证记录与近期提交，当前项目已经越过“底层架构搭建期”，进入“核心系统完成度较高、产品闭环与体验打磨不足”的阶段。以下判断以 `conductor/tracks.md`、`conductor/bug_registry.md`、验证记录为准；`product.md` 中的部分表述偏产品愿景，不作为真实完工口径。

### ✅ 已稳固的基础设施 (Phase 1-10 & Core)
| 模块 | 状态 | 技术要点 |
|------|------|----------|
| **C++20 ECS 核心** | ✅ 完成 | EnTT + Taskflow 并发调度，稳定支持 10,000+ 实体 |
| **渲染与 GPGPU** | ✅ 高完成度 | V3/V4/V5 主体特性、阴影、Clustered、GI、VFX/材质联动基本完成，仅余少量验证债与开放缺陷 |
| **伤害流水线** | ✅ 高完成度 | Combat V2 / M1-M3 路线已收口，统一 DamagePipeline、DoT/召唤/防御/Proc/遥测合同均已落地 |
| **维度拼接 (Mosaic)** | ✅ 完成 | 碎片掉落、3x3 地图拼接、属性共鸣与动态生成 |
| **物品与装备** | ✅ 完成 | 掉落、词缀、传奇融合 (LP)、符文之语、材料存储 |
| **怪物与宿敌** | ✅ 完成 | 动态等级成长、宿敌进化 (Evolution Tier)、高级精英词缀 |
| **基础职业 (剑修)** | ✅ 高完成度 | 核心技能、技能专精、Blade Ascendant 行为与 VFX 已完成大部分实现与验证 |
| **虚空星盘 (Astrolabe)** | ✅ 完成 | 账号级全局被动系统，支持六扇区布局、誓约机制与亲和度解锁 |
| **技能专精 (Spec)** | ✅ 完成 | 深度技能改造树，支持 Tag 视觉主题与节点形态区分 |
| **工程化与门禁** | ✅ 完成 | build / ci / unit / integration / perf 多层验证链已建立，近期工作以稳定化和回归门禁为主 |

### 🔍 当前主要缺口与风险
| 项目 | 现状 | 影响 |
|------|------|------|
| **进阶专精 (Masteries)** | ❌ 未实现 | 剑修仍缺 50 级分流后的 build identity，现有职业深度未闭环 |
| **战斗手感 2.0** | ❌ 未系统启动 | 命中反馈、顿帧、镜头震动、音效层次不足，导致已完成的战斗系统价值感知偏弱 |
| **终局循环 (Eternal Nightmare)** | 🔄 部分 | Mosaic 已有，但 Corruption、无限层推进、奖励/风险倍率、终局结算未闭环 |
| **传家宝 (Heirloom)** | ❌ 未实现 | 局外成长缺关键抓手，长期重复开局驱动力不足 |
| **第二职业 (Mage)** | ❌ 未启动 | 当前只验证了近战主职业，架构的第二职业复用能力尚未证明 |
| **GPUText 完整性** | 🔄 可用但未完备 | 仍有同步回读性能风险、UTF-8/本地化文本链路缺失、MSDF 资源 ownership 工程债 |
| **表现层开放问题** | 🔄 持续收敛中 | 技能 1 / 4 可读性、HDR / V3 离屏启用链路与少量渲染日志问题仍需收尾 |

---

## 🚀 后续规划：从“系统完成”到“产品闭环” (Refined Roadmap)

基于当前“底层成熟、玩法闭环不足、体验层仍有缺口”的现状，后续开发遵循以下原则：

1. **先补单职业完整闭环，再扩内容宽度**：先把剑修的 Masteries、手感、终局、局外成长补齐，再做第二职业。
2. **先做玩家能感知到的提升，再做底层锦上添花**：战斗反馈、终局留存、局外驱动优先级高于继续堆新技术名词。
3. **并行清理阻塞体验的开放缺陷**：表现层和渲染链路的 open bug 不单独成大 Phase，但必须持续清债。

### 📍 Phase 11: 职业深度与战斗手感 (Class Depth & Game Feel)
**优先级：最高**。先把现有主职业做成完整、好玩、可读、可持续迭代的标杆内容。

- [ ] **Track: Sword Cultivator Masteries (剑修进阶专精)**
    - **目标**: 完成 50 级分流、三系核心资源转化、关键技能 / 词缀 / 星盘联动，建立首个真正完整的职业成长闭环。
    - **核心机制转化**:
        - **剑圣 (Sword Saint)**: 剑意 -> 剑流 (Crit / Speed)，强调高频连斩与瞬时爆发。
        - **天剑 (Sky Sword)**: 剑意 -> 灵剑实体 (Turrets / Orbitals)，强调召唤 / 元素转化与空间压制。
        - **魔剑 (Demon Blade)**: 剑意 -> 嗜血 (Dmg / Vuln)，强调生命换伤害、风险收益与吸血循环。
    - **交付范围**:
        - Mastery 选择 UI (`Ascension Altar`)。
        - 三系被动核心、转化规则、代表技能与关键节点。
        - 与 Spec / Astrolabe / 装备词缀的最小可玩联动。
- [ ] **Track: Game Feel 2.0 (战斗手感升级)**
    - **目标**: 让玩家稳定感知到“打中、暴击、破防、护盾、危险区域、技能完成态”的反馈。
    - **子项**:
        - **Hit Stop / Time Dilation**: 基于伤害阈值与技能标签的局部 / 全局顿帧。
        - **Screen Shake V2**: Trauma-based 摄像机震动，支持方向与强度分层。
        - **Dynamic Audio**: 技能 / 击杀 / 危险提示与动态混音。
        - **Readability Pass**: 收敛 `BUG-20260222-001`、`BUG-20260222-003`、`BUG-20260222-005` 等表现缺陷。
- [ ] **Track: Combat Presentation Debt Closure**
    - 收尾当前 open / in-progress 的体验问题，尤其是技能反馈不足、Buff / 特效时序不一致、V3 / HDR 离屏链路误禁用。
    - 要求每项修复都附带手测路径与最小自动化验证，避免“看起来修了，实际又回退”。

### 📍 Phase 12: 局外成长与终局循环 (Meta & Endgame)
**优先级：高**。构建长线游玩的驱动力。

- [ ] **Track: Eternal Nightmare (无尽梦魇)**
    - **目标**: 形成“继续打下去有压力、有奖励、有结算”的终局循环。
    - **范围**:
        - **Corruption**: 怪物强度、词缀密度、掉落品质、事件危险度的统一倍率系统。
        - **Infinite Progression**: 无限层推进、失败结算、阶段性 Boss / 事件插入。
        - **Rewards**: 与 Heirloom、Mosaic、Astrolabe 资源形成回流。
- [ ] **Track: Heirloom System (传家宝)**
    - **目标**: 建立局外保留与新角色继承价值，让重复开局存在长期意义。
    - **范围**:
        - 装备传家宝化标记与属性压缩 / 等级缩放。
        - `Heirloom Vault` 跨角色仓库。
        - 与掉落、稀有度、传奇融合的限制与继承规则。
- [ ] **Track: Mosaic & Environment Assets**
    - 为终局循环补齐关键 UI 与环境素材，但作为支撑项，不先于玩法闭环启动。

### 📍 Phase 13: 内容扩展 (Content Expansion)
**优先级：中**。验证架构的灵活性。

- [x] **Track: Void Astrolabe (虚空星盘) [COMPLETED]**
    - 实现账号级共享存档 (`GlobalSave`)。
    - 设计星盘 UI 与节点解锁逻辑 (使用星尘 Stardust)。
    - 实现核心机制修复（誓约确认、解锁反馈、单元测试）。
- [ ] **Track: Class Prototype - Mage (灵术师)**
    - **启动条件**: 剑修 Masteries、Game Feel 2.0、Eternal Nightmare 至少达到“可稳定手测迭代”状态后再启动。
    - 实现法师基础资源：**法力过载 (Mana Overload)**。
    - 实现 3 个核心技能：
        - **Fireball (火球)**: 投射物 / 爆炸。
        - **Frost Nova (冰环)**: 范围控制 / 护盾。
        - **Arcane Beam (奥术射线)**: 引导 / 高频伤害。
    - 验证 AttributePipeline 对法术伤害的扩展性。
- [ ] **Track: Audio System Integration**
    - 全面集成音效资源，为技能、界面、环境添加音效。
- [ ] **Track: GPUText & Localization Foundation**
    - 解决 GPUText 同步回读、UTF-8 映射、MSDF 资源所有权等工程债，为后续本地化和复杂 UI 文本打底。

### 📍 Phase 14: 引导与发布准备 (Polish & Release)
**优先级：低**。

- [ ] **Track: Main Menu & Polish Assets**
    - **Visuals**: 绘制动态主菜单背景 (孤峰 / 巨剑)、书法风格 Logo 及魂灯样式的存档位。
    - **Polish**: 统一全 UI 交互音效与动效。
- [ ] **Track: Tutorial System**: 动态按键提示与机制引导。
- [ ] **Track: Localization**: 中英文文本抽离与切换。
- [ ] **Track: Release Build Optimization**: 最终包体瘦身与加密。

---

## 📝 立即执行计划 (Next Steps)

建议立即启动 **Phase 11**，并按以下顺序安排后续开发任务：

1. **Sword Cultivator Masteries**：补齐主职业成长闭环，作为第一优先级主线。
2. **Game Feel 2.0 + 表现层清债**：同步解决技能反馈、护盾可读性、战斗反馈不足等问题。
3. **Eternal Nightmare**：把现有战斗 / 掉落 / 地图系统串成真正的终局循环。
4. **Heirloom System**：补齐局外成长，提升长期重复游玩动力。
5. **Mage Prototype**：在第一职业与终局闭环稳定后，再验证第二职业扩展。

### 任务规划规则

- 后续任务拆解默认以本路线图为优先口径，若与其他文档冲突，以 `tracks.md`、`bug_registry.md`、最新验证证据为准。
- 每个新 Track 启动前，先明确：**玩家收益、系统依赖、验证命令、手测路径、回归风险**。
- 若实现过程中发现路线图与代码现状不一致，先更新本文件，再继续展开任务。
