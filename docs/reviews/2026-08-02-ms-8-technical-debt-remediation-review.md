# MS-8 技术债修复 — Review

Date: 2026-08-02

## 审查目标

验证 MS-8 七项技术债（`docs/reports/modular-split-exe-lib-dll/ms-8/evidence.md`）的修复，依据 `docs/designs/ms-8-technical-debt-remediation-design.md` 与 `docs/plans/ms-8-technical-debt-remediation-implementation-plan.md`，覆盖 W1-W6 六个工作包。审查按包独立进行（每次派 reviewer 只读审查 + 主代理/修正轮落地），本文为全包汇总。

## 结论

最终：`提交`（六包均通过至少一轮审查修正与最终验证；剩余风险与 backlog 如实登记，生产 GPU 保持 NO-GO）

## 逐包审查结果

### W1 — S1b 配置持久化数据丢失（P1）

**发现（审查轮）**：Blocker—S7 旧断言仍期望重初始化后 GI 变 false；High—JSON 合法但 root/render 非 object 未校验、已存在文件读失败被当空 JSON 截断、回归矩阵缺失；Medium—V3 serializer 整体替换 `render.v3` 删除未知字段；BUG registry 未登记。

**修复**：`WriteV3ConfigToJson` 仅 merge 18 个已知 V3 字段（保留未知 child 与 flat `render.v3.enabled`，ReleaseGate 依赖）；`WriteV3ConfigToFile` 与 `PersistSelectionMetadata` 全路径 fail-closed（open/parse/root 非 object 均 LOG_WARN 保守返回，绝不清空既有文件）；两处写入改为原子写（同目录临时文件 + `MoveFileExW` 替换，失败保留目标）；V3 setter 返回 `changed && persisted`；runtime GI override / auto-degrade 永不写盘（`m_config` 不作为持久化源）。

**验证**：`tests/unit/QualityTierManagerTest.cpp` 新增 4 个 TEST_CASE（metadata refresh 保留 sentinel 与 GI=true、override 与 6 级 degrade 不持久化、V3-only 写入、非 object 结构四 SECTION fail-closed，`ReadRaw` 字节级证明文件未被触碰）；`S7PairedGiDeltaTest` 断言改为 persisted/effective 均 true。

**Waiver**：`conductor/bug_registry.md` 因他人 BOM 变更未合并 BUG 行，登记于本报告与 memory。

### W2 — 模块边界盲点 + 死 P0

**发现（审查轮）**：High—正则只匹配字面 `#include` 且大小写敏感（`# include <App/...>` 可绕过）；Medium—`isinstance(line, int)` 接受 JSON bool；PCH 扫描/Engine-owned metadata 无测试。

**修复**：schema 1.0→2.0 按根声明策略（Core 禁 `engine/,game/,app/`；Engine 禁 `game/,app/`；Game 禁 `app/`；Engine-owned PCH 禁 `game/,app/`）；Game 加入扫描；direct include 同时支持 quote/angle、`^\s*#\s*include\s+` 预处理器空白、casefold 判定但 evidence 保留原始拼写；entry `line` 显式拒 bool；P0 常量/分支/`p0_blocking` 字段全删（legacy 字段严格 schema 拒绝，exit 2）；ledger 迁移至 schema 2.0（3 roots + PCH policy，entries 空）。

**验证**：`python -m unittest tests/python/ModuleBoundaryCheckerTest.py -v` 23 tests OK；真实仓库 `python scripts/check_module_boundaries.py` → `Observed/ledger edges: 0/0; files: 0` PASS；旧 schema fail-closed exit 2；`py_compile` OK。

### W3 — Skill VFX 标签掩码耦合

**发现（审查轮）**：Blocker—测试镜像函数未覆盖真实 producer（改错 `SkillSystem.cpp` 映射仍可通过）；P2—recipe/legacy 两条路径重复归一化告警两次；P2—测试锁定 generated Tag 具体 bit 值。

