# GPU RenderGraph and Resource Foundation 整改债务登记

> **Track ID**: `gpu_rendergraph_resource_foundation_20260726`
> **状态**: Open — production NO-GO
> **依据**: [生产整改 Track 集成审查](../../../docs/reviews/2026-07-26-gpu-production-remediation-tracks-review.md)

| ID | 债务 | 关闭条件 | 验证 |
| --- | --- | --- | --- |
| RG-1 | compiled plan 未保存或执行跨 pass transition | stable resource ID 的前后 access/stage transition records 成为 immutable plan 的一部分，执行器只消费该计划 | read/write、write/read、跨 stage、条件 pass 顺序合同测试 |
| RG-2 | descriptor 与逻辑资源可按名称脱离 | 每个 access 精确解析一个 typed tag/stable ID，`SceneHdrColor` 与 `SceneColor` 等别名冲突被拒绝 | producer/consumer/resource identity integration tests |
| RG-3 | registry 覆盖与帧推进不完整 | ① registry 是 observer-only：所有 buffer、VAO、query、persistent mapping 的 RAII owner 在创建成功后发 observer 事件、实际 GL 释放前注销；② 每成功正常 rendered frame 恰好一次 `AdvanceFrame`（`RenderSystem::render` 中 `graph.Execute` 成功后），非 per-pass；③ 重复注册同 `(handle, kind)` key 必须拒绝并诊断，绝不计入计数器；④ size 更新、注销、handle 复用与 persistent mapping 先于 backing buffer 移除按 2026-08-02 契约执行（见 spec §3 与 plan R3） | 2026-08-02（MS-8 W5）：FBO/color texture、FullscreenQuad VAO、ComputeBuffer、PersistentBuffer（含 Persistent mapping）、GPUEntitySystem raw render shader/quad VAO/VBO 已接入 observer-only registry，生命周期平衡经集成测试验证；exactly-one `AdvanceFrame` 在 `RenderSystem::render` 内，gate 不再手动推进；重复注册拒绝、尺寸更新防下溢、注销先于 GL 释放均已覆盖。遗留：其余专用 VAO/VBO owner 与 ResourceManager 5 个 compute shader 未登记（后者由 `unloadAll` 唯一释放，不属 registry）。生产 NO-GO，M0-C 硬件证据未产生 |
| RG-4 | reload/capability 治理有平行路径 | production renderer 只经 `ShaderReloadGovernance` 与 capability matrix 决策 reload/fallback | reload failure/retry 与 capability fallback integration tests |
| RG-5 | executor 仍依赖 legacy sync 行为 | `RenderSyncContracts`、`ScopedGLState` 与 `flush -> state guard -> execute -> flush` 的边界被记录，随后迁移或显式保留 | executor contract test 与 plan execution trace |
