# Specification: Rendering Performance & Sync Optimization

## 1. 背景与目标

### 1.1 问题陈述
根据 2026-01-22 性能审计报告（`audit_report.md`），当前渲染架构存在以下瓶颈：

| 编号 | 问题 | 风险等级 | 影响 |
|------|------|----------|------|
| P1 | 粒子系统同步阻塞 | 🔴 高 | CPU 每帧强制等待 GPU 完成 Compute Shader，破坏流水线并行性 |
| P2 | 伤害飘字 Draw Call 爆炸 | 🟡 中 | 数百个独立 `DrawTextEx` 调用增加 CPU 提交负担 |
| P3 | 实体状态同步开销 | 🟡 中 | 20k+ 实体时，CPU 迭代 SSBO 数据耗时约 2-3ms |

### 1.2 目标指标

| 指标 | 当前基线 | 优化目标 | 测量方法 |
|------|----------|----------|----------|
| `GPUParticle_Update` 耗时 | ~1.5ms (100k粒子) | < 0.5ms | `ScopedTimer` 日志 |
| 飘字 Draw Call 数量 | ~500 calls/帧 | ≤ 2 calls/帧 | RenderDoc 抓帧 |
| `GPUEntity_Update` 耗时 | ~2.5ms (20k实体) | < 1.5ms | `ScopedTimer` 日志 |
| 帧率稳定性 (1% Low) | 波动±15% | 波动±5% | FrameGraph 监控 |

---

## 2. 技术方案详述

### 2.1 [P1] 粒子计数器异步化 (Async Particle Count)

#### 当前问题
```cpp
// GPUParticleSystem.cpp:286 - 同步阻塞点
m_atomicBuffer.Read(&aliveCount, sizeof(uint32_t));
```
每帧调用 `Read()` 强制 CPU 等待 GPU 完成当前 Dispatch，造成流水线气泡。

#### 解决方案：双缓冲延迟读取

**核心思想**：本帧使用上一帧的粒子计数进行逻辑预测，消除同步等待。

**技术实现**：
1. **引入双缓冲原子计数器**
   - 创建 `m_atomicBufferPing` 和 `m_atomicBufferPong`
   - 每帧交替使用：当前帧写入 `Pong`，读取 `Ping`（上一帧结果）

2. **异步读取流程**
   ```
   Frame N:
     1. Read m_atomicBufferPing → m_lastKnownAliveCount (无阻塞，GPU早已完成)
     2. Dispatch Compute Shader → 输出到 m_atomicBufferPong
     3. Swap Ping/Pong
   
   Frame N+1:
     1. Read m_atomicBufferPong → m_lastKnownAliveCount
     ...
   ```

3. **粒子发射预估**
   - 使用 `m_lastKnownAliveCount` 判断剩余容量
   - 若上一帧粒子接近满载，本帧主动限流

**修改文件**：
- `src/engine/render/GPUParticleSystem.hpp`: 新增 `m_atomicBufferPing/Pong`, `m_lastKnownAliveCount`
- `src/engine/render/GPUParticleSystem.cpp`: 重构 `Update()` 逻辑

**验收标准**：
- [ ] `GPUParticle_Update` 中无 `glClientWaitSync` 或同步 `Read()` 调用
- [ ] 在 RenderDoc 中确认无 CPU-GPU 同步点
- [ ] 粒子发射稳定，无可见的延迟或丢失

---

### 2.2 [P2] 伤害飘字实例化 (Damage Popup Instancing)

#### 当前问题
```cpp
// DamagePopupManager.hpp:81-103 - 逐个绘制
for (auto& p : popups) {
    DrawTextEx(font, p.text, ...);  // 独立 Draw Call
}
```
每个飘字产生独立的 Draw Call，CPU 提交开销高。

#### 解决方案：GPU 实例化渲染

**核心思想**：将所有飘字打包为实例数据，使用单次 Instanced Draw 渲染。

**技术实现**：

1. **GPU 数据结构** (`GPUData.hpp`)
   ```cpp
   struct GPUPopupInstance {
       Vector2 position;      // 世界坐标
       Vector2 velocity;      // 运动速度
       float   timer;         // 当前时间
       float   lifeTime;      // 总生命周期
       uint32_t glyphStart;   // 字形起始索引 (支持多位数)
       uint32_t glyphCount;   // 字形数量
       Color   color;         // RGBA
       uint32_t flags;        // isCrit, isStatus 等
   };
   ```

2. **数字图集 (Glyph Atlas)**
   - 预渲染 `0-9`, `-`, `+`, `暴击`, `闪避` 等字形到纹理图集
   - 分辨率：每字符 32x48 像素，总图集 512x64
   - 文件：`assets/textures/popup_glyphs.png`