**修复**：`SkillSystem.hpp` 新增公开无副作用 `EncodeSkillVfxElementType(Tag)`/`ResolveSkillVfxElementTypeFromTags(...)`（真实生产实现，非测试镜像）；删除 `SkillVfxElementTagMask`、`effectiveTagMask`、`static_cast<uint32_t>`、`ResolveLegacyElementType`；Engine 新增 `NormalizeSkillVfxEvent` 在 `SubmitSkillEvent` 入队前单边界归一化一次，recipe/legacy 路径只消费已归一化事件；测试调用真实 helper、删除 bit 值断言、仅用 named/high-bit tags。

**验证**：legacy symbols grep 0 命中；单测覆盖 Void>Lightning>Cold>Fire>Physical 优先级、Shadow/Poison→Physical fallback、transmuter override、非法 scalar→Physical。

**登记残余风险（P2）**：未增加 queue→consumer 生产 seam 的行为级断言（测试直达公开 helper 与真实 normalize，未经过 `m_pendingEvents`/`StageSkillEvents`/recipe 选择）；已在计划与 memory 登记，后续 M0 相关包可补正式 seam。

### W4 — Release+LTO 未证明

**发现（审查轮）**：Blocker—smoke 实为 `NoMoreDayTests.exe` doctest 而非 `NoMoreDay.exe`；`--ctest-ignore-regex` 自由正则可吞失败；High—固定文件名覆盖证据、无约束 rmtree、build log 放必删目录、`--source-dir` 未生效；Medium—/GL 仅证明存在未按 target 断言。

**修复**：根 CMake 显式 `option(ENABLE_LTO OFF)` + `check_ipo_supported(LANGUAGES C CXX)` + Release-only IPO 块（7 个必需目标 `INTERPROCEDURAL_OPTIMIZATION_RELEASE` 断言 + configure-time FATAL_ERROR）；`main.cpp` 新增 `--smoke-test`（Logger 初始化后输出标记退出 0，不建窗口/GL）；`scripts/verify_release_lto.py` 改为精确 `--known-failure` allowlist（默认两项既有失败）、`configure_cmd` 接线 source_dir、rmtree 限定规范隔离目录、时间戳证据目录 `docs/reports/release-lto-proof/evidence-<stamp>/`、per-target /GL 断言。

**验证**：全新隔离 cache 255s 重建；/GL 822 行、/LTCG 2 行（含 `NoMoreDay.exe` `/LTCG:incremental` + "All 81668 functions were compiled"）；configure_report 7 目标 TRUE、vcxproj WPO=true；Release CTest `PASS_WITH_KNOWN_FAILURES`（仅 SkillUI 陈旧断言，与 LTO 无关，RelWithDebInfo 同样失败已直接验证）；`NoMoreDay.exe --smoke-test` exit 0。证据归档 `evidence-20260802-100053/`。**未宣称任何 GPU readiness。**

### W5 — RG-3 GPUEntitySystem/RenderGraph 生命周期（M0-B）

**前置**：M0-A explore 调研（用户批准）确认 R1 已实现、R2 部分、R3 缺失、退出条件 A/B 缺证据、C 已实现、D 无实机 GO；`plan.md:112` 声明 R1-R4 依赖 M0-B 合同 → 依赖序允许 W5 先行；M0-A 不可验收结论不变。

**发现（审查轮）**：High—PersistentBuffer Destroy/move 后状态未复位（Persistent 模式访问空 m_fences UB）、OrphanAndUpload 未同步 m_size/registry bytes、生命周期测试断言恒真/无基线、默认 move 复制裸句柄；Medium—Init 前序失败无回滚、pending age 边界（`<=9` vs `<9`）未锁定。

**修复**：`PersistentBuffer::ResetState()` 纯状态复位（Destroy/move 源调用）；`ComputeBuffer::OrphanAndUpload` 更新 m_size + `UpdateResourceSize`；GPUEntitySystem copy/move 全 `=delete`；Init 逐步检查 + failInit lambda 回滚（`W5ScopedFileHider` 注入 `grid_scan.compute` 失败测试）；registry pending `age < kPendingReferenceFrames`（age 0..8 九帧）+ 边界测试；生命周期测试 Reset 后 baseline 归零断言、具体 handle 注销、destroyed-baseline>=created、unloadAll 后 GL drain。

