# 渲染与系统 Bug 记录库

> 目的：沉淀已分析问题，避免重复排查；为后续回归测试提供依据。  
> 维护规则：新增问题先建记录，再修复；状态变化必须更新“验证结果/回归防护”。

## 字段说明

| 字段 | 说明 |
|---|---|
| Bug ID | 唯一编号，建议 `BUG-YYYYMMDD-序号` |
| 发现日期 | 首次确认问题的日期 |
| 发现阶段 | 开发 / 联调 / 手测 / 回归 |
| 严重级别 | `P0` 阻断、`P1` 严重、`P2` 一般、`P3` 轻微 |
| 状态 | `Open` / `In Progress` / `Resolved` / `Verified` / `Closed` |
| 症状描述 | 玩家或开发可见的异常现象 |
| 复现步骤 | 稳定复现路径（尽量最短） |
| 触发条件 | 环境、画质档位、状态机路径等 |
| 影响范围 | 受影响模块、玩法或平台 |
| 根因 | 确认后的技术根因 |
| 关联文件 | 关键代码文件路径 |
| 解决方案 | 修复策略与关键改动点 |
| 验证结果 | 修复后结果与证据（日志/截图/测试） |
| 回归防护 | 新增日志、自动化测试、人工检查点 |
| 备注 | 其他上下文（风险、后续计划） |

## Bug 列表

