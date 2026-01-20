# Phase 3: Triple-Buffered Persistent Mapping 实施计划

**Track ID**: `performance_optimization/phase3_triple_buffer`  
**状态**: 📋 Planned  
**预计工时**: 2-3 天  
**前置依赖**: Phase 1 (MDI 基础设施，可选)

---

## 任务分解 (Task Breakdown)

### Task 3.1: 创建 PersistentBuffer 核心类 ⬜
**优先级**: Critical  
**预计时间**: 3h

**操作**:
1. 创建 `src/engine/render/PersistentBuffer.hpp`
2. 创建 `src/engine/render/PersistentBuffer.cpp`
3. 实现以下接口:
   - `Create(size_t slotSize)` - 分配 3x 持久映射缓冲区
   - `Destroy()` - 释放资源
   - `BeginWrite() -> void*` - 获取可写指针
   - `EndWrite()` - 完成写入，设置 Fence
   - `BindBase(GLuint binding)` - 绑定到 SSBO
   - `static IsSupported() -> bool` - 硬件检测

**实现细节**:
```cpp
void PersistentBuffer::WaitForFence(GLsync& fence) {
    if (!fence) return;
    
    GLenum result = GL_TIMEOUT_EXPIRED;
    while (result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED) {
        result = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000); // 1ms 超时
        if (result == GL_WAIT_FAILED) {
            LOG_ERROR("PersistentBuffer: Fence wait failed!");
            break;
        }
    }
    glDeleteSync(fence);
    fence = nullptr;
}
```

**验收条件**:
- [ ] 编译通过
- [ ] `IsSupported()` 在支持的硬件上返回 true

---

### Task 3.2: 实现硬件兼容性检测 ⬜
**优先级**: High  
**预计时间**: 1h

**操作**:
1. 在 `PersistentBuffer::IsSupported()` 中检测:
   - `GLAD_GL_ARB_buffer_storage` 或 OpenGL 4.4+
   - `GLAD_GL_ARB_sync`
2. 添加日志输出检测结果

**代码**:
```cpp
bool PersistentBuffer::IsSupported() {
    // OpenGL 4.4 原生支持，或通过扩展
    bool hasBufferStorage = GLAD_GL_VERSION_4_4 || GLAD_GL_ARB_buffer_storage;
    bool hasSync = GLAD_GL_VERSION_3_2 || GLAD_GL_ARB_sync;
    
    if (!hasBufferStorage) {
        LOG_WARN("PersistentBuffer: ARB_buffer_storage not available");
    }
    if (!hasSync) {
        LOG_WARN("PersistentBuffer: ARB_sync not available");
    }
    
    return hasBufferStorage && hasSync;
}
```

---

### Task 3.3: 创建 Fallback 路径 ⬜
**优先级**: Medium  
**预计时间**: 1.5h

**操作**:
1. 若不支持 Persistent Mapping，使用传统 `glBufferSubData` 路径
2. 实现 `PersistentBuffer` 的 "Compat Mode"

**设计**:
```cpp
class PersistentBuffer {
public:
    enum class Mode { Persistent, Compat };
    
    void Create(size_t size) {
        if (IsSupported()) {
            m_mode = Mode::Persistent;
            CreatePersistent(size);
        } else {
            m_mode = Mode::Compat;
            CreateCompat(size);
        }
    }
    
    void* BeginWrite() {
        if (m_mode == Mode::Persistent) {
            WaitForFence(m_fences[m_writeSlot]);
            return m_mappedPtr + m_writeSlot * m_slotSize;
        } else {
            return m_stagingBuffer.data();
        }
    }
    
    void EndWrite() {
        if (m_mode == Mode::Persistent) {
            // Fence + Rotate
        } else {
            glNamedBufferSubData(m_bufferId, 0, m_slotSize, m_stagingBuffer.data());
        }
    }
    
private:
    Mode m_mode = Mode::Compat;
    std::vector<uint8_t> m_stagingBuffer;  // Compat 模式用
};
```

---

### Task 3.4: 集成到 GPUEntitySystem ⬜
**优先级**: High  
**预计时间**: 2h

**操作**:
1. 在 `GPUEntitySystem` 中添加 `PersistentBuffer m_entityPersistentBuffer`
2. 修改 `Update()` 使用 `BeginWrite/EndWrite` 模式
3. 修改 `SyncBack()` 使用正确的读取 Offset

**关键改动**:
```cpp
void GPUEntitySystem::Update(entt::registry& registry, float dt) {
    // 替换: m_localData.resize(...); m_entityBuffer.Update(...)
    // 为:
    auto* writePtr = (GPUEntity*)m_entityPersistentBuffer.BeginWrite();
    
    int index = 0;
    auto view = registry.view<Position, Velocity, Radius, GPUIndex>(...);
    view.each([&](auto entity, auto& pos, auto& vel, auto& radius, auto& gpuIdx) {
        gpuIdx.index = index;
        writePtr[index].position = {pos.x, pos.y};
        writePtr[index].velocity = {vel.vx, vel.vy};
        writePtr[index].radius = radius.value;
        writePtr[index].type = registry.all_of<EnemyTag>(entity) ? 1 : 0;
        index++;
    });
    
    m_entityPersistentBuffer.EndWrite();
    m_entityPersistentBuffer.BindBase(1);
    
    // 继续 Compute Shader 调度...
}
```

---

### Task 3.5: 集成到 GPUParticleSystem ⬜
**优先级**: Medium  
**预计时间**: 1.5h

**操作**:
1. 查看 `GPUParticleSystem` 当前 Buffer 使用模式
2. 替换粒子数据上传为 Persistent Buffer
3. 验证粒子渲染正确

---

### Task 3.6: 创建 PersistentBufferTest ⬜
**优先级**: High  
**预计时间**: 1.5h

**操作**:
1. 创建 `tests/unit/PersistentBufferTest.hpp`
2. 测试场景:
   - 创建和销毁
   - 多帧写入读取循环
   - Fence 同步正确性

---

### Task 3.7: 性能验证 ⬜
**优先级**: High  
**预计时间**: 1h

**操作**:
1. 使用 GPU Timer Query 测量 Buffer 同步时间
2. 对比 Compat 模式和 Persistent 模式
3. 验证无 GPU Hang

---

## 依赖关系

```
Task 3.1 ──► Task 3.2 ──► Task 3.3
                │
                ▼
           Task 3.4 ──► Task 3.5
                │
                ▼
           Task 3.6 ──► Task 3.7
```

---

## 验收清单

- [ ] `PersistentBuffer` 类实现完成
- [ ] 硬件检测功能正常
- [ ] Compat 模式回退正常
- [ ] `GPUEntitySystem` 使用 Persistent Buffer
- [ ] `GPUParticleSystem` 使用 Persistent Buffer (可选)
- [ ] `PersistentBufferTest` 通过
- [ ] GPU Timer 验证同步开销接近 0
- [ ] Validation Layer 无错误

---

## 回滚计划

若遇到严重问题:
1. `PersistentBuffer::IsSupported()` 强制返回 false
2. 系统自动使用 Compat 模式
3. 性能回退到原有水平，但不影响功能

---

*规划者: Gemini (Skill: designer)*