**契约**：registry duplicate (handle,kind) reject+LOG_ERROR 保留原记录、missing no-op+LOG_WARN、饱和减法防下溢；ResourceKind +VertexArray/ShaderProgram（枚举尾部，barrier 不破）；observer 绑定 ComputeBuffer→StorageBuffer、PersistentBuffer→StorageBuffer(+PersistentMapping)、VAO→VertexArray、VBO→VertexBuffer、raw shader→ShaderProgram、FullscreenQuad VAO 恢复；exactly-one `AdvanceFrame()` 于 `RenderSystem::render` graph.Execute 成功后一次、gate 手动 advance 删除；`Shutdown()` 幂等全释放（physicsOutput/blockSum/raw shader/quadVAO/VBO）、不 touch 5 个 ResourceManager compute shader（unloadAll 唯一释放）、析构 default 不承担 GL。

**验证**：build.bat 全 gate PASS（legacy/version marker 无新 v3 命中）；`*W5*` 4/28、`*GPUResourceRegistry*` 8/57、`*RenderGraph*` 31/217；ctest `-L unit` 仅已知 flaky（HeavenlySwordClosure）。M0-B 文档 4 处更新（debt_register RG-3 行、spec §3、plan、validation 2026-08-02 两小节）。

**Backlog**：外部 target 合同（5c257e22 删除 attachment 查询）为 M0-B 后续项；未登记 VAO/VBO owner 与 ResourceManager shader 可观测性。

### W6 — 游戏二进制 GPU 门禁（M0-C）

**发现（审查轮）**：Blocker—pass trace 为模拟 RenderGraph 非真实执行、GI paired 不进 verdict、SDF 以合成颜色代理、runner 仅取首个 BEGIN/END 不拒重复且空字段可通过；High—ROI readback 忽略 x/y、vendor 硬编码 "OpenGL Driver"、hooks 缺失未转 NOT_RUN、异常路径无 JSON report；Medium—诊断钳制与文档不符、validation.md 旧段落误导。

**修复**：pass trace 真实化（`RenderSystem::GetLastExecutedPassOrder()` 经 `graph.GetCompiledPlan().passOrder`，删除模拟 testGraph）；GI paired delta 逐 cell 进 verdict；`ProbeGiDistanceField` 真实 `glGetTexImage` 读 JFAPass GL_R16F + 5 点 sign probe（GI-on 必 passed / GI-off not_applicable）；occupancy `missing_pending_m0a` + `blocks_go=true` 全局禁 GO（fail-closed，不实现 M0-A R3）；ROI 带坐标 CPU 裁剪（`ComputeRoiMeanLuma`，非零原点测试）；真实 `glGetString(GL_VENDOR/GL_VERSION/GL_RENDERER)`，空→NOT_RUN；`FixtureRenderDriver::IsProductionDriver()`（默认 false，Engine dependency-neutral），App 层 `GpuGateDriver` 实现（1280x720 RGBA16F FBO）；main.cpp 异常路径完整 NOT_RUN report（marker+BEGIN+JSON+END，恰好 1 marker+1 report）；显式参数按 requested 执行 + `non_exhaustive=true` 禁 GO，默认 120/100；runner 严格 schema（空身份拒绝、GO 时 non_exhaustive/blocks_go 拒绝）；validation.md §15 历史标注 + UTF-8 统一；`GPUParticleSystem::Shutdown` 补释放 4 个 buffer（baseline CRT 退出静态析构 Access Violation）。

