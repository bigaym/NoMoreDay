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

