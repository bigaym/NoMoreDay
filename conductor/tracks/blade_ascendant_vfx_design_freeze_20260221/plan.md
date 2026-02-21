# Blade Ascendant VFX Design Freeze - 执行计划

> **Track ID**: `blade_ascendant_vfx_design_freeze_20260221`

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|---|---|---|---|
| Phase 1 | Context对齐 | 输入约束矩阵与代码接口盘点 | [ ] |
| Phase 2 | 规格冻结 | V2 文档与三份 evidence 矩阵 | [ ] |
| Phase 3 | Track联动 | T2/T3 引用与依赖链同步 | [ ] |
| Phase 4 | 冻结验收 | UTF-8、一致性、自检清单 | [ ] |

---

## Phase 1: Context对齐（4 Tasks）

- [ ] Task 1.1: 从 `职业设计草案_剑修.md` 提取 3.1-3.9 的技能-机制要点
- [ ] Task 1.2: 从 `GPU_Rendering_Quick_Reference.md` 提取硬约束（SSBO/FBO/Pass/Tier）
- [ ] Task 1.3: 盘点现有代码接口（SkillSystem/GPUSkillEffectSystem/VFXPass/DistortionPass）
- [ ] Task 1.4: 产出 `evidence/skill_vfx_matrix.md` 初稿

## Phase 2: 规格冻结（7 Tasks）

- [ ] Task 2.1: 冻结 `SkillVfxEvent` 字段与触发时机
- [ ] Task 2.2: 完成 9 技能特效条目（主特效/触发特效）
- [ ] Task 2.3: 为 9 技能补全并发上限
- [ ] Task 2.4: 为 9 技能补全 Low 回退
- [ ] Task 2.5: 冻结 RenderGraph 合同段（owner/resource/禁止项）
- [ ] Task 2.6: 冻结预算与降级顺序
- [ ] Task 2.7: 产出 `evidence/render_contract_matrix.md` 与 `evidence/tier_fallback_matrix.md`

## Phase 3: Track联动（4 Tasks）

- [ ] Task 3.1: 更新 `blade_ascendant_skill_rendering_integration_20260221/spec.md` 引用
- [ ] Task 3.2: 更新 `blade_ascendant_skill_rendering_integration_20260221/plan.md` 任务映射
- [ ] Task 3.3: 更新 `blade_ascendant_skill_validation_gate_20260221/spec.md` 验收依据
- [ ] Task 3.4: 更新 `conductor/tracks.md` 依赖链与任务计数

## Phase 4: 冻结验收（3 Tasks）

- [ ] Task 4.1: 执行 UTF-8 校验（本 track 相关文档）
- [ ] Task 4.2: 执行一致性自检（文档条目与 evidence 对齐）
- [ ] Task 4.3: 写入 `validation.md` 验证证据与结论

---

## 产出目录约束

- 必须存在：
  - `conductor/tracks/blade_ascendant_vfx_design_freeze_20260221/spec.md`
  - `conductor/tracks/blade_ascendant_vfx_design_freeze_20260221/plan.md`
  - `conductor/tracks/blade_ascendant_vfx_design_freeze_20260221/validation.md`
  - `conductor/tracks/blade_ascendant_vfx_design_freeze_20260221/evidence/skill_vfx_matrix.md`
  - `conductor/tracks/blade_ascendant_vfx_design_freeze_20260221/evidence/render_contract_matrix.md`
  - `conductor/tracks/blade_ascendant_vfx_design_freeze_20260221/evidence/tier_fallback_matrix.md`

