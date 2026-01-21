# 渲染架构审计报告 (2026-01-22)

## 1. 核心结论
目前 `NoMoreDay` 的渲染架构在应对数万个实体（MDI）和高频率特效（GPU Particle）时表现稳健。然而，在 **CPU-GPU 同步** 和 **UI 元素批处理** 方面存在明显的性能隐患，可能在高负载下导致明显的微卡顿（Micro-stuttering）。

## 2. 详细发现

### 2.1 [高风险] 粒子系统同步阻塞 (Sync Readback)
- **文件**: `src/engine/render/GPUParticleSystem.cpp`
- **现象**: 每帧调用 `m_atomicBuffer.Read(&aliveCount, sizeof(uint32_t))`。
- **后果**: 强制 CPU 等待 GPU 完成当前的 Compute Shader 计算，破坏了渲染流水线的并行性。随着粒子数量增加，CPU 停顿时间将线性增长。

### 2.2 [中风险] 伤害飘字 Draw Call 爆炸
- **文件**: `src/engine/render/RenderSystem.cpp`
- **现象**: 在 `RenderSystem` 中循环遍历所有飘字并调用 `DrawTextEx`。
- **后果**: 虽然 Raylib 有内部批处理，但数以百计的独立文本绘制在逻辑上增加了 CPU 提交负担，且无法利用 GPU 实例化优势。

### 2.3 [中风险] 实体状态同步开销
- **文件**: `src/game/systems/GPUEntitySystem.cpp`
- **现象**: 每帧对数万实体的 CPU 数据进行迭代并提交至 SSBO。
- **后果**: 虽然使用 `entt::group` 优化了内存局部性，但在实体数量级达到 20k+ 时，CPU 迭代本身成为主循环的一个大头（约 2-3ms）。

## 3. 改进建议
1. **异步化粒子计数器**：引入双缓冲或三缓冲的原子计数器回读，或者直接在 Compute Shader 中基于上一帧数据进行预估。
2. **飘字实例化渲染**：将 `DamagePopup` 转化为 GPU 实例，由 `GPUSkillEffectSystem` 或独立的实例化着色器渲染。
3. **脏标记同步**：在 `GPUEntitySystem` 中引入脏标记（Dirty Flags），仅对位置发生显著变化的实体更新 SSBO 数据。
