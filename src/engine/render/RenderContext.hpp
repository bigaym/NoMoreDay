#pragma once

// Forward declarations
namespace NoMoreDay::render {
class MDIRenderer;
}
namespace NoMoreDay::systems {
class GPUEntitySystem;
class GPUFlowFieldSystem;
} // namespace NoMoreDay::systems
class ResourceManager;

namespace NoMoreDay {

/**
 * @brief 渲染上下文聚合器。
 *
 * 用于传递渲染相关系统引用，避免直接访问单例。
 * 这是一个轻量级结构，仅存储指针/引用。
 */
struct RenderContext {
  systems::GPUEntitySystem *gpuEntitySystem = nullptr;
  systems::GPUFlowFieldSystem *gpuFlowFieldSystem = nullptr;
  render::MDIRenderer *mdiRenderer = nullptr;
  ResourceManager *resources = nullptr;

  // 快捷访问
  systems::GPUEntitySystem &GPU() { return *gpuEntitySystem; }
  systems::GPUFlowFieldSystem &Flow() { return *gpuFlowFieldSystem; }
  render::MDIRenderer &MDI() { return *mdiRenderer; }
  ResourceManager &Resources() { return *resources; }

  // 验证
  bool IsValid() const { return gpuEntitySystem && mdiRenderer && resources; }
};

} // namespace NoMoreDay
