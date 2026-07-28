# GPU RenderGraph and Resource Foundation 整改债务登记

> **Track ID**: `gpu_rendergraph_resource_foundation_20260726`
> **状态**: Open — production NO-GO
> **依据**: [生产整改 Track 集成审查](../../../docs/reviews/2026-07-26-gpu-production-remediation-tracks-review.md)

| ID | 债务 | 关闭条件 | 验证 |
| --- | --- | --- | --- |
| RG-1 | compiled plan 未保存或执行跨 pass transition | stable resource ID 的前后 access/stage transition records 成为 immutable plan 的一部分，执行器只消费该计划 | read/write、write/read、跨 stage、条件 pass 顺序合同测试 |
| RG-2 | descriptor 与逻辑资源可按名称脱离 | 每个 access 精确解析一个 typed tag/stable ID，`SceneHdrColor` 与 `SceneColor` 等别名冲突被拒绝 | producer/consumer/resource identity integration tests |
| RG-3 | registry 覆盖与帧推进不完整 | buffer、VAO、query、persistent mapping 的 RAII owner 均发 observer 事件，且每 rendered frame 调用 `AdvanceFrame` | 当前已覆盖 FBO、Distortion/JFA SSBO、PersistentBuffer、timer query 和 graph frame；VAO/其余 buffer owner 仍待补齐 |
| RG-4 | reload/capability 治理有平行路径 | production renderer 只经 `ShaderReloadGovernance` 与 capability matrix 决策 reload/fallback | reload failure/retry 与 capability fallback integration tests |
| RG-5 | executor 仍依赖 legacy sync 行为 | `RenderSyncContracts`、`ScopedGLState` 与 `flush -> state guard -> execute -> flush` 的边界被记录，随后迁移或显式保留 | executor contract test 与 plan execution trace |