**验证**：build.bat PASS（legacy 217/70）；`ctest -L gpu`：contract 0.91s ✅、diagnostic 22.08s ✅（2 cases/328 assertions）、gpu.hardware 83.51s **NO_GO（按设计 fail-closed）**；`ctest -L integration` 6/6；`ctest -L ci` 仅 2 项既有失败（UITests.cpp:438 陈旧断言、HeavenlySwordClosure flaky）；Python runner 38 tests OK；`NoMoreDay.exe --gpu-gate`（3 帧/1 切换最小样本）NO_GO exit 0 恰好 1 marker，真实 vendor= NVIDiA Corporation / driver 4.3.0 NVIDIA 591.86 / hooks=true / 9 cells / 真实 7-pass trace / SDF 7/9 / occupancy blocks_go=true；runner e2e exit 1 fail-closed，归档 `artifacts/gpu-gate/local-min-20260802d/gpu_hardware_validation_artifact.json`（367KB，gate_status=NO_GO、succeeded=false、rc=0、schema_errors=0，主代理已逐字段抽查）。

## 验证汇总

| 命令 | 结果 |
|---|---|
| `cmd.exe /c build.bat`（多次，W5/W6 各一次） | 全 gate PASS（legacy/version marker、module boundaries、MS-1、render ABI、assets） |
| `python -m unittest tests/python/ModuleBoundaryCheckerTest.py -v` | 23 tests OK |
| `python scripts/check_module_boundaries.py` | `0/0` PASS |
| `python -m unittest tests/python/GpuHardwareValidationGateRunnerTest.py` | 38 tests OK |
| `bin\NoMoreDayTests.exe --test-case=*W5*` / `*GPUResourceRegistry*` / `*RenderGraph*` | 4/28、8/57、31/217 |
| `ctest -L gpu` / `-L integration` / `-L unit` / `-L ci` | gpu 分层 ✅（hardware NO_GO 按设计）、integration 6/6、unit 仅已知 flaky、ci 仅 2 项既有失败 |
| `ctest -C Release -L ci`（W4） | PASS_WITH_KNOWN_FAILURES（仅 SkillUI 陈旧断言） |
| `verify_release_lto.py`（隔离 cache） | /GL 822、/LTCG 2、7 目标 WPO、exe smoke exit 0 |
| `git diff --check` | 通过（仅 LF/CRLF 警告） |

## 未解决 / 剩余风险

1. **M0-A 不可验收**：R3 occupancy/disocclusion 未实现（门禁因此 `blocks_go` fail-closed）、R2 部分（emissive/occupancy version 缺口）、R4 覆盖不全、退出条件 A/B 缺验证证据；plan Phase 1-5 虽 [x] 但退出条件与 R1-R4 全部 [ ]。
2. **生产 GPU 仍 NO-GO**：W6.7 部分完成（机制已验证，120 样本/100 切换/60s 压力/三 fixture 完整矩阵待实机采样，需 DOD-2 目标硬件）。
3. **raylib 离屏 FBO 集成缺陷**（本地 256 次 `GL_INVALID_OPERATION: Array object is not active` + ROI 全黑）：归 M0-C 生产修复，不影响门禁 fail-closed 语义。
4. **外部 target 合同不完整**（5c257e22 删除 `glGetFramebufferAttachmentParameteriv` 查询）：M0-B 后续项。
5. **W3 queue→consumer 行为级 seam** 缺失（P2，已登记）。
6. **既有 2 项无关失败**：`UITests.cpp:438` 陈旧源码文本断言（BUG-20260314-001 重构后失效）、`HeavenlySwordClosureTests.cpp:97` flaky；记录不修。
7. **`crashes/` 4 个崩溃转储**（2026-08-02 20:23-20:35，W6 修复的 GPUParticleSystem CRT 退出崩溃产物）：untracked，建议清理或加入忽略，不影响仓库。
8. **未提交任何 commit**（用户未授权）。

## 下一步

1. 用户授权后按包分批提交（W1-W6）。
2. M0-A 续作（R2 补齐 → R3 occupancy/disocclusion → R4 fixtures → 退出条件 A/B 证据）→ M0-B 外部 target 合同收尾 → M0-C 实机 DOD-2 完整采样产出 GO/NOT_RUN artifact。
