# Implementation Plan: Rendering Performance & Sync Optimization

## 概述
本计划基于 `spec.md` 中的技术方案，分四个阶段实施，总预计工期 3-4 个工作日。

---

## Phase 1: 粒子系统异步化 (The Unblocking)
**预计耗时**: 0.5 天  
**优先级**: P0 (阻塞性能瓶颈)

### 1.1 准备工作
- [x] **阅读现有代码**: 熟悉 `GPUParticleSystem::Update()` 流程 (L240-337)
- [x] **确认基线**: 运行游戏，记录 `GPUParticle_Update` 当前耗时

### 1.2 核心实现
- [x] **创建双缓冲原子计数器**
  - 文件: `src/engine/render/GPUParticleSystem.hpp`
  - 变更:
    ```cpp
    core::ComputeBuffer m_atomicBufferPing;
    core::ComputeBuffer m_atomicBufferPong;
    uint32_t m_lastKnownAliveCount = 0;
    bool m_atomicPingPong = false;
    ```

- [x] **重构 Update() 逻辑**
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

- [x] **添加发射限流**
  - 当 `m_lastKnownAliveCount > 0.9 * m_maxParticles` 时，丢弃新发射请求并记录警告

### 1.3 验证
- [x] **编译通过**: `.\build.bat`
- [x] **功能测试**: 游戏中触发大量粒子效果，确认无可见异常
- [x] **性能验证**: 
  ```
  日志输出: [ScopedTimer] GPUParticle_Update: XXXμs
  目标: < 500μs (100k 粒子)
  ```
- [x] **RenderDoc 确认**: 抓帧验证无 `glClientWaitSync` 调用

---

## Phase 2: 飘字系统重构 (The Batching)
**预计耗时**: 1.5 天  
**优先级**: P1 (中等影响)

### 2.1 资源准备
- [x] **生成字形图集**
  - 工具: 使用 Python 脚本或 BMFont
  - 字符集: `0123456789+-暴击闪避格挡`
  - 输出: `assets/textures/popup_glyphs.png` (512x64, RGBA8)
  - 元数据: `assets/textures/popup_glyphs.json` (UV 坐标)

### 2.2 数据结构与物理重构 (Response to Audit Warning)
- [x] **定义 GPU 实例结构**
  - 文件: `src/engine/render/GPUData.hpp`
  - 变更:
    - 移除 `velocity` (移至 GPU 计算或仅在发射时传递初速度)
    - 确保 48 字节对齐
    ```cpp
    struct GPUPopupInstance {
        Vector2 startPosition; // 初始位置
        Vector2 velocity;      // 初速度
        float startTime;       // 产生时间
        float lifeTime;        // 生命周期
        uint32_t glyphData;    // content
        uint32_t colorPacked;  // color
        uint32_t flags;        // flags
        float _padding;
    };
    ```

### 2.3 字形图集策略 (The Atlas Decision)
- [x] **采用固定网格图集 (Fixed Grid Atlas)**
  - **决策**: 放弃使用 Raylib `Font` 加载，转而使用美术预焙焙的 512x64 序列帧图集。
  - **理由**:
    1. **性能**: Shader 可通过 `(instanceID / 10, instanceID % 10)` 直接计算 UV，无需额外的 Buffer 存储 UV 数据。
    2. **美学**: 允许使用带描边、渐变、发光的精美艺术数字，而非单调的 TTF 字体。
  - **实现**:
    - 网格: 10列 x 1行 (0-9) + 额外符号
    - Shader UV 公式: `uv.x = (glyphIndex + vertexUV.x) / columnCount`

### 2.4 着色器与渲染器开发
- [x] **Vertex Shader** (`assets/shaders/popup.vert`)
  - **移入物理计算**: `pos = startPos + velocity * t + 0.5 * gravity * t * t`
  - 移除 CPU 端的每帧物理更新，仅在 Emit 时上传一次数据，后续由 `uTime` 驱动。

- [x] **PopupRenderer 实现**
  - 集成 `Loose Texture` (atlas)
  - 实现 `Emit()`: 写入 RingBuffer
  - 实现 `Render()`: 绑定 Atlas 和 Uniforms (Time)

---

## Phase 3: 实体同步加速 (The DOD)
**预计耗时**: 1 天  
**优先级**: P1 (中等影响)

### 3.1 脏标记系统
- [x] **定义 DirtyTransform 组件**
  - 文件: `src/game/components/Common.hpp`
  - 新增:
    ```cpp
    struct DirtyTransform {
        bool isDirty = true;
    };
    ```

- [x] **在物理更新后设置脏标记**
  - 文件: `src/engine/render/GPUEntitySystem.cpp` (`SyncBack` 函数)
  - 逻辑: 当 `|newPos - oldPos| > DIRTY_THRESHOLD` 时，设置 `isDirty = true`
  - 常量: `DIRTY_THRESHOLD = 0.5f` (定义于 `Common.hpp`)

### 3.2 分块更新实现 (Response to Audit Warning)
- [x] **激活增量更新逻辑** (Fix "Always Update" Issue)
  - 文件: `src/engine/render/GPUEntitySystem.cpp`
  - 变更: 将 `Update` 中的无条件 memcpy 修改为基于 ShadowBuffer 的增量更新与批量上传。
  - 关键修复:
    ```cpp
    // 1. Partial Update logic: 仅更新 Dirty 或 New 的实体
    if (needsUpdate) {
        m_shadowBuffer[index] = ...; // Update CPU Shadow Buffer
    }
    // 2. Bulk Upload: 一次性将 Shadow Buffer 上传至 GPU
    //    由于 Triple Buffering，Shadow Buffer 充当了状态真理源
    memcpy(gpuPtr, m_shadowBuffer.data(), m_maxEntities * sizeof(GPUEntity));
    ```

- [x] **引入 Shadow Buffer**
  - 文件: `src/engine/render/GPUEntitySystem.hpp`
  - 新增: `std::vector<components::GPUEntity> m_shadowBuffer;`
  - 作用: 维护当前所有实体的最新 GPU 表现数据，解决 Ring Buffer 数据陈旧问题。

- [x] **扩展 PersistentBuffer 接口**
  - 实际方案: 使用 `memcpy` 批量覆写整个映射区域，利用 Write-Combining (WC) 特性获得最大 PCIe 吞吐量，避免了复杂的区间管理开销。

- [x] **重构 Update() 使用增量逻辑**
  - 步骤:
    1. 遍历实体组，检查 `DirtyTransform`。
    2. 仅对脏实体更新 `m_shadowBuffer`。
    3. 全量 `memcpy` Shadow Buffer 至 GPU Mapped Memory。

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
