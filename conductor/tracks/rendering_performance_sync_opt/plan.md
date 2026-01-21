# Implementation Plan: Rendering Performance & Sync Optimization

## 概述
本计划基于 `spec.md` 中的技术方案，分四个阶段实施，总预计工期 3-4 个工作日。

---

## Phase 1: 粒子系统异步化 (The Unblocking)
**预计耗时**: 0.5 天  
**优先级**: P0 (阻塞性能瓶颈)

### 1.1 准备工作
- [ ] **阅读现有代码**: 熟悉 `GPUParticleSystem::Update()` 流程 (L240-337)
- [ ] **确认基线**: 运行游戏，记录 `GPUParticle_Update` 当前耗时

### 1.2 核心实现
- [ ] **创建双缓冲原子计数器**
  - 文件: `src/engine/render/GPUParticleSystem.hpp`
  - 变更:
    ```cpp
    core::ComputeBuffer m_atomicBufferPing;
    core::ComputeBuffer m_atomicBufferPong;
    uint32_t m_lastKnownAliveCount = 0;
    bool m_atomicPingPong = false;
    ```

- [ ] **重构 Update() 逻辑**
  - 文件: `src/engine/render/GPUParticleSystem.cpp`
  - 步骤:
    1. 在 Dispatch 前，从"上一帧的缓冲区"异步读取计数 (非阻塞)
    2. 使用 `m_lastKnownAliveCount` 替代同步读取的 `aliveCount`
    3. Dispatch 后交换 Ping/Pong 标志
  - 关键代码:
    ```cpp
    // 读取上一帧结果 (GPU 已完成，无阻塞)
    core::ComputeBuffer& readBuffer = m_atomicPingPong ? m_atomicBufferPong : m_atomicBufferPing;
    core::ComputeBuffer& writeBuffer = m_atomicPingPong ? m_atomicBufferPing : m_atomicBufferPong;
    
    readBuffer.Read(&m_lastKnownAliveCount, sizeof(uint32_t));
    
    // ... Dispatch 使用 writeBuffer ...
    
    m_atomicPingPong = !m_atomicPingPong;
    ```

- [ ] **添加发射限流**
  - 当 `m_lastKnownAliveCount > 0.9 * m_maxParticles` 时，丢弃新发射请求并记录警告

### 1.3 验证
- [ ] **编译通过**: `.\build.bat`
- [ ] **功能测试**: 游戏中触发大量粒子效果，确认无可见异常
- [ ] **性能验证**: 
  ```
  日志输出: [ScopedTimer] GPUParticle_Update: XXXμs
  目标: < 500μs (100k 粒子)
  ```
- [ ] **RenderDoc 确认**: 抓帧验证无 `glClientWaitSync` 调用

---

## Phase 2: 飘字系统重构 (The Batching)
**预计耗时**: 1.5 天  
**优先级**: P1 (中等影响)

### 2.1 资源准备
- [ ] **生成字形图集**
  - 工具: 使用 Python 脚本或 BMFont
  - 字符集: `0123456789+-暴击闪避格挡`
  - 输出: `assets/textures/popup_glyphs.png` (512x64, RGBA8)
  - 元数据: `assets/textures/popup_glyphs.json` (UV 坐标)

### 2.2 数据结构
- [ ] **定义 GPU 实例结构**
  - 文件: `src/engine/render/GPUData.hpp`
  - 新增:
    ```cpp
    namespace Constants::Render::Popup {
        constexpr int MAX_POPUPS = 2048;
        constexpr int MAX_GLYPHS_PER_POPUP = 8;
        constexpr float GLYPH_WIDTH = 32.0f;
        constexpr float GLYPH_HEIGHT = 48.0f;
    }
    
    struct GPUPopupInstance {
        Vector2 position;
        Vector2 velocity;
        float timer;
        float lifeTime;
        uint32_t glyphData;  // packed: start << 16 | count
        uint32_t colorPacked; // RGBA8
        uint32_t flags;       // bit0: isCrit, bit1: isStatus
        float _padding;
    };
    static_assert(sizeof(GPUPopupInstance) == 48, "GPUPopupInstance must be 48 bytes for alignment");
    ```

### 2.3 着色器开发
- [ ] **Vertex Shader** (`assets/shaders/popup.vert`)
  - 输入: 实例 ID, 四边形顶点索引
  - 输出: 世界坐标, 纹理 UV, 颜色, 透明度
  - 逻辑:
    1. 从 SSBO 读取 `GPUPopupInstance[gl_InstanceID]`
    2. 计算 Billboard 位置 (Camera-facing)
    3. 基于 `timer/lifeTime` 计算透明度衰减
    4. 若 `isCrit`，应用脉冲缩放: `scale = 1.2 + 0.3 * sin(timer * 20)`

- [ ] **Fragment Shader** (`assets/shaders/popup.frag`)
  - 采样字形图集
  - 应用顶点颜色和透明度
  - SDF 抗锯齿 (可选)

### 2.4 渲染器实现
- [ ] **创建 PopupRenderer 类**
  - 文件: `src/engine/render/PopupRenderer.hpp`, `PopupRenderer.cpp`
  - 职责:
    1. 管理 SSBO (`PersistentBuffer`)
    2. 加载着色器和字形图集
    3. 提供 `Emit(position, amount, isCrit, color)` 接口
    4. `Update(dt)`: 更新所有飘字的 timer
    5. `Render(mvp)`: 执行 Instanced Draw

- [ ] **集成到渲染管线**
  - 文件: `src/engine/render/RenderSystem.cpp`
  - 替换现有的 `DamagePopupManager::Get().Draw()` 调用
  - 在后处理前调用 `PopupRenderer::Get().Render()`

