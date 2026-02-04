# 🌌 星系天赋系统 VFX 与代码架构深度优化规格说明书 (V1.0)

> **Track ID**: `astrolabe-vfx-polish_20260204`
> **目标**: 完成星盘系统遗留的 GPU 着色器特效、粒子反馈机制以及 UI 代码模块化重构。
> **参考**: [星系天赋系统重构审计报告 2026-02-04](../../../设计文档/星系天赋系统重构审计报告.md)

---

## 1. 渲染架构升级 (Rendering Upgrade)

本项目要求将目前的 CPU 基础绘图路径完整迁移至 GPU Shader，并引入粒子动力学反馈。

### 1.1 节点高级着色器 (`talent_node.fs`)
**路径**: `assets/shaders/talent_node.fs`

- **材质表现**:
  - **Available (琥珀色脉动)**: 利用 `sin(uTime * 3.0)` 驱动的琥珀色 (#FFD700) 呼吸光晕。
  - **Activated (天蓝色进度)**: 节点外周的一圈发光环，长度由 `uProgress` 决定。
  - **FullyActivated (饱和金色)**: 饱和金色 (#FFD700) 并带有放射状的“神性”微光。
  - **Sealed (紫色封印)**: 紫色 (#800080) 材质，覆盖一层由 `fragCoord` 生成的动态噪点纹样，模拟“深渊封印”。
- **Uniform 契约**:
```glsl
uniform float uTime;
uniform int uStatus;       // 0=Locked, 1=Available, 2=Activated, 3=FullyActivated, 4=Sealed
uniform float uProgress;   // 0.0 ~ 1.0 (Points/MaxPoints)
uniform vec4 uBaseColor;
```

### 1.2 粒子动力学反馈 (Particle Dynamics)
**实现**: 结合 `GPUParticleSystem`。

- **能量吸入 (Energy Inflow)**: 当点击节点投入点数时，从 `ProfessionStar` 产生一组粒子，加速流向目标 `TalentNode`。
- **超新星爆发 (Supernova)**: 当节点达到 `FullyActivated` 瞬间，产生一次向外扩散的金色冲击波粒子。

---

## 2. UI 系统重构 (UI Refactoring)

### 2.1 `UIAstrolabe` 职责横向解耦
将 `DrawInternal` 的宏大叙事分解为原子化组件：

1. **输入控制器 (`HandleInput`)**:
   - 滚轮缩放与鼠标平移逻辑。
   - 节点与星体的 `CheckCollisionPointCircle` 碰撞检测。
2. **交互处理器 (`ProcessInteraction`)**:
   - `addPointToNode` 的调用。
   - 失败消息 (`s_failMessage`) 的计时器管理。
3. **分层渲染器 (`RenderLayers`)**:
   - `DrawBackgroundShader()`: 背景黑洞。
   - `DrawConnectiveStructure()`: 绘制职业扇区分隔线与轨道环。
   - `DrawDynamicTooltips()`: 高级悬浮窗。

---

## 3. 质量与回归契约 (Quality Contract)

### 3.1 自动化测试集成
- **修复**: 在 `tests/CMakeLists.txt` 中显式添加新测试文件或强制重新生成构建系统。
- **AC**: `NoMoreDayTests.exe` 必须成功显示并运行 `[Unit] AstrolabeSystemTests` 下的所有 6 个用例。

---

## 4. 验收标准 (Acceptance Criteria)

- [ ] **AC1**: 节点渲染完全由 `talent_node.fs` 处理，且具备脉动、进度环、封印噪点效果。
- [ ] **AC2**: 交互时粒子表现符合“能量从主星流向节点”的视觉逻辑。
- [ ] **AC3**: 节点点满时触发超新星扩散特效。
- [ ] **AC4**: `UIAstrolabe` 代码结构清晰，单函数行数控制在合理范围。
- [ ] **AC5**: 确认修复了 CMake 构建问题，单元测试全绿运行。

---
*规格版本: 1.0*  
*最后更新: 2026-02-04*
