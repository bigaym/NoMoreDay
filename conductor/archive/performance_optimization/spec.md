# 性能极致优化策略规格说明书 (Performance Optimization Strategy Specification)

**版本**: 1.0  
**日期**: 2026-01-20  
**目标帧率**: 180 FPS (帧预算 5.5ms)  
**触发文档**: `conductor/analyzer/2026-01-20_performance_risk_analyze.md`

---

## 1. 背景与目标 (Background & Goals)

### 1.1 核心问题
| 瓶颈类型 | 现状 | 目标 |
|---------|------|------|
| CPU Draw Call 开销 | CPU 遍历决定绘制，O(N) 开销 | GPU 自主剔除，CPU 开销 O(1) |
| 跨组件内存访问 | 随机访问导致 Cache Miss | entt::group 物理内存重排，Cache Miss 接近 0 |
| 分支预测失败 | 大规模循环中 if 判断导致流水线停顿 | Branchless 位运算优化 |
| CPU-GPU 同步 | 每帧 Wait-for-Idle 同步开销 | Triple-Buffer 零拷贝，完全解耦 |
| 空间查询计算密度 | 标量距离计算 | SIMD 向量化一次处理 8 实体 |

### 1.2 分阶段交付策略
采用 **5 Phase Rolling Delivery** 模式：
- 每个 Phase 独立成 Track，可单独测试和评估
- 后续 Phase 依赖前置 Phase 的基础设施
- 风险隔离：单一 Phase 失败不影响其他优化成果

---

## 2. Phase 定义 (Phase Definitions)

### Phase 1: GPU-Driven MDI Rendering (P0 - 极致性能)
**Track ID**: `performance_optimization/phase1_mdi_rendering`  
**预计收益**: CPU Draw Call 开销降低 90%+  
**依赖**: 无

#### 技术目标
1. 实现 `DrawArraysIndirectCommand` 缓冲区结构
2. 编写 GPU Culling Compute Shader (视锥剔除 + 简易遮挡)
3. 改造 `GPUEntitySystem::Render()` 使用 `glMultiDrawArraysIndirect`
4. 实现 `InstanceBuffer` 将所有实体变换数据一次性上传 SSBO

#### 影响范围
- `src/engine/render/GPUEntitySystem.hpp/cpp`
- `assets/shaders/` (新增 cull.compute)
- 新增 `src/engine/render/MDIRenderer.hpp/cpp`

---

### Phase 2: EnTT Group Memory Optimization (P1 - 内存效率)
**Track ID**: `performance_optimization/phase2_entt_group`  
**预计收益**: 属性系统更新 Cache Miss 降低 80%+  
**依赖**: 无 (可与 Phase 1 并行)

#### 技术目标
1. 定义 `CombatGroup = entt::group<Position, Velocity, CombatStats>`
2. 定义 `RenderGroup = entt::group<Position, Radius, GPUIndex>`
3. 重构 `StatsSystem::Recalculate()` 使用 Owning Group 遍历
4. 重构 `GPUEntitySystem::Update()` 使用 RenderGroup

#### 影响范围
- `src/game/systems/stats/StatsSystem.cpp`
- `src/engine/render/GPUEntitySystem.cpp`
- `src/game/components/Common.hpp` (组件布局调整)

#### 风险点
> **[CRITICAL]** EnTT Group 对组件顺序有严格要求。创建 Group 后，涉及组件的 `emplace`/`remove` 操作必须通过 Group 提供的接口，否则会导致内存布局混乱。

---

### Phase 3: Triple-Buffered Persistent Mapping (P2 - 零拷贝)
**Track ID**: `performance_optimization/phase3_triple_buffer`  
**预计收益**: CPU-GPU Wait-for-Idle 同步停顿降为 0  
**依赖**: Phase 1 (MDI 基础设施)

#### 技术目标
1. 实现 `PersistentBuffer` 类封装 GL_MAP_PERSISTENT_BIT
2. 分配 3x 容量的环形缓冲区
3. 使用 `glFenceSync` 确保 CPU 写入帧与 GPU 读取帧的安全间隔
4. 改造 `GPUEntitySystem` 和 `GPUParticleSystem` 使用新缓冲区

#### 影响范围
- 新增 `src/engine/render/PersistentBuffer.hpp/cpp`
- `src/engine/render/GPUEntitySystem.cpp`
- `src/engine/render/GPUParticleSystem.cpp`

#### 风险点
> **[CRITICAL]** Fence 管理不当会导致 GPU Hang 或撕裂。必须严格验证 Fence 状态再进行 CPU 写入。

---

### Phase 4: SIMD SpatialGrid Query (P2 - 逻辑提速)
**Track ID**: `performance_optimization/phase4_simd_spatial`  
**预计收益**: 空间查询性能提升 4-6x  
**依赖**: 无 (可与其他 Phase 并行)

