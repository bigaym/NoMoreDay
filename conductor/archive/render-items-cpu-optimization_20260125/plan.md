# Rendering Items CPU Optimization Plan

## Phase 1: Visible Cache & UI Optimization (The CPU Cull)
**目标**: 消除 `UISystem` 中的 $O(N)$ 遍历和冗余坐标转换。
- [x] **1.1 定义缓存结构**: 在 `RenderSystem.hpp` 中定义 `VisibleItemCache` 静态结构。
- [x] **1.2 填充缓存**: 在 `RenderSystem::render` 的 `Item Collection Pass` 中，将通过视锥剔除的物品 ID 和屏幕坐标存入缓存。
- [x] **1.3 UI 复用**: 重构 `UISystem::Draw`，使其直接遍历 `VisibleItemCache` 进行鼠标交互检测，移除 `GetWorldToScreen2D` 调用。
- [x] **1.4 验证**: 运行游戏，检查 `UISystem::GroundHover` 计时器是否大幅下降（预期 < 0.1ms）。

## Phase 2: Beam Instancing (The Batching)
**目标**: 消除 Immediate Mode 渲染，减少 Draw Call 和 Driver Overhead。
- [x] **2.1 Shader 编写**: 创建 `assets/shaders/ui/beam_instanced.vert` 和 `.frag`。
- [x] **2.2 数据结构**: 定义 `GPUBeamInstance` (std430, binding 5)。
- [x] **2.3 渲染重构**:
    - 在 `RenderSystem` 中移除 `DrawRectangleGradientV` / `DrawCircleGradient` 循环。
    - 替换为 `s_beamBuffer` 填充逻辑。
    - 在 `ItemsLabels` pass 之后，执行 `Instanced Beam Draw`。
- [x] **2.4 Buffer Management**: 使用 `ComputeBuffer` 和 `OrphanAndUpload` 策略管理 Beam 数据。

## Phase 3: Cleanup & Final Polish
- [x] **3.1 移除计时器**: 在确认性能达标后，清理 `ScopedTimer` 代码（或保留在 `LOG_LIMITED` 下）。
- [x] **3.2 代码审计**: 检查是否有内存泄漏或野指针（特别是静态缓存的清理）。

## 4. 风险评估
- **静态缓存同步**: `RenderSystem` 必须在 `UISystem` 之前运行，或者由 `Game::render` 显式控制顺序。目前顺序是 `RenderSystem::render` -> `UISystem::Draw`，符合要求。但需注意 Frame Delay 导致的坐标错位（当前方案是在同一帧计算和使用，无延迟）。
