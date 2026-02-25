# AGENTS.md

NoMoreDay 开发代理规则（Windows / PowerShell）。

---

## 0. 术语与优先级

- 关键字语义：
  - `MUST` = 必须执行，不可跳过。
  - `SHOULD` = 默认执行，除非有明确理由。
  - `MAY` = 可选执行。
- 冲突优先级：
  1. 用户明确指令
  2. 本文件
  3. 其他文档

---

## 1. 执行状态机（强流程）

每次任务按以下状态顺序推进：

1. `Context`  
   - MUST：仅在**第一次会话开始**时读取 memory MCP（近期决策、进行中 track、约束）再读代码。
   - MUST NOT：同一会话后续多轮重复读取 memory MCP（除非用户明确要求或当前上下文不足）。
2. `Implement`  
   - MUST：最小化、聚焦修改；不改无关文件。
3. `Verify`  
   - MUST：若本次改动涉及编译相关文件，在规划的 track 完成后执行验证（先构建，再按需要执行 CTest）。
   - MUST：若本次改动涉及编译相关文件，提交前再执行一次验证，确保未引入新 bug。
   - MUST：若本次改动不涉及编译相关文件，MUST NOT 强制执行构建/CTest，且 MUST 在验证记录中注明跳过原因。
   - MUST NOT：在手测修改阶段逐次执行构建/CTest（除非用户明确要求）。
4. `TrackSync`  
   - MUST：更新 track 文档、验证证据、必要时更新 bug_registry。
5. `Closeout`  
   - MUST：归档（如适用）、提交、git notes、memory MCP 收尾。

禁止跨状态跳跃（除非用户明确要求）。

---

## 2. 环境与命令

- Shell MUST 为 PowerShell。
- 同行多命令 MUST 使用 `;`，禁止 `&&`。
- 构建入口 MUST 为仓库根目录 `build.bat`（MSVC-only）。
- 重定向安全：
  - MUST NOT 把 `2` 作为独立参数。
  - 若出现误产物 `./2`，MUST 直接删除。
- 关键日志路径：
  - 运行日志：`bin/logs/NoMoreDay.log`
  - 构建输出：`build.bat` 控制台

---

## 3. 文本与编码规则

- `.md/.cpp/.hpp` 编辑 MUST 使用 `apply_patch`（优先）。
- MUST NOT 用 `Set-Content` / `Out-File` 重写含非 ASCII 文件。
- 文本文件 MUST 为 UTF-8；中文文档 SHOULD 使用 UTF-8 with BOM。
- 读取文本文件时 MUST 显式使用 UTF-8 编码（如 `Get-Content -Encoding utf8`、`Path(...).read_text(encoding='utf-8')`）。
- MUST NOT 采用“先按默认编码读取，失败后再回退 UTF-8”的策略。
- 每次编辑后 MUST 做 UTF-8 校验：
  - `python -c "from pathlib import Path; Path('file').read_text(encoding='utf-8')"`
- 乱码排查顺序 MUST 为：
  1. VS Code `Reopen with Encoding -> UTF-8`
  2. `.vscode/settings.json`
     - `files.encoding = utf8`
     - `files.autoGuessEncoding = false`

---

## 4. 构建与测试规则（已解耦）

### 4.0 执行时机（必须）

- 手测修改阶段：MUST NOT 强制执行构建/CTest。
- 编译相关改动判定（命中任一即视为“涉及编译相关文件”）：
  - 任何 C/C++ 源码或头文件（如 `.cpp/.hpp/.c/.cc/.h/.inl`）
  - 构建系统与构建脚本（如 `CMakeLists.txt`、`*.cmake`、`build.bat`）
  - 会影响编译/链接行为的工程配置
- Track 计划项完成后：若涉及编译相关文件，MUST 执行一次完整验证（`build.bat` + 按需 CTest）。
- 提交前：若涉及编译相关文件，MUST 再执行一次验证，作为最终回归检查。
- 若不涉及编译相关文件：MUST NOT 强制执行构建/CTest；MUST 在验证文档中记录 `Skip Build/CTest: no C++/build changes`。

### 4.1 构建

- 默认构建：`build.bat`
- 快速仅编译：`build.bat notest`
- 静态分析：`build.bat analyze`
- ASan：`build.bat asan`
- 依赖分析：`build.bat includes`

### 4.2 测试（统一 CTest 管理）

- 性能测试 MUST 通过 CTest 执行，禁止 `build.bat perf`。
- 常用命令（MSVC 多配置 MUST 带 `-C`）：
  - `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`
  - `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
  - `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
  - `ctest --test-dir build -C Release -L performance --output-on-failure`

### 4.3 缓存与构建速度

- 构建缓存策略（已内置）：
  - 先从 `PATH` 查找 `sccache/clcache/ccache`
  - 失败后回退到预设 `ccache` 路径
  - 再失败则跳过缓存，不中断构建
