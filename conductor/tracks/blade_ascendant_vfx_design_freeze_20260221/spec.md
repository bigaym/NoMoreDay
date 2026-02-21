# Blade Ascendant VFX Design Freeze - 规格说明书

> **Track ID**: `blade_ascendant_vfx_design_freeze_20260221`  
> **状态**: Pending

---

## 1. 目标

冻结剑修技能特效设计输入，产出“可执行渲染合同包”，作为 T2（渲染集成）和 T3（验证门禁）的唯一上游规范。

## 2. 输入与输出

### 2.1 输入

- `设计文档/职业设计草案_剑修.md`
- `设计文档/特效和UI/GPU_Rendering_Quick_Reference.md`
- 现状代码接口：
  - `src/game/systems/skill/SkillSystem.cpp`
  - `src/engine/render/GPUSkillEffectSystem.cpp`
  - `src/engine/render/passes/VFXPass.cpp`
  - `src/engine/render/passes/DistortionPass.cpp`

### 2.2 输出（冻结包）

- 主规范文档：
  - `设计文档/特效和UI/BladeAscendant_SkillVFX_Design_v2.md`
- 轨道资产：
  - `conductor/tracks/blade_ascendant_vfx_design_freeze_20260221/spec.md`
  - `conductor/tracks/blade_ascendant_vfx_design_freeze_20260221/plan.md`
  - `conductor/tracks/blade_ascendant_vfx_design_freeze_20260221/validation.md`
- 设计证据（本 track 目录下）：
  - `evidence/skill_vfx_matrix.md`（9技能特效矩阵）
  - `evidence/render_contract_matrix.md`（pass/resource/owner矩阵）
  - `evidence/tier_fallback_matrix.md`（Low/Med/High/Ultra 回退矩阵）

## 3. 冻结范围

- 9 技能主特效与触发反馈（每技能定义并发上限 + Low 回退）
- `SkillVfxEvent` 契约字段与生成时机
- RenderGraph 接入合同（owner/resource/禁止项）
- 特效预算和降级顺序
- 资产清单（复用/新增）

## 4. 不在范围

- 具体 shader/代码实现
- ABI 版本升级实施（仅给出触发条件）
- 性能压测执行（由 T3 执行）

## 5. 设计质量门槛

- 一致性：技能逻辑事件与特效触发一一对应
- 可实现性：不违反 SSBO/FBO/帧序约束
- 可验收性：每条要求都能映射到测试或证据
- 可回退性：Low/Medium 仍可判读技能关键反馈

## 6. 验收标准（DoD）

- [ ] V2 文档包含 9 技能完整条目，且每条含并发上限与 Low 回退
- [ ] V2 文档包含 `SkillVfxEvent` 契约与触发时机表
- [ ] V2 文档包含 RenderGraph 禁止项（尤其 FBO0）
- [ ] V2 文档包含预算阈值与降级顺序
- [ ] `evidence/*.md` 三份矩阵文档齐备
- [ ] T2/T3 的 spec 明确引用 V2 文档
- [ ] 全部文档 UTF-8 校验通过