| Bug ID | 发现日期 | 发现阶段 | 严重级别 | 状态 | 症状描述 | 复现步骤 | 触发条件 | 影响范围 | 根因 | 关联文件 | 解决方案 | 验证结果 | 回归防护 | 备注 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| BUG-20260212-001 | 2026-02-12 | 手测 | P1 | Verified | 进入游戏后出现大面积异常底色，场景资源不渲染/表现错误 | 1) 进入 `GameplayState`；2) 正常开始战斗场景；3) 观察地图与实体资源渲染异常 | 渲染路径处于 `BeginTextureMode(m_sceneRT)`（离屏 RT）时触发 | 主游戏场景渲染稳定性；资源可见性；测试验收 | 将内部 HDR/Composite 强制在离屏 RT 路径启用，破坏了 `GameplayState` 既有离屏渲染流程与相机空间约束 | `src/engine/render/RenderSystem.cpp`; `src/game/states/GameplayState.cpp` | 本轮恢复并锁定安全门控：`useHdrSceneBuffer = bloomEnabled && (compositeTarget.framebuffer == 0)`；并收紧 Composite 分支（仅 `useHdrSceneBuffer` 时使用 PostProcess 输出），避免离屏 RT 复用默认 framebuffer 后处理链 | 2026-02-13：离屏渲染风险已锁定。此前观察到的 Low/Ultra 表现一致问题已确认由 BUG-20260213-001（粒子系统失效）导致，随其修复后，渲染分级已恢复正常 | 低频诊断日志 `RenderSystem: HDR chain ...` 保留；离屏路径安全门控锁定 | 渲染分级表现异常的子问题已由 BUG-20260213-001 彻底解决 |
| BUG-20260213-001 | 2026-02-13 | 联调 | P1 | Verified | GPU 粒子系统完全不生效（主菜单背景、技能特效均不可见） | 1) 启动游戏；2) 在主界面观察背景粒子；3) 释放包含粒子的技能 | 任意档位；包含异步回读机制开启时 | 视觉特效表现；Phase 4 验收结果；主界面完整性 | 1) `GPUUtils` 状态丢失导致始终退回 `COMPAT` 模式；2) `Update` 中 `m_currentParticleCount` 仅每 60 帧根据延迟回读更新，导致调度死锁（计数常驻为 0）；3) 缺少 `rlDrawRenderBatchActive` 导致渲染状态与 Raylib 批处理冲突 | `src/engine/render/GPUUtils.cpp`; `src/engine/render/GPUParticleSystem.cpp` | 1) 缓存 `GPUUtils` 支持信息，恢复 `PERSISTENT` 模式；2) 建立每帧 CPU 侧粒子计数估算机制（基础值+新发射），仅将回读作为兜底校准；3) 在 `Render` 前强制刷新 Raylib 批处理；4) 加固每 Pass VAO 绑定 | 2026-02-13：主菜单背景粒子回归；日志显示 `Estimated` 与 `Readback` 计数器高同步（差异 < 5）；技能特效正常显示并遵循档位差异 | 增加低频（每 60 帧）粒子计数同步对比日志（`Estimated vs Readback`）；关键 OpenGL 指令前必须调用 `rlDrawRenderBatchActive` 存入开发规范 | 此 Bug 隐蔽性极高，涉及异步状态机同步，修复逻辑具普适性，不涉及“硬编码”强行赋值 |
| BUG-20260214-001 | 2026-02-14 | 手测 | P2 | Verified | Debug 构建下点击“开始游戏”后进程异常退出（产生 `.dmp`），RelWithDebInfo 暂未复现 | 1) 使用 Debug 构建启动；2) 进入主菜单点击开始游戏；3) 观察到异常退出 | 当前仅在 Debug 构建观察到；RelWithDebInfo 同路径未复现 | 开发期联调稳定性；可能掩盖潜在 UB/竞态 | 1) `glBufferStorage` 使用了非法标志 `GL_MAP_FLUSH_EXPLICIT_BIT` 导致映射失败回退；2) `PersistentBuffer::FlushRange` 在 Persistent 模式下未绑定 Buffer 导致对 Buffer 0 进行刷新；3) `ItemFactory` 等系统中 `std::uniform_int_distribution` 在特定稀有度下参数非法 (`min > max`) 触发 Debug 断言 | `src/engine/render/PersistentBuffer.cpp`; `src/engine/render/MDIRenderer.cpp`; `src/game/systems/item/ItemFactory.cpp`; `src/game/systems/item/DropSystem.cpp`; `src/game/systems/world/EnemySpawnSystem.cpp` | 1) 分离 Storage 和 Map 标志位；2) 显式绑定刷新；3) 全量加固随机数分布函数，增加 `std::max(min, max)` 保护和逻辑分支校验 | 2026-02-15：Debug 模式不再崩溃，随机数断言修复确认有效。Release 模式下全量测试通过 | 随机数参数严格校验逻辑合入开发规范；PersistentBuffer 移动语义保护 | 此次修复解决了多处导致 Debug 构建不稳定的底层隐患，提升了系统的整体健壮性 |
| BUG-20260215-001 | 2026-02-15 | 回归 | P2 | Verified | 基线 VFX 资产加载时持续出现 `unknown material ... fallback to 0`，导致材质替换事件退化并污染性能日志 | 1) 执行 `.\build.bat perf`；2) 观察 VFX 资产加载阶段日志；3) 可见 `HoloBlade/FireGlow/...` 被误判 unknown | `VFXSequenceManager` 先于材质注册初始化时触发 | 渲染特效一致性；性能证据可信度；VFX 材质替换行为 | 序列解析在材质名转 ID 时未确保 `MaterialManager` 已初始化，导致预置材质映射表为空 | `src/engine/vfx/VFXSequenceManager.cpp`; `src/engine/render/MaterialManager.cpp`; `tests/performance/MaterialVFXBenchmark.cpp` | 在 `VFXSequenceManager::LoadFromJson` 前置调用 `MaterialManager::Get().Initialize()`，确保解析期材质注册可用 | 2026-02-15：`.\build.bat perf` 输出中基线 VFX 未再出现 unknown-material fallback 警告；性能套件 `57/57` 通过 | 将“序列解析前材质注册必须就绪”纳入 VFX 资产加载约束；保留 perf 日志作为回归基线 | 属于渲染表现回归风险，已在 `render-risk-msvc-hardening_20260215` 完成修复并验证 |
| BUG-20260215-002 | 2026-02-15 | 回归 | P2 | Verified | 构建后测试可能执行旧版 `bin\NoMoreDayTests.exe`，导致基准证据与当前源码不一致 | 1) 修改性能测试代码后执行 `.\build.bat perf`；2) 对比日志与源码文本；3) 发现日志仍为旧用例输出 | VS 多配置输出落在 `bin\<Config>` 且根 `bin` 有旧可执行时触发 | CI/本地验证可信度；性能回归判定 | `build.bat` 固定调用根 `bin\NoMoreDayTests.exe`，未与实际配置产物路径对齐 | `build.bat`; `CMakeLists.txt`; `tests/CMakeLists.txt` | 1) 统一 `NoMoreDay/NoMoreDayTests` 输出目录到根 `bin`（含各配置覆盖）；2) `build.bat` 优先执行根 `bin\NoMoreDayTests.exe`，配置路径仅作兜底 | 2026-02-15：`build.bat` 日志显示 `Using test executable: ..\bin\NoMoreDayTests.exe`；`build/analyze/perf` 全部通过且输出匹配新测试逻辑 | 在构建脚本中打印测试可执行路径，降低误用陈旧二进制风险 | 属于最近改动引入的验证链路回归风险，已闭环修复 |

