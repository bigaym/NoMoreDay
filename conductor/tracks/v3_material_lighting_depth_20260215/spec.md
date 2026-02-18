# V3 Material Lighting Depth Spec

> **Track ID**: `v3_material_lighting_depth_20260215`  
> **Type**: `feature`  
> **Priority**: P1  
> **Depends On**: `v3_baseline_contracts_20260216`, `v3_shadow_pipeline_20260215`, `v3_clustered_lighting_20260215`  
> **对应设计文档**: [GPU_Rendering_System_3.md §7](../../设计文档/特效和UI/GPU_Rendering_System_3.md)  
> **实施路线**: Step D（第 4-6 周）

## 0. Carry-Over Objective (Merged)

This track absorbs the deferred clustered-lighting optimization objective from
`v3_clustered_lighting_20260215`.

1. Keep clustered 128-light benchmark non-regression as a hard safety contract.
2. Recover and re-establish `>=5%` mean improvement target for the clustered 128-light A/B profile after Material 2.0 integration.
3. Keep fallback determinism and visual parity guarantees unchanged.

## 1. Goal

将材质受光响应从 V2 ("base color + emissive") 升级为 Material 2.0，实现 "同光异材" 的视觉层次感：

1. Normal-driven 细节受光方向。
2. Roughness-controlled 高光宽度。
3. Specular 反射响应。
4. AO (Ambient Occlusion) 环境遮蔽。
5. 与 V1 资产完全向后兼容。

## 2. Scope

1. `src/engine/render/GPUData.hpp` — `GPUMaterialDataV2`
2. `src/engine/render/material/MaterialManager.*`
3. `src/engine/render/core/RenderConstants.hpp`
4. `assets/shaders/entity_mdi.frag` — BRDF-lite 分支
5. `assets/shaders/particle.frag` — BRDF-lite 分支
6. material asset schema 与验证器
7. `src/engine/render/resource/TextureArrayManager.*` — Texture2DArray 管理
8. `assets/textures/defaults/` — 中性默认纹理
9. tests under `tests/unit`, `tests/integration`, `tests/performance`

## 3. Data Model

### 3.1 Schema 升级（对齐 §7.1）

- `material_schema_version`: 1 → **2**

### 3.2 新增字段（对齐 §7.2）

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---:|---|
| `normalMapSlot` | int | -1 | Normal map 在 Texture2DArray 中的层索引 |
| `roughness` | float | 0.6 | 粗糙度 |
| `specular` | float | 0.2 | 镜面反射强度 |
| `ao` | float | 1.0 | 环境遮蔽 |
| `heightBias` | float | 0.0 | 高度偏移 |
| `detailNormalScale` | float | 1.0 | 细节法线缩放 |

### 3.3 GPU 结构（对齐 §21.4）

```cpp
struct alignas(16) GPUMaterialDataV2 {
    glm::vec4 baseColor;           // 基础颜色
    glm::vec4 emissiveAndIntensity; // 自发光 + 强度
    glm::vec4 pbrLite;             // roughness, specular, ao, heightBias
    glm::vec4 textureSlots;        // albedo, normal, roughness, reserved
    glm::vec4 detailParams;        // detailNormalScale + reserved[3]
    glm::vec4 reserved[3];
};
static_assert(sizeof(GPUMaterialDataV2) == 128);
```

### 3.4 ABI 契约

- 结构必须通过 ABI V3 生成链路产出。
- 128B 对齐由 `static_assert` 强制验证。

## 4. ECS and Systems

1. 复用现有 `MaterialComponent` 和 `MaterialManager` 生命周期。
2. 扩展运行时上传路径以产出 `GPUMaterialDataV2`。
3. 保持 DOD 布局，禁止热路径堆分配。

## 5. Shader 策略（对齐 §7.4）

### 5.1 BRDF-lite 接入

- `entity_mdi.frag` 和 `particle.frag` 接入 BRDF-lite 分支。
- 点光/聚光统一采用 `N·L`（diffuse）与 half-vector（specular）。
- 保持 non-PBR 美术参数风格，但计算过程物理可解释。

### 5.2 Tier 分支控制

- **Low/Medium**: 自动关闭高阶材质分支（normal/specular/roughness 不采样）。
- **High**: 部分启用（normal + roughness only）。
- **Ultra**: 完全启用所有材质分支。

### 5.3 与 Shadow + Cluster 的整合

最终光照方程：

```
finalColor = attenuation * shadowFactor * BRDF-lite(N, L, V, material)
```

- `shadowFactor` 来自 Shadow Track。
- `lightList` 来自 Clustered Track（如已启用）。
- 本 Track 负责 `BRDF-lite` 部分。

## 6. 资源策略（对齐 §7.5）

### 6.1 Texture2DArray 分层管理

- Normal、Roughness 纹理使用 `Texture2DArray` 分层管理。
- 复用现有 asset registry 和 texture slot assignment 路径。
- `TextureArrayManager` 负责层分配、生命周期和 resize 安全。

### 6.2 中性默认纹理

- 缺失的 normal map slot 解析为中性默认纹理（flat normal: `(0.5, 0.5, 1.0)`）。
- 缺失的 roughness slot 使用默认中灰值。
- 默认纹理在引擎初始化时预创建，不在热路径中加载。

### 6.3 资产热重载（对齐 §14.3）

- 材质热重载采用 **双缓冲句柄策略**：新资源验证通过后原子替换。
- 热重载失败时保持旧资源有效，不得产生黑屏或崩溃。

## 7. Persistence（对齐 §14.1-§14.2）

### 7.1 v1 → v2 兼容

- v1 JSON 资产自动填充默认值映射。
- 缺失字段发出 `spdlog::warn` 明确提示（1 次/资产节流）。
- 禁止崩溃和未定义内存读取。

### 7.2 校验策略

- 非法字段和缺失字段必须可诊断，不得无声失败。
- `material_schema_version` 必须存在且有效。
- v1 兼容通过默认值映射表，不修改原始资产文件。

## 8. Performance Budget（对齐 §15.3）

| 指标 | 预算 |
|---|---:|
| High 档材质新增开销 | ≤ 0.6ms |
| Low/Medium 档 | 与 V2 持平（分支关闭） |

## 9. 风险与缓解

| 风险 | 影响 | 缓解 |
|---|---|---|
| Texture2DArray 层数不足 | 材质无法加载 | 动态扩容 + 日志告警 |
| v1 资产兼容遗漏 | 崩溃或渲染异常 | 全量 v1 资产回归测试 |
| 热重载原子替换失败 | 黑屏 | 双缓冲句柄 + 回退到旧资源 |

## 10. Non-Goals

1. 不实现完整 PBR（Cook-Torrance 等），仅 BRDF-lite。
2. 不创建美术资产（Normal Map 等纹理由美术管线提供）。
3. 不修改 VFX 序列器（由 VFX Track 覆盖）。

## 11. Acceptance Criteria

1. 角色/地形/特效材质在相同光照下展现稳定、可见的视觉差异。
2. v1 资产加载无崩溃，使用确定性默认值。
3. Tier 降级行为确定性（Low/Medium 无高阶开销）。
4. High 档材质新增开销 ≤ 0.6ms。
5. 热重载材质资产不崩溃。
6. Shader ABI CPU/GLSL 无不匹配。
7. Unit/integration/perf tests 通过。
