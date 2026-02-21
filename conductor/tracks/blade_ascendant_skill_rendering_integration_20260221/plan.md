# Blade Ascendant Skill Rendering Integration - 执行计划

> **Track ID**: `blade_ascendant_skill_rendering_integration_20260221`  
> **规范来源**: `设计文档/特效和UI/BladeAscendant_SkillVFX_Design_v2.md`

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|---|---|---|---|
| Phase 1 | 事件契约接入 | `SkillVfxEvent` 端到端联通 | [ ] |
| Phase 2 | 9技能视觉落地 | 分技能主特效与关键反馈 | [ ] |
| Phase 3 | RenderGraph 合同收敛 | owner/resource/FBO 合同验证 | [ ] |
| Phase 4 | Tier回退与预算 | 回退矩阵 + 预算门限验证 | [ ] |

---

## Phase 1: 事件契约接入

- [ ] Task 1.1: `SkillSystem` 在关键节点发 `SkillVfxEvent`
- [ ] Task 1.2: `GPUSkillEffectSystem` 消费并缓存事件
- [ ] Task 1.3: `SwordIntentVisualSystem` 接入 `EmpoweredConsume/Buff*`
- [ ] Task 1.4: 事件缺失兜底（静默降级）

## Phase 2: 9技能视觉落地

- [ ] Task 2.1: 实现 3.1/3.2（流云刺、裂空斩）
- [ ] Task 2.2: 实现 3.3/3.4（灵剑决、剑气护体）
- [ ] Task 2.3: 实现 3.5/3.6（万剑归宗、剑阵）
- [ ] Task 2.4: 实现 3.7/3.8/3.9（心剑、回旋、绝影）
- [ ] Task 2.5: 每技能并发上限参数化

## Phase 3: RenderGraph 合同收敛

- [ ] Task 3.1: `VFXPass` Read/Write/Owner 声明校核
- [ ] Task 3.2: `DistortionPass` 读写关系校核
- [ ] Task 3.3: FBO0 非法写路径回归检查
- [ ] Task 3.4: resize 重建链路回归

## Phase 4: Tier回退与预算

- [ ] Task 4.1: 落地 Low/Medium 回退策略
- [ ] Task 4.2: 高压场景预算采样（VFXPass + DistortionPass）
- [ ] Task 4.3: 产出截图与预算证据
- [ ] Task 4.4: 更新 validation 文档与配置说明