#### 技术目标
1. 重构 `SpatialHashGrid::query()` 使用 xsimd 批处理
2. 一次处理 8 个实体坐标 (AVX2 / SSE4)
3. 向量化距离平方计算和掩码过滤
4. 实现 `SIMDSpatialGrid` 子类（保留原实现作为 Fallback）

#### 影响范围
- `src/engine/physics/SpatialGrid.hpp`
- 新增 `src/engine/physics/SIMDSpatialGrid.hpp`
- `src/game/systems/combat/ProjectileSystem.cpp` (使用新 API)
- `src/game/systems/ai/AISystem.cpp` (使用新 API)

#### 设计模式
```cpp
// 批处理 8 个实体的距离检测
using batch_pos = xsimd::batch<float, xsimd::avx2>;
batch_pos dx = batch_pos::load_aligned(&query_x[i]) - center_x;
batch_pos dy = batch_pos::load_aligned(&query_y[i]) - center_y;
batch_pos dist_sq = dx * dx + dy * dy;
auto mask = dist_sq < radius_sq;
// 使用 mask 过滤结果
```

---

### Phase 5: Branchless Combat Logic (P3 - 流水线优化)
**Track ID**: `performance_optimization/phase5_branchless`  
**预计收益**: 大规模循环分支预测失败率降低 95%+  
**依赖**: Phase 2 (Group 遍历模式)

#### 技术目标
1. 重构 `CombatSystem::CalculateDamage()` 消除 if 分支
2. 使用位掩码 `mask = -(int)condition` 技术
3. 优化 `StatsSystem` 中的 Buff 条件判断
4. 编写 Benchmark 对比优化前后的流水线效率

#### 影响范围
- `src/game/systems/combat/CombatSystem.cpp`
- `src/game/systems/stats/StatsSystem.cpp`
- `src/game/systems/monster/MonsterAffixSystem.cpp`

#### 代码示例
```cpp
// Before (有分支)
if (hasCrit) damage *= critMultiplier;

// After (无分支)
int mask = -(int)hasCrit;  // 全 1 或全 0
float mult = 1.0f + (critMultiplier - 1.0f) * mask;
damage *= mult;
```

---

## 3. 依赖图 (Dependency Graph)

```
         Phase 1 (MDI)
              │
              ▼
         Phase 3 (Triple-Buffer)
              
Phase 2 (EnTT Group) ────► Phase 5 (Branchless)
              
         Phase 4 (SIMD) [独立]
```

---

## 4. 验收标准 (Acceptance Criteria)

### 全局指标
| 指标 | 基准值 | 目标值 | 测试方法 |
|-----|--------|--------|----------|
| 帧时间 (10k 敌人) | > 8ms | < 5.5ms | RenderDoc Frame Capture |
| Draw Call 数量 | O(N) | O(1) | glGetIntegerv(GL_DRAW_CALLS) |
| 属性更新耗时 | TBD | 降低 50% | Tracy Profiler |
| CPU-GPU 同步等待 | > 0.5ms | < 0.1ms | GPU Timer Query |

### 各 Phase 验收
| Phase | 通过条件 |
|-------|----------|
| Phase 1 | `glMultiDrawArraysIndirect` 成功调用，渲染结果与原系统一致 |
| Phase 2 | `StatsSystem` 使用 Group 遍历，所有 Stats 相关测试通过 |
| Phase 3 | Fence 同步正常，无 GPU Hang，Validation Layer 无错误 |
| Phase 4 | `SIMDSpatialGridTest` 通过，性能提升 >= 3x |
| Phase 5 | `BranchlessCombatTest` 通过，Benchmark 显示提升 |

---

## 5. 风险与缓解 (Risks & Mitigation)

| Phase | 风险 | 缓解策略 |
|-------|------|----------|
| Phase 1 | Raylib rlgl 状态机冲突 | 绕过 rlgl，直接使用 OpenGL API |
| Phase 2 | Group 破坏现有组件操作 | 封装 `GroupAwareRegistry` 代理类 |
| Phase 3 | Persistent Map 驱动兼容性 | 运行时检测 ARB_buffer_storage，回退标准 Buffer |
| Phase 4 | SIMD 指令集兼容性 | xsimd 自动选择最佳指令集，附带 Scalar Fallback |
| Phase 5 | 可读性下降 | 充分注释 + 并行保留原 Debug 版本 |

---

## 6. 建议执行顺序 (Recommended Execution Order)

1. **Phase 2 (EnTT Group)** - 独立且低风险，可立即开始
2. **Phase 4 (SIMD)** - 独立且收益明确
3. **Phase 1 (MDI)** - 核心优化，需要较多开发时间
4. **Phase 3 (Triple-Buffer)** - 依赖 Phase 1
5. **Phase 5 (Branchless)** - 收尾优化，依赖 Phase 2

---

*设计者: Gemini (Skill: designer)*
