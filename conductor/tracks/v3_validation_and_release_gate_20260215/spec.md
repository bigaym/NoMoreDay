# V3 Validation and Release Gate Spec

> **Track ID**: `v3_validation_and_release_gate_20260215`  
> **Type**: `quality`  
> **Priority**: P0  
> **Depends On**: 所有 V3 feature tracks  
> **对应设计文档**: [GPU_Rendering_System_3.md §15-§16, §19-§20](../../设计文档/特效和UI/GPU_Rendering_System_3.md)  
> **实施路线**: Step F（第 8-10 周）

## 1. Goal

定义并强制执行 V3 发布门禁，确保渲染升级是可度量的、安全的和可逆的。所有 V3 功能在合并/发布前必须通过 4 层门禁验证。

## 2. Scope

1. validation 文档和脚本 under `conductor/` 和 `scripts/`
2. 测试套件 in `tests/unit`, `tests/integration`, `tests/performance`
3. `RenderConfig` feature flag 集成 (`render.v3.enabled`)
4. CI/自动化门禁条件
5. 截图差异回归工具
6. 压力运行脚本
7. 风险矩阵追踪与验证

## 3. Feature Flag 契约（对齐 §16.4）

```json
{
  "render": {
    "v3": {
      "enabled": false
    }
  }
}
```

- `render.v3.enabled=false` 时所有 V3 Pass 跳过，回退到 V2 路径。
- 黑屏或 >10% 性能回归时：**自动回退 V2** 并阻断合并。

## 4. 四层门禁定义（对齐 §16）

### 4.1 功能门禁（Functional Gate）

| 检查项 | 通过条件 |
|---|---|
| 默认帧缓冲路径 | 渲染正确，无黑屏 |
| 离屏帧缓冲路径 | 渲染正确，无黑屏 |
| 多分辨率路径 | 渲染正确（至少 3 种分辨率） |
| Quality Tier 矩阵 | Low/Medium/High/Ultra 全通过 |
| Resize 稳定 | 连续 resize 无崩溃/泄漏 |
| Alt+Tab / context restore | 恢复后渲染正确 |
| 热重载路径 | Shader/Material/VFX 热重载稳定 |

### 4.2 契约门禁（Contract Gate）

| 检查项 | 通过条件 |
|---|---|
| ABI layout 校验 | `GPU_ABI_VERSION=3` 所有结构 size/alignment 快照匹配 |
| Binding 冲突检查 | 无 binding 冲突 |
| RenderGraph 合同 | Pass 顺序 + Frame Ownership 断言通过 |
| Schema 版本校验 | material_v2 + vfx_v3 校验通过 |

### 4.3 性能门禁（Performance Gate，对齐 §15）

| 场景 | FPS 阈值 | 通过条件 |
|---|---:|---|
| 常规场景 | ≥270 FPS | 无回归 |
| 高压场景（combat） | ≥180 FPS | 无回归 |
| 极限场景（stress） | ≥144 FPS | 无回归 |

**V3 新增 Pass 预算**:

| Pass | 常规 | 高压 | 极限 |
|---|---:|---:|---:|
| LightCullingPass | 0.15ms | 0.30ms | 0.45ms |
| Shadow* | 0.40ms | 0.90ms | 1.30ms |
| LightingPass | 0.60ms | 1.00ms | 1.30ms |

**回归阈值**: 合并阻断条件为 >10% 性能回归（无已批准豁免）。

### 4.4 稳定性门禁（Stability Gate，对齐 §16.3）

| 检查项 | 通过条件 |
|---|---|
| 30 分钟压力运行 | 无持续 VRAM 增长 |
| Context restore | 无黑屏 |
| 内存泄漏 | 压力运行前后 VRAM delta < 阈值 |

## 5. 发布与回退策略（对齐 §16.4）

1. **灰度开关**: `render.v3.enabled`。
2. **自动回退触发条件**:
   - 检测到黑屏。
   - 性能回归 >10%。
3. **回退行为**:
   - 自动切换到 V2 路径。
   - 发出阻断报告。
   - 阻止合并。
4. **回退演练**: 必须有注入故障的回退演练测试。

## 6. 截图差异回归（对齐 §20.5）

1. 关键场景视觉回归通过截图对比自动化检查。
2. 差异阈值可配置（建议 SSIM > 0.95 或像素差异 < 2%）。
3. 超阈值差异自动标记为 review 需求。

## 7. 风险矩阵追踪（对齐 §19）

| 风险 ID | 风险 | 影响 | 缓解 | 验证方法 |
|---|---|---|---|---|
| R-V3-001 | Shadow Atlas 溢出抖动 | 阴影闪烁 | 确定性淘汰 + 滞回策略 + 日志计数 | Atlas 压力测试 |
| R-V3-002 | Cluster 溢出导致漏光 | 视觉错误 | 固定裁剪优先级 + 溢出统计 + 回归用例 | 128+ lights 回归 |
| R-V3-003 | ABI 偏移错位 | 难排查渲染异常 | 生成链路唯一化 + CI layout 快照 | ABI 快照测试 |
| R-V3-004 | Tier 降级抖动 | 帧时间波动 | 降级冷却时间 + 恢复阈值滞回 | Tier 切换压力测试 |
| R-V3-005 | 热重载中断 | 黑屏/资源错乱 | 双缓冲句柄 + 验证后替换 | 热重载故障注入测试 |

每个风险必须在本 Track 中有对应的验证任务。

## 8. 输出产物

1. **门禁报告**: JSON + CSV 格式，可存档和对比。
2. **基线快照**: 每次构建的性能基线数据，用于趋势分析。
3. **回退决策日志**: 自动回退时的完整决策链记录。
4. **风险验证清单**: 每个风险的缓解措施验证状态。

## 9. Acceptance Criteria（对齐 §20）

1. 门禁套件可在本地和 CI 中执行。
2. 报告可复现且可存档。
3. 回退行为确定性。
4. 合并策略由工具强制执行，非仅人工 review。
5. 代码与资产契约已落地并通过构建。
6. `build.bat`、`build.bat analyze`、`build.bat perf` 通过。
7. Low/Medium/High/Ultra 全矩阵验证通过。
8. 三档性能阈值与新增 Pass 预算达标。
9. 关键场景视觉回归通过（截图差异阈值受控）。
10. V3 异常可自动回退 V2，且有可追踪日志证据。