### 2.5 迁移与兼容
- [ ] **修改 EffectSystem 调用点**
  - 将 `DamagePopupManager::Get().Emit()` 替换为 `PopupRenderer::Get().Emit()`
  - 保持接口兼容，最小化修改范围

- [ ] **保留 CPU 路径** (用于状态文本)
  - 中文状态文本 (`"中毒"`, `"眩晕"`) 仍使用 `DrawTextEx` 渲染
  - 数字伤害使用 GPU 实例化

### 2.6 验证
- [ ] **编译通过**
- [ ] **视觉测试**: 伤害飘字显示正确，暴击有动画效果
- [ ] **性能验证**:
  ```
  RenderDoc: Draw Call 数量 (飘字相关) ≤ 2
  日志: PopupRenderer_Render: < 200μs
  ```

---

## Phase 3: 实体同步加速 (The DOD)
**预计耗时**: 1 天  
**优先级**: P1 (中等影响)

### 3.1 脏标记系统
- [ ] **定义 DirtyTransform 组件**
  - 文件: `src/game/components/Common.hpp`
  - 新增:
    ```cpp
    struct DirtyTransform {
        bool isDirty = true;
    };
    ```

- [ ] **在物理更新后设置脏标记**
  - 文件: `src/engine/render/GPUEntitySystem.cpp` (`SyncBack` 函数)
  - 逻辑: 当 `|newPos - oldPos| > DIRTY_THRESHOLD` 时，设置 `isDirty = true`
  - 常量: `DIRTY_THRESHOLD = 0.5f` (定义于 `Common.hpp`)

### 3.2 分块更新实现
- [ ] **块管理数据结构**
  - 文件: `src/engine/render/GPUEntitySystem.hpp`
  - 新增:
    ```cpp
    static constexpr int BLOCK_SIZE = 1024;
    std::vector<uint32_t> m_blockDirtyCounts;  // [numBlocks]
    std::vector<bool> m_blockNeedsUpdate;       // [numBlocks]
    ```

- [ ] **重构 Update() 使用分块逻辑**
  - 步骤:
    1. 第一遍扫描: 统计每个块的 `dirtyCount`
    2. 第二遍写入: 仅对 `blockNeedsUpdate[i]` 的块执行 SSBO 更新
  - 伪代码:
    ```cpp
    // Pass 1: Count dirty entities per block
    for (auto entity : group) {
        int blockIdx = gpuIndex / BLOCK_SIZE;
        if (registry.all_of<DirtyTransform>(entity)) {
            m_blockDirtyCounts[blockIdx]++;
        }
    }
    
    // Pass 2: Update only dirty blocks
    for (int blk = 0; blk < numBlocks; ++blk) {
        if (m_blockDirtyCounts[blk] == 0) continue;
        // Update SSBO for this block range
        void* ptr = m_persistentBuffer.BeginWriteRange(blk * BLOCK_SIZE * sizeof(GPUEntity), BLOCK_SIZE * sizeof(GPUEntity));
        ...
    }
    ```

### 3.3 SIMD 优化 (可选)
- [ ] **评估 AVX2 加速收益**
  - 仅当 Profiler 显示 memcpy 为瓶颈时启用
  - 使用 `_mm256_storeu_ps` 批量写入 Vector2 数据

### 3.4 验证
- [ ] **编译通过**
- [ ] **功能测试**: 实体位置更新正确，无抖动
- [ ] **性能验证**:
  ```
  场景: 20k 实体，50% 静止
  目标: GPUEntity_Update < 1.5ms (相比基线 2.5ms)
  ```

---

## Phase 4: 验证与验收 (The Audit)
**预计耗时**: 0.5 天  
**优先级**: P0 (质量保证)

### 4.1 基准测试
- [ ] **创建性能测试场景**
  - 100,000 粒子持续发射
  - 500 伤害飘字同屏
  - 20,000 实体，50% 移动

- [ ] **记录优化前后数据**
  | 指标 | 优化前 | 优化后 | 提升 |
  |------|--------|--------|------|
  | GPUParticle_Update | | | |
  | PopupRenderer_Render | N/A | | |
  | GPUEntity_Update | | | |
  | 帧率 1% Low | | | |

### 4.2 回归测试
- [ ] **运行单元测试**: `./build/bin/NoMoreDayTests.exe`
- [ ] **人工验收**:
  - 粒子效果正常（墨迹、火焰、金币）
  - 伤害飘字显示正确（数字、颜色、动画）
  - 怪物移动流畅，无位置跳变
  - 无崩溃或内存泄漏 (运行 10 分钟压力测试)

### 4.3 二次审计
- [ ] **使用 `architecture-auditor` 技能**
  - 确认无新引入的同步阻塞
  - 确认 SSBO 访问符合 DOD 原则
  - 确认无主循环堆分配

### 4.4 文档更新
- [ ] **更新 MEMORY**: 记录优化成果
- [ ] **关闭 Track**: 将 `metadata.json` 的 `status` 改为 `completed`

---

## 依赖关系图

```
Phase 1 (粒子异步化)
     │
     ├──► Phase 2 (飘字实例化) ──┐
     │                           │
     └──► Phase 3 (实体同步优化)─┴──► Phase 4 (验收)
```

Phase 2 和 Phase 3 可并行执行。Phase 4 需等待所有前置阶段完成。

---

## 风险登记册

| 风险 | 概率 | 影响 | 缓解措施 | 责任人 |
|------|------|------|----------|--------|
| 双缓冲导致粒子爆发时过度发射 | 低 | 中 | 添加软限流，日志告警 | - |
| 字形图集生成工具链问题 | 中 | 低 | 准备备用方案：手工 Photoshop | - |
| SIMD 优化引入平台兼容性问题 | 低 | 高 | 使用编译时检测，提供标量 Fallback | - |
