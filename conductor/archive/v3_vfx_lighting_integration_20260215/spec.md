# V3 VFX Lighting Integration Spec

> **Track ID**: `v3_vfx_lighting_integration_20260215`  
> **Type**: `feature`  
> **Priority**: P1  
> **Depends On**: `v3_baseline_contracts_20260216`, `v3_shadow_pipeline_20260215`, `v3_clustered_lighting_20260215`, `v3_material_lighting_depth_20260215`  
> **对应设计文档**: [GPU_Rendering_System_3.md §8](../../设计文档/特效和UI/GPU_Rendering_System_3.md)  
> **实施路线**: Step E（第 6-8 周）

## 1. Goal

将 V3 光影/材质控制能力暴露给 VFX 内容创作流程，使高端视觉效果完全数据驱动（而非 shader 硬编码），提升内容生产效率和效果一致性。

## 2. Scope

1. `src/engine/vfx/VFXTypes.hpp` — 新事件类型定义
2. `src/engine/vfx/VFXSequenceManager.*` — 解析/验证扩展
3. `src/engine/vfx/VFXSequencerSystem.*` — Runtime handler 扩展
4. `src/engine/vfx/VFXBudgetEstimator.*` (new) — 序列预算估计器
5. VFX schema 文档 + 验证器
6. `assets/vfx/templates/v3/` — 12 个 V3 模板序列
7. `tools/vfx_preview/` — VFX 预览场景工具
8. tests under `tests/unit`, `tests/integration`, `tests/performance`

## 3. Data Model

### 3.1 Schema 升级（对齐 §8.1）

- `vfx_schema_version`: 2 → **3**

### 3.2 新事件类型（对齐 §8.2）

```cpp
enum class VFXEventType : uint8_t {
    // ... existing v2 events ...
    ShadowPulse,         // 阴影脉冲效果
    LightProfileBlend,   // 光照配置混合
    MaterialPhaseShift   // 材质相位偏移
};
```

### 3.3 事件 Payload 结构

```cpp
struct ShadowPulseParams {
    float softnessScale;    // 柔和度缩放
    float intensityScale;   // 强度缩放
    float duration;         // 持续时间(s)
};

struct LightProfileBlendParams {
    uint32_t profileA;      // 起始光照配置 ID
    uint32_t profileB;      // 目标光照配置 ID
    float blendTime;        // 混合时间(s)
};

struct MaterialPhaseShiftParams {
    float roughnessScale;   // 粗糙度缩放
    float specularScale;    // 镜面反射缩放
    float emissiveScale;    // 自发光缩放
    float duration;         // 持续时间(s)
};
```

### 3.4 事件执行策略（对齐 §8.3）

每个事件支持 `tierPolicy` 字段：

| 策略 | 行为 |
|---|---|
| `strict` | 当前 Tier 不支持时，立即失败并记录错误 |
| `degrade` | 自动降级到当前 Tier 可用的近似效果 |
| `skip` | 当前 Tier 不支持时，静默跳过但记录 info 日志 |

**失败必须结构化日志，禁止 silent ignore**。

## 4. ECS and Systems

1. 复用现有 VFX 运行时实体/组件。
2. 扩展 sequencer runtime 以调度新事件 handler。
3. 每个新事件类型一个独立 handler 函数。
4. Handler 通过接口访问 Shadow/Material/Light 系统，不直接依赖内部实现。

## 5. Persistence（对齐 §14.1-§14.2）

### 5.1 兼容策略

- `vfx_schema_version=3` JSON 解析器验证新字段和 tierPolicy 枚举。
- v2 序列保持可加载（N/N-1 兼容）。
- 不支持或格式错误的事件必须输出 **显式错误**，禁止无声忽略。

### 5.2 校验规则

- `tierPolicy` 必须是 `strict|degrade|skip` 之一。
- 事件 payload 的数值范围必须校验（如 duration > 0）。
- 未知事件类型输出 warning 并跳过（安全降级）。

## 6. 工具链要求（对齐 §8.4）

### 6.1 VFX 预览场景（MUST）

- 支持时间线可视化（事件在时间轴上的分布）。
- 支持热重载 diff（修改 JSON → 自动检测差异 → 刷新预览）。
- 可在独立窗口或内嵌面板中运行。

### 6.2 序列预算估计器（MUST）

- 分析单个 VFX 序列的成本构成：
  - 粒子成本（发射数/生命周期/计算复杂度）
  - 灯光成本（动态光源数/光照范围）
  - 阴影/材质成本（shadow pulse 影响/材质相位偏移复杂度）
- 输出格式：
  - **机器可读**: JSON（供 CI/自动化消费）
  - **人类可读**: 控制台摘要（供美术人员查看）
- 预算超限时输出 warning 提示。

### 6.3 模板序列（MUST）

至少 12 个 V3 模板序列，覆盖以下类别：

| 类别 | 最少数量 | 示例 |
|---|---:|---|
| 近战 (melee) | 3 | 斩击闪光、重击地震、连击节奏 |
| 法术 (spell) | 3 | 火球爆炸、冰冻扩散、雷击链 |
| AoE | 2 | 范围毒雾、神圣光柱 |
| 召唤 (summon) | 2 | 暗影传送、元素凝聚 |
| 环境交互 | 2 | 火把点燃、水面波纹扩散 |

**每个模板必须包含 tierPolicy 配置和 Tier 降级行为测试**。

## 7. Performance

- 新事件 handler 不得在主循环热路径中产生堆分配。
- 单次 VFX 序列调度开销 < 0.1ms。
- 序列压力测试（同时 10 个序列运行）不得导致帧时间波动 > 1ms。

## 8. 风险与缓解

| 风险 | 影响 | 缓解 |
|---|---|---|
| 事件 handler 性能开销超预期 | 帧时间飙升 | 预算估计器 + 压力测试 + tierPolicy 降级 |
| V2 序列兼容性遗漏 | 旧效果失效 | v2 全量回归测试 |
| 热重载 diff 不稳定 | 预览工具不可用 | 双缓冲加载 + 校验通过后替换 |

## 9. Acceptance Criteria

1. 3 种新事件类型可解析、校验并确定性执行。
2. `tierPolicy` 行为（strict/degrade/skip）有测试覆盖。
3. 预算估计器输出 JSON + 人类可读摘要。
4. 预览工具支持时间线可视化和热重载 diff。
5. 12 个模板序列全部可加载并通过 Tier 降级测试。
6. v2 序列兼容性回归通过。
7. Build 和 tests 通过。
