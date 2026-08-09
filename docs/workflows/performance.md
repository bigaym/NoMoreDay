# NoMoreDay 性能测试流程

## 工具与准备

| 工具 | 位置（环境变量） | 用途 |
|---|---|---|
| Tracy v0.13.1 | `%NMD_TRACY%`（`tracy-profiler.exe` GUI、`tracy-capture.exe` 命令行抓取） | CPU 热点、帧时间、线程、内存分配分析（需构建集成） |
| RenderDoc v1.45 | `%NMD_RENDERDOC%`（`qrenderdoc.exe` GUI、`renderdoccmd.exe` CLI） | OpenGL 帧捕获与 GPU 侧分析（无需改代码） |
| VS 2026 性能探查器 | `%NMD_VS_DIR%` | 免集成采样（PDB 符号即可） |
| WPR + WPA | 系统自带 | 全系统 ETW 采样兜底 |

环境变量由用户在用户级环境（HKCU）设置，前缀 `NMD_`：`NMD_DEVTOOLS`（工具根）、`NMD_TRACY`、`NMD_RENDERDOC`、`NMD_WINDBG_CDB`（cdb.exe 全路径）、`NMD_VS_DIR`。PowerShell 中引用为 `$env:NMD_TRACY`，cmd 中为 `%NMD_TRACY%`。

1. 构建使用 `build.bat` 默认的 `RelWithDebInfo` 配置（含 PDB，符号是 Tracy 采样和 VS 探查器定位代码行的前提）；不要用 `debug` 配置测性能。
2. 目标预算：60+ FPS（单帧 16.67ms）、1 万实体（参考 `conductor/tech-stack.md`）。
3. 测试前先确认内存等基线指标（`conductor/rendering_system_progress.md` 中记录的历史数据），保证同一场景可比。
4. Tracy 目前未集成进构建（`CMakeLists.txt` 无 `TRACY_ENABLE`）；需要做 CPU 热点分析时先完成下节集成，否则用 VS 性能探查器或 WPR 采样。

## CPU 热点分析（Tracy）

### 集成（一次性）

1. 在 `CMakeLists.txt` 为游戏目标添加 `TRACY_ENABLE` 编译定义；raylib 自身已内置 Tracy 支持（定义 `TRACY_ENABLE` 即可打开 raylib 内部 zone）。
2. 若需内存分配统计，再定义 `TRACY_ENABLE_ALLOCATORS`；若需周期采样调用栈，定义 `TRACY_SAMPLING_HZ`（如 8000）并确保 PDB 可被符号解析。
3. 重新构建后，集成有 `ZoneScoped` 或 raylib zone 的代码会自动向 Tracy 上报。

### 抓取

- GUI 连接：先启动 `%NMD_TRACY%\tracy-profiler.exe`，再运行游戏，在 Tracy 主窗口选择连接 `localhost:8086`（默认端口）。
- 命令行抓取（后台、可控时长）：
  ```
  %NMD_TRACY%\tracy-capture.exe -o <输出>.tracy -a 127.0.0.1 -s <秒数>
  ```
  `-s` 指定抓取秒数，结束自动保存；不带 `-s` 时 Ctrl+C 停止。
- 回放已有抓取：启动 `tracy-profiler.exe` 后直接加载 `<输出>.tracy` 文件。

### 分析要点

1. 帧时间图：红色超预算（>16.67ms）的帧即掉帧点，点击定位到该帧。
2. Zone 视图：按自耗时/总耗时排序，找热点函数；配合调用栈（Call Stack）看是谁调进来的。
3. 线程泳道：检查 Taskflow 工作线程的并行度与空闲，找串行化瓶颈（锁、同步、单线程聚集）。
4. 内存视图（若开启分配器）：查高频分配点与峰值，结合 mimalloc 配置核对。

### 导出与留存

- `%NMD_TRACY%\tracy-csvexport.exe <输入>.tracy > <输出>.csv` 导出文本结果，便于写入报告/对比基线。
- 抓取文件（`.tracy`）保留在测试产物目录，原始文件不写入记忆或 Bug 条目，只写结论。

## GPU 帧分析（RenderDoc）

1. 启动 `%NMD_RENDERDOC%\qrenderdoc.exe`。
2. 两种挂载方式：
   - 直接启动：`qrenderdoc.exe <游戏exe>`（自动注入捕获层）。
   - 注入已运行进程：File → Inject into Process → 选择游戏进程。
3. 抓帧：游戏内按 `F12`（默认快捷键）抓取当前帧；可在 Capture Options 中设置连续抓 N 帧或定时抓取。
4. 分析：
   - Draw Call 列表：按耗时排序，找高开销 draw，看状态切换与冗余调用（如未裁剪的离屏实体）。
   - 着色器调试：进入某 draw 的 Shader Debugger，逐步验证 shader 逻辑。
   - 资源检视：纹理/缓冲查看，核对分辨率、格式、Mip 链。
   - 性能计数器：Action 列表里查看 GPU 时间、带宽等计数器（AMD/NVIDIA 插件随包提供）。
5. 命令行抓帧（无 GUI 环境）：`%NMD_RENDERDOC%\renderdoccmd.exe capture -o <输出目录> <游戏exe> <参数>`；分析仍需用 GUI 打开 `.rdc` 文件。
6. 抓帧文件（`.rdc`）留存路径记录在测试报告；结论按下节回写。

## 快速采样（免集成，兜底）

- VS 性能探查器：对游戏 exe 做采样（sample），以 CPU 百分比排序热点；配合并发可视化看 Taskflow 线程利用率。要求 exe 旁有 PDB。
- WPR：`wpr -start generalprofile` → 运行游戏场景 → `wpr -stop <输出>.etl`，用 WPA 打开分析；适合跨进程/系统级占用问题。

## 报告与回写

1. 每次测试记录：构建版本/哈希、场景与输入、样本帧数、捕获文件路径、关键数字（帧时间 p95、热点函数耗时占比等）。
2. 性能缺陷走 Bug 登记：memory 工具建 `BUG-YYYYMMDD-NNN` 记忆条目（`memory_memory_store`，`conversation_id=bug:<BUG-ID>`），严重级别按掉帧影响定级，模块标签用 `perf`；规范见 `docs/workflows/debugging.md`「缺陷记录与检索（memory）」。`conductor/bug_registry.md` 已封存，不再登记。
3. 优化项按设计 → 计划 → 实施 → 测试流程推进；完成后复测同一场景并对比基线，验证数字写回 Bug 记忆条目。
4. 若性能问题改变渲染合同或风险等级，同步更新 `conductor/specs/rendering_engine_v5_master_spec.md`、`conductor/rendering_system_progress.md` 与相关计划。

## 记录要求

- 结论必须可复现：命令、场景、版本齐备，采样点明确。
- 只写可定位的结论与数字，不把原始抓取文件/大日志写入记忆或 Bug 条目。
- 分析用的命令本身即「记录要求」中的可执行验证命令，保留在 Bug 记忆条目中。
