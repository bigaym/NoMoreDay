# AGENTS.md

NoMoreDay 开发代理规则（Windows）。

## 1) 环境与命令
- Shell: Windows PowerShell。
- 同一行多命令使用 `;`，不要用 `&&`。
- 重定向安全：避免把 `2` 作为独立参数传给命令；错误重定向可能在仓库根目录误生成名为 `2` 的文件/目录。若出现 `./2`，按误产物处理并直接删除。
- 构建入口固定为仓库根目录 `build.bat`（MSVC 环境）。
- 关键日志：
  - 运行日志：`bin/logs/NoMoreDay.log`
  - 构建输出：`build.bat` 控制台

## 2) 编码规则（强制）
- 文本/Markdown 一律 UTF-8；中文文档优先 UTF-8 with BOM。
- 常规编辑优先 `apply_patch`。
- 禁止用 `Set-Content`/`Out-File` 重写含非 ASCII 的 `.md/.cpp/.hpp`。
- 编辑后必须做 UTF-8 校验：
  - `python -c "from pathlib import Path; Path('file').read_text(encoding='utf-8')"`
- 乱码排查顺序：
  1. VS Code: `Reopen with Encoding -> UTF-8`
  2. 检查 `.vscode/settings.json`：
     - `files.encoding = utf8`
     - `files.autoGuessEncoding = false`
- 认知约束：乱码通常源于错误解码/重编码链路（ACP/GBK/终端码页），非 BOM 本身。

## 3) 构建与验证流程
1. 运行 `build.bat`。
2. 若失败，先修编译错误，再继续。
3. 直到 `build.bat` 成功。
4. 启动游戏进行运行验证。
5. 渲染问题必须同时保留：
   - 屏幕表现证据
   - `bin/logs/NoMoreDay.log` 证据

## 4) Bug 追踪规则
- 统一登记：`conductor/bug_registry.md`。
- 新增前先查重（症状/根因/路径相同视为同类）。
- Bug ID 规范：`BUG-YYYYMMDD-XXX`。
- 以下场景必须建档：
  - 运行时崩溃
  - 可见渲染异常
  - 最近改动引入回归
- 每条记录至少包含：
  - ID/日期/严重级别/状态
  - 症状/复现步骤/触发条件
  - 根因/关联文件/解决方案
  - 验证结果/回归防护/备注
- 状态流转：`Open -> In Progress -> Resolved -> Verified -> Closed`。
- 无验证证据不得标记 `Closed`。
- 修复后必须同步更新：代码 + `conductor/bug_registry.md`。

## 5) 渲染管线安全约束
- 明确场景目标归属（默认 framebuffer vs 离屏 framebuffer）。
- 除最终屏幕合成外，不得硬编码输出到 FBO 0。
- 保持帧序：`Input -> Player Movement -> AI -> Combat -> Spatial Grid Rebuild -> Physics`。
- Low Tier 回退路径必须可用（Phase 0 兼容）。
- Resize 必须安全重建/重设 framebuffer 资源。

## 6) 架构与平台约束
- 架构：EnTT ECS；Gameplay 与 Render 子系统分离。
- 关键渲染模块：
  - `RenderSystem`
  - `GPUEntitySystem`
  - `MDIRenderer`
  - `PostProcessPass`
  - `FramebufferManager`
- Windows 约束：
  - 定义 `WIN32_LEAN_AND_MEAN` 与 `NOMINMAX`
  - 处理 `DrawText`（WinAPI vs raylib）冲突
  - 以 MSVC 为主（可存在 MinGW 配置）

## 7) Git 与变更纪律
- 最小化、聚焦修改。
- 不回退与当前任务无关的用户改动。
- 未经明确要求，禁止破坏性 git 命令。
- 若发现意外无关改动，先停下并询问用户。

## 8) 完成定义（DoD）
- 代码已落地。
- `build.bat` 成功。
- 运行行为已验证（如适用）。
- 已清晰汇报结果与变更文件。

## 9) 常用命令
- Build: `build.bat`
- Tail log: `Get-Content bin/logs/NoMoreDay.log -Tail 200`
- Search: `rg "pattern" src`

---

维护原则：可执行、可检查、少歧义；新增规则优先短句和清单化。
