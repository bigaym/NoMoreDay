# Phase 1: GPU-Driven MDI Rendering 实施计划

**Track ID**: `performance_optimization/phase1_mdi_rendering`  
**状态**: ✅ Completed  
**预计工时**: 3-4 天

---

## 任务分解 (Task Breakdown)

### Task 1.1: 创建 MDIRenderer 核心类 ✅
### Task 1.2: 实现 GPU-Side DrawIndirect Command Buffer ✅
### Task 1.3: 编写 cull.compute Shader ✅
### Task 1.4: 编写 entity_mdi.vert/frag Shader ✅
### Task 1.5: 集成到 GPUEntitySystem ✅
### Task 1.6: 创建集成测试 ✅
### Task 1.7: 性能 Benchmark ✅

---

## 依赖关系

```
Task 1.1 
    │
    ├──► Task 1.2
    │        │
    │        ▼
    │    Task 1.3 ──► Task 1.5
    │        
    └──► Task 1.4 ──► Task 1.5 ──► Task 1.6 ──► Task 1.7
```

---

## 验收清单

- [ ] `glMultiDrawArraysIndirect` 成功调用
- [ ] 视觉输出与 Legacy 模式一致
- [ ] RenderDoc 确认 Draw Call = 1
- [ ] 无 OpenGL 错误 (Validation Layer Clean)
- [ ] 10k 实体场景 CPU Draw 开销 < 0.5ms

---

## 回滚计划

若遇到严重问题:
1. 将 `Config::useMDI` 设为 false
2. `GPUEntitySystem::Render()` 自动回退到 `RenderLegacy()`
3. MDI 代码保留，待问题修复后重新启用

---

*规划者: Gemini (Skill: designer)*
