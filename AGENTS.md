# NoMoreDay Project Rules

## Priority

- Use Chinese thinking methods for all tasks.
- Instruction precedence: user request > `AGENTS.md` > nearest repository document (`conductor/*`, `README.md`) > memory. Prefer the more specific and stricter rule.
- Repository files are authoritative; memory is supporting context only.

## Language And Encoding

- Treat repository text as UTF-8. Chinese design documents are under `设计文档/` and `conductor/`; verify terminal encoding before treating garbled text as corruption.
- Use UTF-8-aware PowerShell commands when reading or writing Chinese text.
- Keep source code, identifiers, and log strings in English; localize user-facing text through data.

## Workspace Boundaries

- Do not create worktrees or commits unless the user explicitly requests them.
- Reuse established decisions, terminology, hierarchy, and scope; do not silently rename or relocate documents, tracks, or layers.
- Prioritize engineering best practices when making changes; do not modify unrelated content.
## Repository Map

- `src/`: application, engine, and gameplay code.
- `tests/`: automated test suites.
- `scripts/`, `tools/`: project automation and tools.
- `assets/`: game data and rendering assets.
- `conductor/`: process standards, specifications, tracks, and archives.
- `设计文档/`: Chinese game and technical design documents.
- `docs/`: workflows and project outputs.
- `third_party/`: vendored dependencies.
- `bin/`, `build/`: generated build outputs.

## Workflows

- Design: load `docs/workflows/design.md`; produce a verifiable `*-design.md` (or Track `spec.md`).
- Planning: load `docs/workflows/planning.md`; produce a `*-plan.md` with implementation rationale, pseudocode, tests, and completion criteria, not full code.
- Implementation: load `docs/workflows/implementation.md`; complete atomic tasks and apply its code rules as needed.
- Testing: load `docs/workflows/testing.md`; choose adequate coverage and retain evidence.
- Debugging: load `docs/workflows/debugging.md`; document, reproduce, fix, and prevent regression.
- Review: load `docs/workflows/review.md`; conclude `提交` or `修改` against design and plan, and write reports under `docs/reviews/`.
- Rendering/GPU: also load `conductor/specs/rendering_engine_v5_master_spec.md` and `conductor/rendering_system_progress.md`.
- New dependencies, frameworks, or tools: also load `conductor/tech-stack.md` and follow the design workflow.
- New features follow design -> planning -> implementation -> testing. Bugs follow debugging -> implementation -> testing. Documentation changes use the design or planning workflow as appropriate and check links and indexes.
- See `conductor/code_standard.md` and `conductor/code_styleguides/` for detailed code standards.

## Agent Operations

### Tools

- Prefer `codebase-memory-mcp` graph tools for code definitions, call chains, data flow, and impact analysis; use `glob` or `grep` for configuration, scripts, text, or graph gaps.
- Use the terminal only when no specialized tool fits. For compilation, testing, or other high-output commands, do not emit full output to context; filter it or redirect it to a text file.

### Memory, Context, And Evidence

- If `memory_*` tools are unavailable, report the limitation and stop work that requires memory guarantees. Query relevant memory before planning, editing, deciding, or debugging; on resumed work, query first and treat a miss as new work.
- Record key decisions, direction changes, implementation checkpoints, verification, and commits with the change, verification result, and current risk or blocker. Never store secrets, credentials, raw logs, or unverified speculation; update memory for new defect types.
- Load only task-relevant references. Support conclusions with command output, tests, specifications, or manual checks; identify anything unverified as a risk or follow-up.
