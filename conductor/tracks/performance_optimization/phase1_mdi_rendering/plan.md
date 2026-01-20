# Phase 1: GPU-Driven MDI Rendering 实施计划

**Track ID**: `performance_optimization/phase1_mdi_rendering`  
**状态**: 📋 Planned  
**预计工时**: 3-4 天

---

## 任务分解 (Task Breakdown)

### Task 1.1: 创建 MDIRenderer 核心类 ⬜
**优先级**: Critical  
**预计时间**: 4h

**操作**:
1. 创建 `src/engine/render/MDIRenderer.hpp`
2. 创建 `src/engine/render/MDIRenderer.cpp`
3. 实现以下接口:
   - `Init(uint32_t maxEntities)` - 初始化 Buffer 和 Shader
   - `Shutdown()` - 释放 GPU 资源
   - `UpdateInstances(span<GPUInstanceData>)` - 上传实例数据
   - `Cull(const glm::vec4 frustum[4])` - 执行 GPU 剔除
   - `Render(const glm::mat4& vp)` - 执行间接绘制

**验收条件**:
- [ ] 编译通过
- [ ] 单元测试框架就位

---

### Task 1.2: 实现 GPU-Side DrawIndirect Command Buffer ⬜
**优先级**: Critical  
**预计时间**: 2h

**操作**:
1. 在 `MDIRenderer` 中创建 `m_commandBuffer`
2. 实现 `ResetCommand()` 方法 (使用 Compute Shader 将 instanceCount 归零)
3. 封装 `glDrawArraysIndirect` 调用

**代码片段**:
```cpp
void MDIRenderer::ResetCommand() {
    // 使用 Compute Shader 清零，避免 CPU-GPU 往返
    rlEnableShader(m_resetShader.id);
    m_commandBuffer.BindBase(0);
    rlComputeShaderDispatch(1, 1, 1);
    utils::GPUUtils::MemoryBarrier();
}
```

---

### Task 1.3: 编写 cull.compute Shader ⬜
**优先级**: Critical  
**预计时间**: 3h

**操作**:
1. 创建 `assets/shaders/cull.compute`
2. 实现视锥剔除逻辑 (2D AABB vs 4 平面)
3. 使用 `atomicAdd` 填充可见索引

**关键点**:
- 处理边界情况 (实体在屏幕边缘)
- 考虑实体半径扩展 Frustum 检测范围

---

### Task 1.4: 编写 entity_mdi.vert/frag Shader ⬜
**优先级**: Critical  
**预计时间**: 2h

**操作**:
1. 创建 `assets/shaders/entity_mdi.vert`
2. 创建 `assets/shaders/entity_mdi.frag`
3. 修改顶点着色器从 SSBO 读取实例数据
4. 支持 Texture Atlas 索引

---

### Task 1.5: 集成到 GPUEntitySystem ⬜
**优先级**: High  
**预计时间**: 3h

**操作**:
1. 在 `GPUEntitySystem::Init()` 中初始化 `MDIRenderer`
2. 在 `GPUEntitySystem::Update()` 中同步数据到 MDIRenderer
3. 替换 `GPUEntitySystem::Render()` 调用 MDIRenderer
4. 保留原有 `Render()` 为 `RenderLegacy()`

**回滚策略**:
```cpp
void GPUEntitySystem::Render() {
    if (Config::Get().useMDI && m_mdiRenderer.IsSupported()) {
        m_mdiRenderer.Render(m_viewProj);
    } else {
        RenderLegacy();
    }
}
```

---

### Task 1.6: 创建集成测试 ⬜
**优先级**: High  
**预计时间**: 2h

**操作**:
1. 创建 `tests/integration/MDIRenderTest.hpp`
2. 测试场景:
   - 1000 实体随机位置，验证渲染结果与 Legacy 一致
   - 10000 实体，验证无渲染异常
   - 边界测试：0 实体、所有实体不可见

---

### Task 1.7: 性能 Benchmark ⬜
**优先级**: Medium  
**预计时间**: 2h

**操作**:
1. 使用 Tracy Profiler 标记 `MDIRenderer::Cull` 和 `Render`
2. 对比 Legacy 和 MDI 模式下:
   - CPU 帧时间
   - Draw Call 数量
   - GPU Compute 耗时

---

## 依赖关系

```
Task 1.1 
    │
    ├──► Task 1.2
    │        │
    │        ▼
    │    Task 1.3 ──► Task 1.5
    │        
    └──► Task 1.4 ──► Task 1.5 ──► Task 1.6 ──► Task 1.7
```

---

## 验收清单

- [ ] `glMultiDrawArraysIndirect` 成功调用
- [ ] 视觉输出与 Legacy 模式一致
- [ ] RenderDoc 确认 Draw Call = 1
- [ ] 无 OpenGL 错误 (Validation Layer Clean)
- [ ] 10k 实体场景 CPU Draw 开销 < 0.5ms

---

## 回滚计划

若遇到严重问题:
1. 将 `Config::useMDI` 设为 false
2. `GPUEntitySystem::Render()` 自动回退到 `RenderLegacy()`
3. MDI 代码保留，待问题修复后重新启用

---

*规划者: Gemini (Skill: designer)*
