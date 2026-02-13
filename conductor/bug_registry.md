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
| BUG-20260212-001 | 2026-02-12 | 手测 | P1 | Resolved | 进入游戏后出现大面积异常底色，场景资源不渲染/表现错误 | 1) 进入 `GameplayState`；2) 正常开始战斗场景；3) 观察地图与实体资源渲染异常 | 渲染路径处于 `BeginTextureMode(m_sceneRT)`（离屏 RT）时触发 | 主游戏场景渲染稳定性；资源可见性；测试验收 | 将内部 HDR/Composite 强制在离屏 RT 路径启用，破坏了 `GameplayState` 既有离屏渲染流程与相机空间约束 | `src/engine/render/RenderSystem.cpp`; `src/game/states/GameplayState.cpp`; `assets/vfx/sword_slash.json`; `assets/vfx/lightning_strike.json` | 本轮恢复并锁定安全门控：`useHdrSceneBuffer = bloomEnabled && (compositeTarget.framebuffer == 0)`；并收紧 Composite 分支（仅 `useHdrSceneBuffer` 时使用 PostProcess 输出），避免离屏 RT 复用默认 framebuffer 后处理链；同时补充 High 档位非 Distortion 事件拉开 Low/Ultra 差异 | 2026-02-13：`build.bat` 通过；`NoMoreDayTests.exe` 全量通过；`bin/logs/NoMoreDay.log` 未检出 GL error，且存在 `VFXSequenceManager: loaded 10 sequence assets from assets/vfx` 记录；实机截图复核待补充 | 低频诊断日志 `RenderSystem: HDR chain ...` 继续保留；离屏路径安全门控保留；高档位差异改为包含非 Distortion 事件，避免单点依赖 | 待补充同场景 Low/Ultra 截图与 Gameplay 稳定渲染截图后，再推进状态到 `Verified/Closed` |

