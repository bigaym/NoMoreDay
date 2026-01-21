# Phase 3: Triple-Buffered Persistent Mapping 规格说明书

**Track ID**: `performance_optimization/phase3_triple_buffer`  
**优先级**: P2 (零拷贝)  
**预计收益**: CPU-GPU Wait-for-Idle 同步停顿降为 0  
**依赖**: Phase 1 (MDI 基础设施)

---

## 1. 问题陈述 (Problem Statement)

### 当前 CPU-GPU 同步模式
```
Frame N:
├── CPU: 写入 Buffer
├── glBufferSubData (隐式同步!) ← GPU 必须等待上一帧读取完成
├── GPU: 读取 Buffer
└── glReadBuffer (显式同步!) ← CPU 必须等待 GPU 写入完成

瓶颈: CPU 和 GPU 相互等待，无法真正并行
```

### 同步开销分析
| 操作 | 典型耗时 | 原因 |
|------|----------|------|
| `glBufferSubData` | 0.2-0.5ms | 驱动内部做了 CPU-GPU 同步 |
| `glMapBuffer` | 0.1-1.0ms | 必须等待 GPU 完成读取 |
| `ComputeBuffer::Read` | 0.5-2.0ms | GPU → CPU 回读是最慢操作 |

---

## 2. 技术方案 (Technical Design)

### 2.1 Triple-Buffer 环形结构

```
┌─────────────────────────────────────────────────────────────────┐
│                     Persistent Mapped Buffer                    │
├───────────────────┬───────────────────┬────────────────────────┤
│   Slot 0 (GPU)    │   Slot 1 (GPU)    │   Slot 2 (CPU Write)   │
│   Reading Frame N │   Reading Frame N-1│   Writing Frame N+1    │
└───────────────────┴───────────────────┴────────────────────────┘
                    ↑                   ↑
                    │                   │
               Fence N-2            Fence N-1
              (Signaled)           (Pending)
```

### 2.2 Fence 同步逻辑

```cpp
// 每帧更新
void PersistentBuffer::BeginWrite() {
    // 等待最老的帧 (N-2) 完成
    WaitForFence(m_fences[m_writeSlot]);
    
    // 返回可写指针
    return m_mappedPtr + m_writeSlot * m_slotSize;
}

void PersistentBuffer::EndWrite() {
    // 发出内存屏障确保写入完成
    glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
    
    // 为当前帧创建 Fence
    m_fences[m_writeSlot] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    
    // 旋转到下一个 Slot
    m_writeSlot = (m_writeSlot + 1) % 3;
}
```

### 2.3 OpenGL 扩展要求

| 扩展 | 用途 | 必要性 |
|------|------|--------|
| `ARB_buffer_storage` | Persistent Mapping | 必需 |
| `ARB_sync` | Fence 同步 | 必需 |
| `ARB_map_buffer_range` | 精细映射控制 | 推荐 |

### 2.4 创建标志

```cpp
void PersistentBuffer::Create(size_t size) {
    m_slotSize = size;
    m_totalSize = size * 3;  // Triple buffer
    
    glCreateBuffers(1, &m_bufferId);
    
    GLbitfield flags = 
        GL_MAP_WRITE_BIT |           // CPU 可写
        GL_MAP_PERSISTENT_BIT |       // 持久映射
        GL_MAP_COHERENT_BIT;          // 自动同步 (或用 FlushMappedBufferRange)
    
    glNamedBufferStorage(m_bufferId, m_totalSize, nullptr, flags);
    
    m_mappedPtr = (uint8_t*)glMapNamedBufferRange(
        m_bufferId, 0, m_totalSize,
        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT
    );
}
```

---

## 3. 数据结构

```cpp
// src/engine/render/PersistentBuffer.hpp

class PersistentBuffer {
public:
    void Create(size_t slotSize);
    void Destroy();
    
    // 获取当前可写 Slot 的指针
    void* BeginWrite();
    void EndWrite();
    
    // 获取当前 GPU 可读 Slot 的 Buffer Offset
    size_t GetReadOffset() const;
    
    // 绑定到 Shader Storage 绑定点
    void BindBase(GLuint bindingPoint) const;
    
    // 检查硬件支持
    static bool IsSupported();

private:
    GLuint m_bufferId = 0;
    uint8_t* m_mappedPtr = nullptr;
    size_t m_slotSize = 0;
    size_t m_totalSize = 0;
    
    int m_writeSlot = 0;
    int m_readSlot = 2;  // GPU 读取的是 N-2 帧
    
    GLsync m_fences[3] = {nullptr, nullptr, nullptr};
    
    void WaitForFence(GLsync& fence);
};
```

---

## 4. 使用模式

### 4.1 GPUEntitySystem 集成

```cpp
// 每帧更新实体位置
void GPUEntitySystem::Update(entt::registry& registry, float dt) {
    // 获取可写指针 (自动等待 Fence)
    auto* writePtr = (GPUEntity*)m_persistentBuffer.BeginWrite();
    
    // 直接写入映射内存，无需 staging buffer
    int index = 0;
    auto group = registry.group<Position, Velocity, Radius, GPUIndex>();
    for (auto [entity, pos, vel, radius, gpuIdx] : group.each()) {
        writePtr[index].position = {pos.x, pos.y};
        writePtr[index].velocity = {vel.vx, vel.vy};
        writePtr[index].radius = radius.value;
        index++;
    }
    
    m_persistentBuffer.EndWrite();
    
    // GPU 读取的是两帧前的数据，完全无等待
    m_persistentBuffer.BindBase(1);
    rlComputeShaderDispatch(...);
}
```

### 4.2 延迟说明

> **[INFO]** Triple-Buffer 引入 2 帧延迟。对于物理模拟这是可接受的，但需注意:
> - 玩家位置: 继续使用即时同步 (单独处理)
> - 敌人/投射物: 使用 Persistent Buffer

---

## 5. 实现计划

### Task 3.1: 实现 PersistentBuffer 类
**文件**: `src/engine/render/PersistentBuffer.hpp/cpp`

### Task 3.2: 添加硬件检测
**文件**: `src/engine/render/PersistentBuffer.cpp`
- 检测 `ARB_buffer_storage` 和 `ARB_sync`

### Task 3.3: 集成到 GPUEntitySystem
**文件**: `src/engine/render/GPUEntitySystem.cpp`
- 替换 `ComputeBuffer` 为 `PersistentBuffer`

### Task 3.4: 集成到 GPUParticleSystem
**文件**: `src/engine/render/GPUParticleSystem.cpp`
- 粒子系统使用 Persistent Buffer

### Task 3.5: 验证与测试
- GPU Timer Query 测量同步开销
- Validation Layer 检测

---

## 6. 验收标准

| 指标 | 基准 | 目标 |
|------|------|------|
| CPU 等待 GPU 时间 | > 0.5ms | < 0.05ms |
| Buffer Update 耗时 | ~0.3ms | ~0ms (Direct Write) |
| Validation Layer | Clean | Clean |

---

## 7. 风险与缓解

| 风险 | 缓解策略 |
|------|----------|
| 驱动不支持 | 运行时检测，回退到 glBufferSubData |
| Fence 死锁 | 设置超时，超时后强制同步 |
| 2帧延迟影响游戏感 | 玩家实体单独处理，不使用 Persistent Buffer |

---

*设计者: Gemini (Skill: designer)*
