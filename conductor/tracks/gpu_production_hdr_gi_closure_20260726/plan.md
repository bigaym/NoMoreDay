# GPU Production HDR/GI Closure 实施计划

> **Track ID**: `gpu_production_hdr_gi_closure_20260726`
> **依赖 Spec**: [spec.md](./spec.md)
> **状态**: [ ] Planned

---

## 实施思路/原理

以 Gameplay external render texture 为唯一输出：捕获调用方状态，将已绘 world 种入内部 HDR 工作纹理，执行同一 feature matrix 的 pass 后由 Composite 写回。用 target descriptor 和状态恢复边界取代 `offscreenV3SafeMode`，不在本 Track 重写 RenderGraph。

GI 数据在被消费前显式产出。VFX 可见绘制继续保持视觉顺序，但使用其当前帧 snapshot 写入 `EmissiveVfx`。每个 GI 输入有版本，不能通过 HDR 亮度或隐含 pass 顺序推断依赖。SDF 以负 epsilon 或 occupancy 解决零值；history 用 2D 重投影并在任何不安全条件直接拒绝。

## 伪代码引导

```text
RenderGameplay(target):
  saved = CaptureTargetState(target)
  hdr = AcquireHdrTarget(target.renderExtent)
  CopyExternalSceneToHdr(target, hdr)
  ExecuteExistingFeatureSequence(hdr, target) // never suppress because offscreen
  CopySelectedOutputToTarget(target)
  RestoreTargetState(saved)

BuildGiInputs(frame):
  key = {camera, zoom, viewport, sdfExtent, scale, occluderVersion}
  mask = RebuildOrReuseMask(key)
  sdf = RebuildOrReuseSdf(mask.version)
  emissive = Merge(Lights(), Materials(), CaptureVfxSnapshot())
  return {mask, sdf, emissive}

CompositeGi(current, metadata):
  previousUv = Reproject(currentUv, currentCamera, previousCamera)
  valid = InBounds(previousUv) && MetadataCompatible(metadata)
  valid = valid && OccupancyMatches(previousUv)
  Blend(current, history, valid ? temporalWeight : 0)
```

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
| --- | --- | --- | --- |
| 1 | 离屏 HDR 合同 | external target 到 HDR/Composite 无黑帧闭环 | [x] |
| 2 | GI 输入顺序 | 显式 emissive/occluder 生产者和 SPH 隔离 | [x] |
| 3 | GI 正确性 | SDF 符号、失效键和 history rejection | [x] |
| 4 | 回退与配置 | Tier 默认、诊断和资源清理 | [x] |
| 5 | 回归证据 | 自动化及实机离屏证据 | [x] |

## 原子任务拆分

### Phase 1: 离屏 HDR 合同

- [x] Task 1.1: 定义 external target descriptor，明确 framebuffer、viewport、scissor、extent、origin、format 与非所有权。
- [x] Task 1.2: 抽取 state capture/restore 并覆盖所有早退路径。
- [x] Task 1.3: 实现 external scene seed 到 `HdrSceneColor` 的坐标一致 blit，覆盖 resize/Y 翻转/format 诊断。
- [x] Task 1.4: 移除 `offscreenV3SafeMode` pass suppression，保留 feature flag 与 capability fallback。
- [x] Task 1.5: 统一离屏/backbuffer Composite 输出选择，关闭 HDR/postprocess 时仍回写场景。

### Phase 2: GI 输入顺序

- [x] Task 2.1: 为 mask、SDF、三类 emissive 和 combined 输入建立 version、producer 和 debug name。
- [x] Task 2.2: 将 LightManager/PBR emission 写入当前帧 GI 输入，禁止 HDR 亮度代理。
- [x] Task 2.3: 在可见 VFX 前创建只读 VFX emission snapshot，Radiance 只读该 snapshot。
- [x] Task 2.4: 调整 pass 插入顺序并为新增 image/SSBO 读写补齐 GL barrier。
- [x] Task 2.5: 移除或隔离 Fluid 对生产 mask/emissive 的写入。

### Phase 3: GI 正确性

- [x] Task 3.1: 扩展 Occluder cache key，纳入 camera、zoom、viewport、extent、scale 与内容版本。
- [x] Task 3.2: 让上述变化触发明确重建/失效并输出更新原因计数。
- [x] Task 3.3: 修改 distance resolve/radiance stop，采用负 epsilon 或 occupancy 合同。
- [x] Task 3.4: 添加 SDF GPU readback fixture，对比 CPU 掩码、内外符号和 ray-stop。
- [x] Task 3.5: 增加 history metadata、2D reproject、disocclusion 和版本拒绝。
- [x] Task 3.6: 在 GI toggle、resize、zoom、遮挡/发光变化统一 invalidate history。

### Phase 4: 回退与配置

- [x] Task 4.1: 将所有 shipped Tier fluid 默认改为关闭、粒子数为零。
- [x] Task 4.2: 增加仅开发构建可用的 Fluid opt-in，Release 和持久化配置不得重启它。
- [x] Task 4.3: 明确 GI capability/shader 失败时的 V4/HDR 回退和诊断。
- [x] Task 4.4: 验证 feature/tier/Shutdown 后 HDR、GI、history、实验资源按现有 RAII owner 释放。

### Phase 5: 回归证据

- [x] Task 5.1: 增加 Gameplay offscreen integration test，断言 pass trace 与非黑 ROI。
- [x] Task 5.2: 增加 camera/zoom/resize、动态遮挡/发光的 GI fixtures。
- [x] Task 5.3: 增加 history rejection unit/integration tests。
- [x] Task 5.4: 在目标 GPU 保存 High/Ultra 截图、trace 与 SDF readback artifact。

## 测试方法

| 层级 | 覆盖内容 | 命令/证据 |
| --- | --- | --- |
| Unit | target descriptor、view key、SDF sign、history reprojection | `./bin/NoMoreDayTests.exe --test-case="[Unit]*GI*"` |
| Integration | V5 contract、Gameplay offscreen、invalidation、tier/Fluid 路由 | `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` |
| CI | ABI/binding 及通用渲染回归 | `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` |
| Performance | 修复不引入现有 pass 预算回归 | `ctest --test-dir build -C Release -L performance --output-on-failure` |
| Hardware | 离屏 High/Ultra、readback、fallback | 保存截图、trace、readback；未运行不得视为通过 |

每次 C++ 变更先运行 `./build.bat`，再由窄到宽运行测试。硬件 readback 缺失必须记录为未完成，不能由无头测试替代。

## 验证任务完成

- [ ] 离屏和 backbuffer 按同一 feature matrix 执行，100 次 GI/tier/resize 切换没有黑帧或 GL state 泄漏。
- [ ] SDF 和 history fixtures 满足严格符号与 rejection 合同。
- [ ] Release/所有 Tier 默认关闭 SPH，且它不写生产 GI。
- [ ] 构建、相关 CTest 和实机 artifact 完整，可交给硬件 Gate 复查。
