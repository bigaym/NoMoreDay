# 🌌 星系天赋系统 VFX 与代码架构深度优化实施计划 (V1.0)

> **Track ID**: `astrolabe-vfx-polish_20260204`
> **预计工时**: 12h
> **风险等级**: 🟡 中 (涉及着色器编写与构建系统修复)

---

## 阶段 1: 环境固化与测试集成 (Sub-Track T1)
**目标**: 解决 CMake 构建系统未检测到新测试的问题。

### Task 1.1: 修复测试编译 ⚡
- [x] 修改 `tests/CMakeLists.txt` 或执行 `cmake --build build --target clean` 后重新配置。 (已完成)
- [x] 运行 `NoMoreDayTests.exe` 验证 `AstrolabeSystemTests` 和 `TalentLayoutTests` 存在并全绿。 (已完成)

---

## 阶段 2: GPU 渲染强化 (Sub-Track T2)
**目标**: 实现高级节点特效。

### Task 2.1: 编写 `talent_node.fs` 🎨
- [x] 在 `assets/shaders/` 下创建 `talent_node.fs`。 (已完成)
- [x] 实现 spec 要求的四种状态材质 (Pulsing, Ring, Glow, Seal Noise)。 (已完成)

### Task 2.2: 集成到渲染路径
- [x] 修改 `AstrolabeRenderer` 加载并应用该着色器。 (已完成)
- [x] 替换原生 Raylib 绘图指令。 (已完成)

---

## 阶段 3: 动力学反馈系统 (Sub-Track T3)
**目标**: 增加灵力流动机机制。

### Task 3.1: 能量流向粒子设计
- [x] 接入 `GPUParticleSystem`。 (已完成)
- [x] 在 `UIAstrolabe` 交互成功时，调用粒子发射器发送从职业星到节点的粒子。 (已完成)

### Task 3.2: 爆发效果实现
- [x] 点满节点时，以节点为中心触发金色冲击波特效。 (已完成)

---

## 阶段 4: UI 代码重构与清理 (Sub-Track T4)
**目标**: 模块化 UIAstrolabe。

### Task 4.1: 函数组件化
- [x] 拆分 `DrawInternal` 为 `HandleCameraInput`, `HandleInteraction`, `DrawOverlay`, `DrawTooltips`。 (已完成)
- [x] 确保单一职责原则 (SRP)。 (已完成)

### Task 4.2: 交付验证
- [/] 执行全量验收（包含视觉、性能、逻辑）。 (正在进行：逻辑验证已完成，Shader 已优化)

---
*计划版本: 1.0*  
*最后更新: 2026-02-04*