- 可用参数：
  - `nocache`
  - `cache=auto|sccache|ccache|clcache`
  - `nofastbuild`
  - `noruntimeopt`
  - `j=N`

---

## 5. 失败处理规则

- 构建/测试失败时 MUST 先修失败项，再继续。
- 不允许带失败证据进入“完成”状态。
- 性能失败若与当前任务无关，MUST：
  1. 明确标注“非本任务阻塞”
  2. 在验证文档中写清失败用例和数值
  3. 在 bug_registry 或既有风险项中挂接

---

## 6. Bug 追踪规则

- 统一登记：`conductor/bug_registry.md`
- 新增前 MUST 查重（症状/根因/路径相同视为同类）。
- Bug ID 格式 MUST 为：`BUG-YYYYMMDD-XXX`
- 以下场景 MUST 建档：
  - 运行时崩溃
  - 可见渲染异常
  - 最近改动引入回归
- 状态流转 MUST 为：
  - `Open -> In Progress -> Resolved -> Verified -> Closed`
- 无验证证据 MUST NOT 标记 `Closed`。

---

## 7. 渲染管线安全约束

- MUST 明确输出目标归属（默认 framebuffer vs 离屏 framebuffer）。
- 除最终屏幕合成外，MUST NOT 硬编码输出到 FBO 0。
- 帧序 MUST 保持：
  - `Input -> Player Movement -> AI -> Combat -> Spatial Grid Rebuild -> Physics`
- Low Tier 回退路径 MUST 可用。
- Resize 时 framebuffer 资源 MUST 安全重建/重设。

### 7.1 GPU 重构后渲染引导（必须）

- 权威参考文档：`设计文档/特效和UI/GPU_Rendering_Quick_Reference.md`
- MUST 以 RenderGraph 为唯一渲染编排入口（`Build/Execute/Validate`）。
- MUST 保持契约一致：`GPU_ABI_VERSION = 5`、`RENDERGRAPH_CONTRACT_VERSION = 3`。
- MUST 遵循图形 API 基线：OpenGL 4.3+（MSVC-only, Windows）。
- MUST 维持帧序约束：`Input -> Player Movement -> AI -> Combat -> Spatial Grid -> Physics -> Render`。
- MUST 遵循输出约束：仅 `CompositePass` 可写 `BackBuffer(FBO 0)`，其余 pass 写离屏目标。
- MUST 注意 SSBO 绑定预算：全局 0-15 已占满，新增数据优先使用 Compute 本地 binding 或时间片复用。

---

## 8. 架构与平台约束

- 架构 MUST 为 EnTT ECS；Gameplay 与 Render 子系统分离。
- 关键渲染模块（修改时 SHOULD 优先审查）：
  - `RenderSystem`
  - `GPUEntitySystem`
  - `MDIRenderer`
  - `PostProcessPass`
  - `FramebufferManager`
- Windows 约束：
  - MUST 定义 `WIN32_LEAN_AND_MEAN` 与 `NOMINMAX`
  - MUST 处理 `DrawText`（WinAPI vs raylib）冲突
  - MSVC 为主

---

## 9. Git 纪律

- MUST 最小化改动范围。
- MUST NOT 回退无关用户改动。
- MUST NOT 使用破坏性 git 命令（除非用户明确要求）。
- 若发现意外无关改动，MUST 立即停止并询问用户。

---

## 10. Track 收尾（强制）

Track 完成时 MUST 按顺序执行：

1. 编译与测试检查（按变更类型）  
   - 若涉及编译相关文件：  
     - `build.bat`  
     - `build.bat analyze`（如适用）  
     - `ctest --test-dir build -C Release -L performance --output-on-failure`（如适用）
   - 若不涉及编译相关文件：  
     - MUST 记录 `Skip Build/CTest: no C++/build changes`
2. 文档与追踪同步  
   - 更新 `plan.md` / `validation.md` / `metadata.json` / `tracks.md`
3. 归档 track（命令行 `move`）  
   - `move conductor\tracks\<track_id> conductor\archive\`
4. 提交与记录  
   - 提交代码+文档  
   - `git notes add -m "<detail>"`
5. memory MCP 收尾  
   - 清理干扰记忆  
   - 写入关键决策、证据、风险、后续项

---

## 11. 决策分歧提问模板（必须）

当存在多方案或架构风险时，必须按以下格式向用户提问：

1. 背景说明：问题/冲突/风险
2. 选项列表（至少 2 个）  
   - 选项 A：优点 / 缺点  
   - 选项 B：优点 / 缺点
3. 推荐方案（Recommended）+ 依据（本文件或 code_standard/perf 基准）
4. 决策影响（兼容性/性能/工作量/风险）

---

维护原则：短句、可执行、可检查、无歧义、机器优先。
