# 🌌 星系天赋系统重构实施计划 (V1.1)

> **Track ID**: `astrolabe-refactor_20260204`
> **依赖 Spec**: `spec.md` (V1.1)
> **预计总工时**: 3-4 天 (28-36h)
> **风险等级**: 🟢 低 (无需存档兼容，可放开手脚)

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 预计工时 | 状态 |
|------|------|----------|----------|------|
| **Phase 1** | 数据模型重构 | `TalentNode`, `TalentGraph`, 组件扩展 | 4h | 🔵 待开始 |
| **Phase 2** | 布局服务 | `TalentLayoutService` 动态坐标计算 | 3h | 🔵 待开始 |
| **Phase 3** | 加载器重写 | `TalentLoader` 新格式支持 | 3h | 🔵 待开始 |
| **Phase 4** | 核心系统逻辑 | `AstrolabeSystem` 亲和度/誓约机制 | 5h | 🔵 待开始 |
| **Phase 5** | UI 渲染重构 | `AstrolabeRenderer` 扇区布局 + GPU 特效 | 8h | 🔵 待开始 |
| **Phase 6** | UI 交互重构 | `UIAstrolabe` 誓约对话框/节点交互 | 5h | 🔵 待开始 |
| **Phase 7** | 测试与集成 | 单元测试, 视觉验证 | 4h | 🔵 待开始 |

---

## Sub-Tracks 概览

本 Track 拆分为以下 **7 个子 Track**，每个子 Track 可独立执行并产出可验证的交付物。

| Sub-Track | ID | 依赖 | 描述 |
|-----------|-----|------|------|
| **T1** | `data-model` | - | 数据模型重构：类型定义、组件扩展 |
| **T2** | `layout-service` | T1 | 布局服务：动态坐标计算 |
| **T3** | `talent-loader` | T1 | 加载器：新 JSON 格式解析 (直接重写) |
| **T4** | `system-logic` | T1, T3 | 核心系统：亲和度、誓约、节点解锁 |
| **T5** | `renderer` | T1, T2 | UI 渲染：扇区、轨道、节点 + GPU 特效 |
| **T6** | `interaction` | T4, T5 | UI 交互：誓约对话框、节点点击 |
| **T7** | `testing` | All | 测试：单元测试、集成验证 |

---

## Phase 1: 数据模型重构 (Sub-Track T1)

**目标**: 定义新的数据类型，扩展玩家组件。

### Task 1.1: 定义职业枚举和节点类型
- [ ] 在 `TalentData.hpp` 中新增 `ProfessionID` 枚举:
  - BladeMaster(0), Spellweaver(1), SpiritWarden(2), Guardian(3), ShadowHunter(4), Berserker(5)
- [ ] 重命名/新增 `TalentNodeType` 枚举 (Minor, Major, Core)
- [ ] 定义 `TierThreshold` 结构体或 constexpr (Tier 1/2/3 门槛)

**文件**: `src/game/data/TalentData.hpp`

### Task 1.2: 定义新的 TalentNode 结构体
- [ ] 创建 `TalentNode` 结构体 (参考 spec.md §2.2)
- [ ] 包含 `profession`, `tier`, `sectorIndex`, `maxPoints` 字段
- [ ] 复用现有 `modifiers`, `effects` 等效果字段
- [ ] 添加 `mutable float x, y` 用于运行时坐标

**文件**: `src/game/data/TalentData.hpp`

### Task 1.3: 定义 TalentGraph 和 ProfessionStar
- [ ] 创建 `ProfessionStar` 结构体 (name_key, desc_key, 坐标)
- [ ] 创建 `TalentGraph` 结构体 (professionStars[6], nodes map)
- [ ] 添加 JSON 序列化/反序列化函数

**文件**: `src/game/data/TalentData.hpp`

