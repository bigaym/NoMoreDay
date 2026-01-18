# Technical Specification: Constant & Magic Number Management

**Track ID:** `const_refactor`
**Revision:** 1.0

## 1. 架构目标
消除代码库中的硬编码字面量（Magic Numbers），建立“单一事实来源”（SSOT），确保逻辑一致性并简化后期平衡调整。

## 2. 常量存放规范 (Standard Placement)

### 2.1 游戏逻辑常量 (Gameplay Logic)
**位置**: `src/game/components/Common.hpp`
**命名空间**: `NoMoreDay::Constants`
**适用范围**:
*   **数值上限 (Caps)**: 抗性上限、CDR上限、移速上限。
*   **战斗公式系数**: 护甲衰减系数、属性转化比例、默认暴击伤害。
*   **AI 行为参数**: 警戒范围、休眠阈值、更新间隔。
*   **物理参数**: 碰撞半径偏移、速度归一化阈值 (Epsilon)。

### 2.2 渲染与视觉常量 (Rendering & Visuals)
**位置**: `src/engine/render/GPUData.hpp`
**命名空间**: `NoMoreDay::components::Colors` (颜色) / `NoMoreDay::Constants::Render` (其他)
**适用范围**:
*   **UI 调色板**: 面板背景色、边框色、各级文本颜色。
*   **特效参数**: 粒子基础缩放、颜色渐变值。
*   **GPU 数据结构定义**: 必须与 Shader 布局严格匹配的 Struct。

## 3. 详细变更规范 (Detailed Contract)

### 3.1 物理与逻辑阈值对齐
定义 `Physics::EPSILON_VELOCITY = 0.001f`。
所有 `if (vel > 0.01f)` 或 `if (abs(vx) > 0.001f)` 必须统一替换为该常量。

### 3.2 属性上限同步
确保 `Combat::Cap` 命名空间中的值在所有逻辑判断处被引用。
禁止在 `GameplayState.cpp` 等 UI/State 层进行二次硬编码限制。

### 3.3 UI 颜色标准化
所有 `DrawRectangle(..., Color{...})` 必须替换为 `Colors::UI_BORDER_DEFAULT` 等语义化常量。

## 4. 编码准则
1.  **禁止字面量**: 除 `0`, `1`, `0.0f`, `1.0f` 外，禁止在逻辑表达式中使用任何无名数字。
2.  **Constexpr 优先**: 所有常量必须使用 `constexpr` 声明以确保编译时计算。
3.  **内联定义**: 位于命名空间内的常量应使用 `inline constexpr` (C++17+) 以防止多重定义。

## 5. 决策记录 (Architecture Decisions)
*   **AD-001**: 严禁在 `.cpp` 文件顶部定义局部逻辑常量。所有影响游戏平衡的数值必须进入 `Common.hpp`。
*   **AD-002**: 颜色值统一采用 `0xRRGGBBAA` 宏或 `Colors::FromHex` 辅助函数，严禁在渲染循环外构造 `Color` 对象。
