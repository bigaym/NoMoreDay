# NoMoreDay 调试流程

## 开始前

1. 先执行 `memory_mistake_note_search(<任务上下文>)` 与 `memory_memory_search(tags=[bug])` 查询已知坑、相关记忆、规格、测试和运行证据（历史缺陷归档见已封存的 `conductor/bug_registry.md`）。
2. 对新增缺陷，在修改实现前创建或更新 Bug 记忆条目，使用 `BUG-YYYYMMDD-NNN` 编号并标记严重级别和状态（见「缺陷记录与检索（memory）」）。
3. 先建立最短可重复路径，明确触发条件、影响范围和预期行为；无法稳定复现时，记录观察条件与证据缺口。
4. 崩溃、死锁、内存等运行时缺陷，优先用 cdb 抓取/分析 dump 作为最短复现证据（见下节）。

## 崩溃与运行时分析（cdb）

cdb 是 WinDbg 的命令行版本（与 WinDbgX 共用 dbgeng.dll 调试引擎），适合在 CLI 环境中附加进程、抓 dump、分析崩溃，无需图形界面。

1. 定位 cdb：环境变量 `%NMD_WINDBG_CDB%`（用户级已设置，指向 WinDbg 包内 cdb.exe）；未设置时用 `where.exe cdb` 查找，或按 WindowsApps 包内路径形如 `C:\Program Files\WindowsApps\Microsoft.WinDbg_*_x64__8wekyb3d8bbwe\amd64\cdb.exe` 定位。
2. 常见场景与命令：
   - 分析崩溃 dump：`cdb -z <dump.dmp> -c "!analyze -v; .ecxr; kb; q"`
   - 附加运行中进程查栈：`cdb -p <PID> -c "kb; q"`
   - 带子进程启动并抓崩溃：`cdb -o <exe> <args>`
   - 捕获新 dump：`.dump /ma <path>`（在 cdb 会话内）
3. 常用调试命令：`!analyze -v`（自动定位根因）、`.ecxr`（切到异常上下文）、`kb`/`kp`（调用栈）、`dt <符号>`（结构体字段）、`!lmi`（模块信息）、`!heap -p`（堆损坏/泄漏）、`.sympath srv*`（微软符号服务器）、`!sym noisy`（符号加载诊断）。
4. 分析结论（异常码、栈、根因）写入 Bug 记忆条目；分析用命令本身就是「记录要求」中的可执行验证命令。

## 修复与回归

1. 使用代码图谱追踪相关调用链和数据流，确认根因后执行最小修复。
2. 为根因添加自动化回归测试，或在无法自动化时添加明确的人工检查点。
3. 按测试与验证流程运行构建、针对性测试和必要的手测。
4. 验证后更新 Bug 记忆条目的根因、关联文件、解决方案、验证结果和回归防护；状态只可在证据支持时推进。状态/验证变化用 `memory_memory_update` 或按 `conversation_id=bug:<BUG-ID>` 增量写，可复用的新根因模式记 `memory_mistake_note_add`。

## 记录要求

- Bug 记忆条目必须保留至少一个可执行验证命令或可复现的手测结果。
- 诊断日志和截图只记录可定位的结论，不把原始大日志写入记忆。
- 若修复改变合同、计划或风险等级，同步更新相应 Track 规格、计划和验证文档。

## 缺陷记录与检索（memory）

`memory` 工具（`memory_memory_store` / `memory_memory_search` / `memory_mistake_note_add` / `memory_mistake_note_search`）是缺陷记录的权威存储与唯一检索入口；`conductor/bug_registry.md` 已于 2026-08-09 封存，仅作历史回溯。`BUG-YYYYMMDD-NNN` 编号、严重级别、状态流转约定保持不变。

### 完整 Bug 记录（memory_memory_store）

- `memory_type=error`；`conversation_id=bug:<BUG-ID>`（每个 bug 独立会话 id，同一 id 增量更新可绕过语义去重）。
- 标签：`bug` + `<BUG-ID>` + 严重度 `p0`/`p1`/`p2`/`p3` + 状态 `st-open`/`st-inprogress`/`st-resolved`/`st-verified`/`st-closed` + 模块（`render`/`ui`/`combat`/`ai`/`vfx`/`skill`/`build`/`perf`/`save`/`test` 等）。
- 正文模板（只写可检索结论，不写原始日志）：
  ```
  [Bug] <症状一句话>
  状态: <status>（更新 YYYY-MM-DD）
  触发: <触发条件/复现路径>
  根因: <根因>
  修复: <修复要点>
  验证: <验证命令 + 结果>
  回归防护: <自动化测试/检查点>
  关联: <文件路径>
  ```

### 错误模式复盘（memory_mistake_note_add）

修复中发现可复用的根因模式时记一条 mistake note；多个 bug 同根因时复用同一 `error_pattern`（系统自动累计 `failure_count`）。`error_pattern` 写可复用的一句话模式，`context_signature` 写模块/场景，另记错误与正确做法。开工前必须 `memory_mistake_note_search(<任务上下文>)`，命中已知坑时先对照处理。
