# Spec: 敌人渲染架构重构 (CPU 权威化)

## 1. 问题陈述 (Problem Statement)
当前 `GPUEntitySystem` 采用“GPU 物理更新 + CPU 同步回传 (SyncBack)”的架构，存在以下严重问题：
- **控制权冲突**: CPU AI 逻辑与 GPU 物理位移竞争同一份坐标数据。
- **高延迟 (N-2 延迟)**: 使用 `PersistentBuffer` 的三缓冲机制导致 CPU 获取的坐标是 2 帧前的，造成视觉上的“幽灵位移”和抖动。
- **难以调试**: GPU 端的物理错误难以通过标准 C++ 工具追踪。

## 2. 目标架构 (Target Architecture)
转向“CPU 权威 (Source of Truth) + GPU 加速渲染”模式。

### 核心变更点：
- **CPU 权威**: `entt::registry` 中的 `Position` 和 `Velocity` 是唯一真理。
- **单向数据流**: 
  1. CPU 运行 AI 和物理系统。
  2. CPU 将所有 Entity 数据一次性写入 GPU SSBO。
  3. GPU 仅执行 **Culling (剔除)** 和 **LOD 计算**。
  4. GPU 生成 Indirect Draw 命令并渲染。
- **移除 SyncBack**: 彻底删除从 GPU 读取数据回写 CPU 组件的逻辑。
- **0 帧延迟**: 确保当前帧提交的数据在当前帧渲染。

## 3. 技术定义 (Technical Definitions)

### GPU 数据结构 (GPUData.hpp)
```cpp
struct GPUInstanceData {
    vec2 position;
    float rotation;
    float scale;
    uint32_t typeID;
    uint32_t statusFlags; // 状态位，如是否受击等
};
```

### 控制流变更
1. **Logic Phase**: CPU 更新所有敌人位置。
2. **Prep Phase**: `GPUEntitySystem::Submit` 将数据拷贝至 `SSBO_T0`。
3. **GPU Phase**: 
   - `cull.compute` 读取 `SSBO_T0` -> 写入 `DrawIndirectBuffer`。
   - `render.vert` 读取 `SSBO_T0` 使用 `gl_InstanceID` 索引。

## 4. 验收清单 (Acceptance Criteria)
- [ ] 移除 `GPUEntitySystem::SyncBack` 及其相关 Buffer 读取。
- [ ] 敌人移动不再出现 2 帧延迟的视觉抖动。
- [ ] MDI 渲染正确，剔除功能依然有效。
- [ ] 移除 `physics.compute` 中不必要的物理模拟，保留仅用于 VFX 的部分（如需要）。
