# Track 1: Render Constants 实施计划 (V1.0)

> **Track ID**: `T1_render_constants`
> **依赖 Spec**: `spec.md` (V1.0)
> **预计工时**: 4h

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|---|---|---|---|
| **Phase 1** | 常量定义 | `RenderConstants.hpp` | 🟢 已完成 |
| **Phase 2** | C++ 重构 | 替换所有 C++ 端绑定字面量 | 🟢 已完成 |
| **Phase 3** | Shader 审计 | 添加绑定来源注释 | 🟢 已完成 |
| **Phase 4** | 验证 | 编译测试、视觉验收 | 🟢 已完成 |

---

## Phase 1: 常量定义 (Constant Definition)

### Task 1.1: 创建 RenderConstants.hpp
- [x] 在 `src/engine/render/` 创建 `RenderConstants.hpp`
- [x] 定义 `Binding` 枚举 (0-15)
- [x] 定义 `UBOBinding` 枚举
- [x] 定义 `TextureUnit` 枚举
- [x] 定义 `Barrier` 枚举及 `operator|`
- [x] 定义 `GPU` 命名空间常量 (`MAX_ENTITIES` 等)

**验证**: 文件编译通过，无语法错误。

---

## Phase 2: C++ 重构 (C++ Refactor)

### Task 2.1: 重构 MDIRenderer.cpp
- [x] 添加 `#include "engine/render/RenderConstants.hpp"`
- [x] 替换 `BindPreviousNoSync(0)` → `static_cast<uint32_t>(Binding::SSBO_ENTITY_DATA)`
- [x] 替换 `BindPreviousNoSync(1)` → `Binding::SSBO_VISIBLE_ID`
- [x] 替换 `BindBase(2)` (Command Buffer) → `Binding::SSBO_COMMAND`
- [x] 替换 `BindPreviousNoSync(3)` → `Binding::SSBO_VISUAL_STATS`
- [x] 替换 `glMemoryBarrier(...)` 调用为 `GPUUtils::MemoryBarrier(ToGL(Barrier::All))`

**验证**: `Select-String -Path 'MDIRenderer.cpp' -Pattern 'Bind.*\([0-3]\)'` 返回空。

### Task 2.2: 重构 GPUEntitySystem.cpp
- [x] 添加 `#include "engine/render/RenderConstants.hpp"`
- [x] 替换 `m_persistentEntityBuffer.BindPreviousNoSync(0)` → `Binding::SSBO_ENTITY_DATA`
- [x] 替换 `m_persistentEntityBuffer.BindPrevious(1)` (Legacy Render)
- [x] 审计其他 `BindBase` 调用

**验证**: 编译通过。

### Task 2.3: 重构 GPUParticleSystem.cpp
- [x] 替换粒子相关绑定 (4, 5, 6) → `Binding::SSBO_PARTICLES`, `SSBO_PARTICLE_FREE`, `SSBO_PARTICLE_COUNT`

### Task 2.4: 重构 GPUFlowFieldSystem.cpp
- [x] 替换流场相关绑定 (7, 8, 9) → `Binding::SSBO_FLOW_*`

### Task 2.5: 重构 GPUSkillEffectSystem.cpp
- [x] 替换技能特效绑定 → `Binding::SSBO_SKILL_EFFECTS`

### Task 2.6: 重构 PopupRenderer.cpp
- [x] 替换弹出数字绑定 → `Binding::SSBO_POPUP_*`

---

## Phase 3: Shader 审计 (Shader Audit)

### Task 3.1: 审计 Entity Rendering Shaders
- [x] `cull.compute`: 添加绑定来源注释
- [x] `entity_mdi.vert`: 添加绑定来源注释
- [x] `entity_mdi.frag`: 检查是否有绑定 (通常无)

### Task 3.2: 审计 Particle Shaders
- [x] `particle_emit.compute`
- [x] `particle_update.compute`
- [x] `particle_render.vert` / `.frag`

### Task 3.3: 审计 Flow Field Shaders
- [x] `flow_reset.compute`
- [x] `flow_integration.compute`
- [x] `flow_vector.compute`

### Task 3.4: 审计 Skill Effect Shaders
- [x] `skill_effect.vert` / `.frag`

### Task 3.5: 创建绑定索引对照表
- [x] 在 `assets/shaders/README.md` (或新建 `BINDINGS.md`) 记录 C++/GLSL 对照

---

## Phase 4: 验证 (Verification)

### Task 4.1: 编译验证
- [x] 执行 `.\build.bat`
- [x] 确保无编译错误、无新增警告

### Task 4.2: 自动化搜索验证
- [x] 执行: `Select-String -Path 'src/**/*.cpp' -Pattern 'Bind(Base|Previous)\([0-9]+\)' -Recurse`
- [x] 确认结果为空 (或仅剩非渲染相关调用)

### Task 4.3: 单元测试
- [x] 运行 `.\build\bin\Release\NoMoreDayTests.exe`
- [x] 确保所有测试通过

### Task 4.4: 视觉验收
- [x] 运行游戏，进入战斗场景
- [x] 验证实体渲染、粒子特效、技能效果、伤害数字显示正常
- [x] (可选) 使用 RenderDoc 捕获帧，检查 Buffer Binding 正确性

### Task 4.5: 提交与标记
- [x] Git Commit: `refactor(render): introduce RenderConstants for type-safe bindings`
- [x] Git Tag: `render_refactor_T1_complete`

---

## 任务依赖图

```
Task 1.1 ─┬─ Task 2.1 ─┬─ Task 4.1
          ├─ Task 2.2 ─┤
          ├─ Task 2.3 ─┤
          ├─ Task 2.4 ─┤
          ├─ Task 2.5 ─┤
          ├─ Task 2.6 ─┼─ Task 4.2
          │            │
          └─ Task 3.* ─┴─ Task 4.3 ─ Task 4.4 ─ Task 4.5
```

---

*计划版本: 1.0*
*最后更新: 2026-01-26*