3. **渲染管线**
   - **Vertex Shader** (`popup.vert`): 基于实例 ID 读取 SSBO 数据，计算 Billboard 位置
   - **Fragment Shader** (`popup.frag`): 采样字形图集，应用颜色和透明度衰减
   - **Draw Call**: `glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, activePopupCount)`

4. **生命周期管理**
   - `DamagePopupManager` 维护环形缓冲区 (Ring Buffer)
   - 每帧将活动飘字上传至 `PersistentBuffer`
   - 在 Vertex Shader 中基于 `timer/lifeTime` 进行剔除（设置 `gl_Position.w = 0`）

**修改文件**：
- `src/engine/render/GPUData.hpp`: 新增 `GPUPopupInstance` 结构
- `src/game/systems/combat/DamagePopupManager.hpp`: 重构为 GPU 友好的数据布局
- `src/engine/render/PopupRenderer.hpp/cpp`: 新系统，负责实例化渲染
- `assets/shaders/popup.vert`, `popup.frag`: 新着色器
- `assets/textures/popup_glyphs.png`: 字形图集

**验收标准**：
- [ ] 500 个飘字同屏时，Draw Call ≤ 2
- [ ] 暴击飘字保持脉冲缩放动画效果
- [ ] 性能测试：飘字渲染耗时 < 0.2ms

---

### 2.3 [P3] SSBO 提交优化 (Selective Entity Update)

#### 当前问题
```cpp
// GPUEntitySystem.cpp:101-146
for (auto entity : group) {
    // 每帧迭代所有实体，无论是否移动
    gpuPtr[index].position = {pos.x, pos.y};
    ...
}
```
即使实体静止，也执行 memcpy 和 SSBO 更新。

#### 解决方案：脏标记 + 分块更新

**核心思想**：仅更新位置发生显著变化的实体，减少 CPU 迭代和 GPU 传输量。

**技术实现**：

1. **脏标记组件** (`Common.hpp`)
   ```cpp
   struct DirtyTransform {
       bool isDirty = true;  // 初始为脏，确保首次上传
   };
   ```

2. **物理系统集成**
   - 在 `PhysicsSystem` 或 GPU Compute Shader 中，当实体位移 > `DIRTY_THRESHOLD` 时设置 `isDirty = true`
   - `DIRTY_THRESHOLD = 0.5f` (半像素)

3. **分块更新策略**
   - 将实体按索引分为 N 个块 (Block Size = 1024)
   - 每块维护 `blockDirtyCount`
   - 仅当 `blockDirtyCount > 0` 时更新该块的 SSBO 区域

4. **SIMD 加速 (可选)**
   - 使用 AVX2 的 `_mm256_storeu_ps` 批量写入位置数据
   - 需确保 `GPUEntity` 结构 32 字节对齐

**修改文件**：
- `src/game/components/Common.hpp`: 新增 `DirtyTransform` 组件
- `src/engine/render/GPUEntitySystem.cpp`: 重构 `Update()` 使用分块更新
- `src/engine/render/GPUEntitySystem.hpp`: 新增块管理数据结构

**验收标准**：
- [ ] 静态场景（无移动实体）时，`GPUEntity_Update` 耗时 < 0.3ms
- [ ] 动态场景下，性能提升 ≥ 30%
- [ ] 无渲染伪影或实体位置抖动

---

## 3. 技术约束

| 约束类型 | 规范 |
|----------|------|
| C++ 标准 | C++20，使用 `constexpr`/`consteval` 进行编译时优化 |
| OpenGL | 4.3+ (SSBO, Compute Shader, Indirect Draw) |
| 内存安全 | 禁止裸 `new/delete`，全程 RAII |
| 同步规则 | **禁止**在主循环中引入 `glClientWaitSync` 或同步 `Read()` |
| 线程安全 | 粒子发射的 `m_emitMutex` 锁持有时间 < 10μs |

---

## 4. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 双缓冲导致粒子计数延迟一帧 | 极端情况下可能发射过量粒子 | 实现软上限检查，`if (lastCount > 0.9 * maxParticles) { limitEmission(); }` |
| 字形图集不支持 Unicode | 无法显示中文状态文本 | 状态文本保持 CPU 渲染，仅数字使用 GPU 实例化 |
| 脏标记增加组件数量 | EnTT 迭代性能轻微下降 | 使用 `entt::basic_sparse_set` 提高稀疏组件访问效率 |

---

## 5. 依赖项

| 依赖 | 用途 | 状态 |
|------|------|------|
| `PersistentBuffer` | 三缓冲 SSBO 管理 | ✅ 已实现 |
| `MDIRenderer` | 多绘制间接渲染 | ✅ 已实现 |
| `ScopedTimer` | 性能计时 | ✅ 已实现 |
| RenderDoc | 抓帧分析 | 🔧 需用户安装 |