### Task 1.4: 扩展 AstrolabeComponent
- [ ] 添加 `professionAffinity[6]` 数组
- [ ] 添加 `mainProfession` 字段 (-1 = 未誓约)
- [ ] 添加 `nodePoints` map (节点 ID -> 已投入点数)
- [ ] 添加辅助方法 `getAffinity()`, `hasVow()`, `isMainProfession()`
- [ ] 更新 `to_json` / `from_json` 函数

**文件**: `src/game/components/Progression.hpp`

### Task 1.5: 扩展布局常量
- [ ] 在 `Constants::Astrolabe` 中添加 `PROFESSION_COUNT`, `SECTOR_ANGLE`
- [ ] 添加轨道半径常量 `ORBIT_R1` ~ `ORBIT_R4`
- [ ] 添加节点大小常量

**文件**: `src/game/components/Common.hpp`

**交付物**: 编译通过，新类型可用于后续阶段。

---

## Phase 2: 布局服务 (Sub-Track T2)

**目标**: 实现动态节点坐标计算。

### Task 2.1: 创建 TalentLayoutService 类
- [ ] 创建 `TalentLayoutService.hpp` / `.cpp`
- [ ] 实现 `getSectorStartAngle(ProfessionID)` - 返回扇区起始角度
- [ ] 实现 `getOrbitRadius(tier)` - 返回对应 Tier 的轨道半径

**文件**: `src/game/systems/skill/TalentLayoutService.hpp`, `.cpp`

### Task 2.2: 实现节点位置计算
- [ ] 实现 `computeNodePositions(TalentGraph&)`
- [ ] 遍历所有节点，根据 `(profession, tier, sectorIndex)` 计算 `(x, y)`
- [ ] 计算各 Profession Star 的位置 (内环轨道)
- [ ] 处理扇区内多节点的均匀角度分布

**文件**: `src/game/systems/skill/TalentLayoutService.cpp`

### Task 2.3: 单元测试布局计算
- [ ] 验证 6 职业扇区起始角度正确 (0°, 60°, 120°, ...)
- [ ] 验证节点坐标落在对应轨道半径上
- [ ] 验证节点不重叠

**文件**: `tests/TalentLayoutTests.cpp`

**交付物**: `TalentLayoutService` 可计算所有节点坐标。

---

## Phase 3: 加载器重写 (Sub-Track T3)

**目标**: 直接重写，支持新的 JSON 格式，**无需兼容旧格式**。

### Task 3.1: 创建新 JSON 数据
- [ ] 重写 `assets/data/profession_talents.json`
- [ ] 为剑修职业定义完整节点 (参考职业设计草案_剑修.md 的星盘天赋布局)
- [ ] 为其他 5 个职业定义占位节点 (3-5 个/职业)
- [ ] 定义 6 个 ProfessionStar

**文件**: `assets/data/profession_talents.json`

### Task 3.2: 重写 TalentLoader
- [ ] 删除旧的 `LoadAstrolabe()` 函数
- [ ] 实现 `LoadProfessionTalents(path, TalentGraph&)` 函数
- [ ] 解析 `profession_stars` 数组
- [ ] 解析 `nodes` 数组，填充 `TalentGraph.nodes`
- [ ] 调用 `TalentLayoutService::computeNodePositions()` 填充坐标

**文件**: `src/game/data/TalentLoader.hpp`, `.cpp`

**交付物**: 加载器可解析新格式并计算节点坐标。

---

## Phase 4: 核心系统逻辑 (Sub-Track T4)

**目标**: 实现亲和度解锁和誓约机制。

### Task 4.1: 重构 AstrolabeSystem 接口
- [ ] 更新 `AstrolabeSystem` 类签名 (参考 spec.md §3.2)
- [ ] 添加 `canUnlockNode()`, `addPointToNode()`, `getNodeStatus()` 声明

**文件**: `src/game/systems/skill/AstrolabeSystem.hpp`

### Task 4.2: 实现亲和度检查逻辑
- [ ] 实现 `meetsTierRequirement()` - 检查职业亲和度是否满足节点 Tier 门槛
- [ ] 实现 `meetsVowRequirement()` - 若为 Core 节点，检查是否为主修职业
- [ ] 实现 `canUnlockNode()` - 组合上述检查

