# Specification: Rendering Foundation Migration (Phase 0)

> **Track ID**: `rendering_foundation_migration_20260212`
> **Status**: Draft
> **Parent**: [rendering_system_progress.md](../../rendering_system_progress.md)

## 1. 背景与目标
当前 `RenderSystem` 是一个单体巨类，`render()` 函数过长且难以扩展。为了实现 GPU 渲染系统 2.0（HDR、Bloom、动态光照），必须先将渲染逻辑解耦为 **RenderGraph** 架构。

**目标**:
- 引入 `RenderGraph` 调度器。
- 实现 `TransientResourcePool` 降低 FBO 开销。
- 建立 `QualityTierManager` 驱动的画质分级。
- 将 `RenderSystem` 拆分为原子化的 `RenderPass`。

## 2. 技术设计

### 2.1 目录结构
```text
src/engine/render/
├── graph/
│   ├── RenderGraph.hpp/cpp
│   ├── RenderPass.hpp
│   └── RenderContext.hpp
├── resources/
│   ├── TransientResourcePool.hpp/cpp
│   └── FramebufferPool.hpp
├── core/
│   ├── QualityTierManager.hpp/cpp
│   └── RenderConstants.hpp
└── passes/
    ├── ScenePass.hpp/cpp
    ├── VFXPass.hpp/cpp
    ├── UIWorldPass.hpp/cpp
    └── CompositePass.hpp/cpp
```

### 2.2 核心 API 契约

#### RenderPass 基类
```cpp
namespace nmd {
class RenderGraphBuilder;
class RenderContext;

class RenderPass {
public:
    virtual ~RenderPass() = default;
    virtual void Setup(RenderGraphBuilder& builder) = 0;
    virtual void Execute(RenderContext& context) = 0;
    virtual const char* GetName() const = 0;
};
}
```

#### QualityTier 配置
```cpp
enum class QualityTier {
    Low,
    Medium,
    High,
    Ultra
};

struct RenderConfig {
    bool bloomEnabled;
    int maxParticles;
    // ... 详见 GPU_Rendering_System_2.md
};
```

### 2.3 资产契约
- 本阶段不涉及新资产，但 `QualityTierManager` 将读取 `settings.json`（如果存在）或自动检测 GPU 性能。

## 3. 验收标准 (DoD)
- [ ] 游戏画面与重构前完全一致（像素级）。
- [ ] `RenderSystem.cpp` 的 `render` 函数被精简为 Graph 构建与执行。
- [ ] 性能在标定机上无退步。
- [ ] 通过 `RenderingBenchmark` 测试。
