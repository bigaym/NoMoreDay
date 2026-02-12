# AGENTS.md

Guidance for coding agents working in `NoMoreDay` on Windows.

## 1) Environment Baseline (Windows)
- OS/Shell: Windows + PowerShell.
- Build entry: always use `build.bat` from repo root.
- Compiler: MSVC (Visual Studio environment initialized by `build.bat`).
- Primary logs:
  - Runtime log: `bin/logs/NoMoreDay.log`
  - Build output: console output from `build.bat`

## 2) Encoding Rules (Critical)
- All markdown/text docs must be UTF-8.
- Prefer UTF-8 with BOM for Chinese-heavy docs to avoid VS Code mis-detection.
- If garbled text appears:
  1. In VS Code: `Reopen with Encoding -> UTF-8`
  2. Ensure workspace has `.vscode/settings.json` with:
     - `files.encoding = utf8`
     - `files.autoGuessEncoding = false`

## 3) Build & Verify Workflow
1. Run: `build.bat`
2. If build fails, fix compile errors first; do not continue with runtime assumptions.
3. Re-run: `build.bat` until success.
4. Then launch game and validate behavior in gameplay.
5. For rendering bugs, always collect evidence from both:
   - on-screen behavior
   - `bin/logs/NoMoreDay.log`

## 4) Render Pipeline Safety Checks
When touching render code, verify these invariants:
- Scene target ownership is explicit (default framebuffer vs offscreen framebuffer).
- No pass should hardcode output to FBO 0 unless it is truly final screen composite.
- Preserve frame order:
  - Input -> Player Movement -> AI -> Combat -> Spatial Grid Rebuild -> Physics
- Keep Low Tier fallback path working (Phase 0-compatible behavior).
- Resize path must recreate/resize framebuffer resources safely.

## 5) Project Architecture Notes
- ECS architecture via EnTT.
- Systems are split across gameplay and render subsystems.
- GPU-focused rendering includes:
  - `RenderSystem`
  - `GPUEntitySystem`
  - `MDIRenderer`
  - `PostProcessPass`
  - `FramebufferManager`

## 6) Platform-Specific Constraints
- Define `WIN32_LEAN_AND_MEAN` and `NOMINMAX` for Windows conflicts.
- Windows API `DrawText` can conflict with raylib `DrawText`; keep existing macro strategy.
- MinGW-specific options may exist, but MSVC is primary in this workspace.

## 7) Git & Change Discipline
- Make minimal, focused edits.
- Do not revert unrelated user changes.
- Do not use destructive git commands unless explicitly requested.
- If unexpected unrelated changes appear during work, stop and ask before proceeding.

## 8) Definition of Done for Agent Tasks
A task is done only when all are true:
- Code edits applied.
- `build.bat` succeeds.
- Runtime behavior validated (if applicable).
- Key result and changed files reported clearly.

## 9) Quick Commands
- Build: `build.bat`
- Tail runtime log: `Get-Content bin/logs/NoMoreDay.log -Tail 200`
- Search symbols/text: `rg "pattern" src`

---

Maintainers can extend this file with track-specific rules, but keep this document concise and executable.