**文件**: `src/game/systems/skill/AstrolabeSystem.cpp`

### Task 4.3: 实现节点点数投入
- [ ] 实现 `addPointToNode()`:
  - 检查可用点数 > 0
  - 检查 `nodePoints[id] < maxPoints`
  - 增加 `nodePoints[id]`
  - 累加 `professionAffinity[profession]`
  - 若首次激活，添加到 `activated_nodes`
  - 调用 `AttributePipeline::Calculate()` 刷新属性

**文件**: `src/game/systems/skill/AstrolabeSystem.cpp`

### Task 4.4: 实现誓约机制
- [ ] 实现 `canTakeVow()` - 检查是否已有誓约
- [ ] 实现 `takeVow()`:
  - 设置 `mainProfession`
  - 触发 UI 反馈 (可选粒子特效)

**文件**: `src/game/systems/skill/AstrolabeSystem.cpp`

### Task 4.5: 实现属性应用
- [ ] 实现 `applyTalentStats()`:
  - 遍历 `activated_nodes`
  - 获取 `nodePoints[id]` 计算最终效果值
  - 应用 `modifiers`, `damage_modifiers`, `conversions`, `effects`

**文件**: `src/game/systems/skill/AstrolabeSystem.cpp`

**交付物**: 核心逻辑可正常解锁节点、累加亲和度、执行誓约。

---

## Phase 5: UI 渲染重构 (Sub-Track T5)

**目标**: 渲染六扇区布局。

### Task 5.1: 重构 AstrolabeRenderer::DrawStars
- [ ] 遍历 `TalentGraph.nodes` 替代 `AstrolabeMap.stars`
- [ ] 根据 `TalentNodeType` 选择节点大小和颜色
- [ ] 渲染 6 个 ProfessionStar (内环)

**文件**: `src/game/systems/ui/AstrolabeRenderer.cpp`

### Task 5.2: 实现节点状态渲染
- [ ] 实现 `getNodeColor()` 辅助函数:
  - Locked -> 灰色
  - Available -> 琥珀色 + 脉动
  - Activated (部分) -> 天蓝色
  - Activated (满) -> 金色
  - Core (非主修) -> 紫色 + 封印纹理

**文件**: `src/game/systems/ui/AstrolabeRenderer.cpp`

### Task 5.3: 移除连接线渲染
- [ ] 注释或删除 `DrawConnections()` 调用 (无前置节点依赖)
- [ ] 可选: 绘制轨道圆环 (视觉引导)

**文件**: `src/game/systems/ui/AstrolabeRenderer.cpp`

### Task 5.4: 渲染扇区分隔线 (可选)
- [ ] 从黑洞中心向外绘制 6 条分隔线，区分职业扇区
- [ ] 使用半透明渐变，避免视觉干扰

**文件**: `src/game/systems/ui/AstrolabeRenderer.cpp`

### Task 5.5: 渲染节点点数指示
- [ ] 在节点下方显示 "n/N" 点数文本
- [ ] 或使用填充环形指示器

**文件**: `src/game/systems/ui/AstrolabeRenderer.cpp`

### Task 5.6: GPU 特效 Shader
- [ ] 创建 `talent_node.fs` 节点特效 Shader
- [ ] 实现状态驱动的发光/脉动效果
- [ ] 实现封印纹理 (Procedural)
- [ ] 集成到 `AstrolabeRenderer`

**文件**: `assets/shaders/talent_node.fs`, `AstrolabeRenderer.cpp`

---

## Phase 6: UI 交互重构 (Sub-Track T6)

**目标**: 实现节点点击和誓约对话框。

### Task 6.1: 更新 UIAstrolabe 节点命中检测
- [ ] 遍历 `TalentGraph.nodes` 进行碰撞检测
- [ ] 添加 ProfessionStar 的碰撞检测

