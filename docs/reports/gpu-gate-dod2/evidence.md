# DOD-2 实机 Gate 判定边界（RTX 4070S，2026-08-02）

> 决策（用户批准）：接受测试二进制环境局限，如实记录 DOD-2 判定边界。
> ROI/SDF/GI readback 三项因管线上下文不完整无效，不计入硬件判定；
> 以 GL 清零/capability/压力/leak/预算等有效项作为 DOD-2 结论，gate 标记 `environment_limited`。

## 1. 执行环境与命令

- 硬件：NVIDIA GeForce RTX 4070 SUPER（本机，用户指定）。
- 命令：`python scripts/gpu_hardware_validation_gate.py --revision dod2-20260801`（samples=120, toggle=100, stress=true）。
- 后续修复复跑：`--revision glfix-20260801`（GL_INVALID_ENUM 修复后）。
- 归档：`artifacts/gpu-gate/dod2-20260801/` 与 `artifacts/gpu-gate/glfix-20260801/`（`artifacts/` 已 gitignore）。

## 2. 有效判定项（计入 DOD-2 结论）

| 判定项 | dod2-20260801 | glfix-20260801 | 结论 |
|---|---|---|---|
| GL debug 消息计数 | 256 捕获 + 3,593,483 dropped | **0 / 0** | ✅ 通过 |
| severe (ERROR/HIGH) GL 错误 | >0（fail-closed → NO_GO） | **0** | ✅ 通过 |
| global_failures | 1（severe GL 消息） | **空** | ✅ 通过 |
| capability / preflight | 通过（renderer=RTX 4070 SUPER，debug callback 安装） | 通过 | ✅ 通过 |
| schema 校验 | 通过 | 通过 | ✅ 通过 |
| 压力/泄漏（registry 快照） | 已完整执行 60s（13 快照至 55s）但 verdict 被 GL 洪泛 fail-closed | 执行，无泄漏 | ✅ 无泄漏（leak_candidate_count=0, net_growth=0） |
| lambda passes 预算/结构 | — | 4 个恒入 pass 有 valid_samples 120 | 有效（仅 lambda 子集） |

## 3. 无效判定项（环境局限，不计入）

| 判定项 | 现象 | 根因 |
|---|---|---|
| 矩阵 9 格 ROI readback | `roi_mean_brightness` 恒 0 → `non_black_roi_passed=false` | 测试二进制从未调用 `RenderSystem::Initialize()`（`GPUHardwareValidationGateTest.cpp:44-56` 仅 `InitWindow(1,1)`+`GPUUtils::Initialize`）→ 全部 `g_*` pass 静态全局为 null → Lighting/GI/HeightShadow/PostProcess 链不入 graph（artifact timings 实证：9 格仅 Scene/VFX/UIWorld/Composite 有样本）；harness `RenderInput()` resources/renderContext=nullptr 且 render 不传 hooks → 场景零绘制 |
| SDF probe | `sdf_sign_valid=false` | 黑帧直接推论（见 ROI） |
| GI-on paired delta | `gi_indirect_passed` 偶真 | 黑帧下无 GI 贡献，delta 无效 |

**补充因素**：测试窗口 1×1（`InitWindow(1,1)`）→ 遗留 GL_VIEWPORT=1×1 → `s_hdrSceneBuffer` 创建为 1×1 → composite blit 只覆盖 harness FBO 左上角 1 像素 → ROI 读回全 0。

判定逻辑本身无 bug（全黑输入下必然失败）；`S7 evidence.md:146,237` 已记载同因。

## 4. GL_INVALID_ENUM 修复记录（DOD-2 暴露的真实生产 bug）

- **根因**：`src/engine/render/RenderSystem.cpp` `CaptureCompositeTargetState()` 每帧 3 次调用 `glGetFramebufferAttachmentParameteriv`，传入三个核心 GL 不存在的 pname 常量（`0x8D24`/`0x8D25`/`0x825D`）→ 实机热路径每帧 3 条 `GL_INVALID_ENUM`（type=0x824C, severity=0x9146），gate 全流程数百万条。查询自引入以来从未取得有效数据（width/height/compType 恒 0，永远走 viewport 回退）。
- **修复**（commit `5c257e2`）：删除 3 次无效查询 + 手写常量 + 缓存 PFN；`framebuffer!=0` 分支直接 viewport 回退（`internalFormat=0`，`flipY` 不变）；`ScopedTargetStateGuard` 保存/恢复逻辑原样。仅触碰该函数。
- **验证**：实机 dropped 3,593,483→0、debug 256→0、severe 0；`git grep` 无遗留 pname 引用；0x8CE0 为合法 `GL_COLOR_ATTACHMENT0` 与本次无关。

## 5. DOD-2 判定边界结论

- **gate_status = NO_GO（environment_limited）**：fail-closed 保持；矩阵 readback 三项在测试二进制上下文无效，不计入硬件判定。
- **有效结论**：RTX 4070S 实机 GL 诊断清零、capability/preflight/压力/泄漏/lambda passes 预算有效。
- **后续**：真实 readback 判定需移至游戏二进制上下文（`RenderSystem::Initialize()` + 真实 hooks + 正确 viewport + 游戏侧 FixtureRenderDriver 或 `--gate` 模式），为 S6 契约范围外工作，另行立项。

## 6. 命令与证据

- `python scripts/gpu_hardware_validation_gate.py --revision dod2-20260801` / `--revision glfix-20260801`
- artifact：`artifacts/gpu-gate/{dod2,glfix}-20260801/gpu_hardware_validation_artifact.json`
- 支持证据：`docs/reviews/2026-08-01-modular-split-ms-6-review.md`（GL 修复 + 矩阵根因记录）