**文件**: `src/game/systems/ui/UIAstrolabe.cpp`

### Task 6.2: 实现节点点击逻辑
- [ ] 左键点击节点时:
  - 调用 `AstrolabeSystem::addPointToNode()`
  - 失败时显示原因提示 (点数不足 / Tier 门槛 / Core 封印)

**文件**: `src/game/systems/ui/UIAstrolabe.cpp`

### Task 6.3: 实现 ProfessionStar 点击
- [ ] 点击未誓约时的 ProfessionStar:
  - 显示誓约确认对话框

**文件**: `src/game/systems/ui/UIAstrolabe.cpp`

### Task 6.4: 实现誓约对话框
- [ ] 创建对话框 UI:
  - 职业名称和描述
  - 警示文本
  - 长按确认按钮 (2 秒计时)
  - 取消按钮
- [ ] 长按完成后调用 `AstrolabeSystem::takeVow()`

**文件**: `src/game/systems/ui/UIAstrolabe.cpp` 或新增 `UIVowDialog.cpp`

### Task 6.5: 更新 Tooltip
- [ ] 显示节点当前点数 / 最大点数
- [ ] 显示效果数值 (基于当前点数计算)
- [ ] 显示 Tier 门槛和当前亲和度

**文件**: `src/game/systems/ui/UIAstrolabe.cpp`

**交付物**: 完整的节点交互和誓约流程。

---

## Phase 7: 测试与集成 (Sub-Track T7)

**目标**: 全面验证功能。

### Task 7.1: 单元测试
- [ ] 测试 `TalentLayoutService` 坐标计算
- [ ] 测试 `AstrolabeSystem` 亲和度门槛逻辑
- [ ] 测试誓约机制 (canTakeVow, takeVow)
- [ ] 测试多点节点 (addPointToNode)

**文件**: `tests/AstrolabeSystemTests.cpp`

### Task 7.2: 集成测试
- [ ] 从新存档开始，完整流程测试:
  - 点亮 Tier 1 节点
  - 累积亲和度解锁 Tier 2
  - 誓约职业
  - 解锁 Core 节点
- [ ] 验证属性正确应用

### Task 7.3: 视觉验证
- [ ] 截图对比：六扇区布局
- [ ] 验证节点状态颜色
- [ ] 验证誓约对话框
- [ ] 验证 GPU 特效 (光晕/脉动)

### Task 7.4: 性能验证
- [ ] 验证大量节点 (100+) 时渲染帧率
- [ ] 确保渲染开销 < 1ms

**交付物**: 全部验收标准通过。

---

## 执行依赖图

```
         ┌─────┐
         │ T1  │ 数据模型
         └──┬──┘
       ┌────┼────┐
       ▼    ▼    ▼
    ┌────┐┌────┐┌────┐
    │ T2 ││ T3 ││ T4 │
    │布局││加载││系统│
    └──┬─┘└──┬─┘└──┬─┘
       │     │     │
       └──┬──┴──┬──┘
          ▼     ▼
       ┌────┐┌────┐
       │ T5 ││ T6 │
       │渲染││交互│
       └──┬─┘└──┬─┘
          └──┬──┘
             ▼
          ┌────┐
          │ T7 │
          │测试│
          └────┘
```

---

## 风险监控

| 风险点 | 检测时机 | 熔断条件 | 应对措施 |
|--------|----------|----------|----------|
| 数据模型编译错误 | T1 完成后 | 编译失败 | 回滚并简化类型 |
| 布局计算节点重叠 | T2 测试 | 可视化异常 | 调整角度分布算法 |
| JSON 解析崩溃 | T3 测试 | 加载失败 | 完善错误处理 |
| 属性未正确应用 | T4 测试 | 单元测试失败 | 调试 AttributePipeline |
| GPU 特效不兼容 | T5 测试 | 渲染异常 | 启用 CPU Fallback |

---

*计划版本: 1.1*
*最后更新: 2026-02-04*
