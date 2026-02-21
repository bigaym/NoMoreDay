# Blade Ascendant Skill Rendering Integration - 规格说明书

> **Track ID**: `blade_ascendant_skill_rendering_integration_20260221`  
> **状态**: Pending  
> **依赖**: `blade_ascendant_vfx_design_freeze_20260221`  
> **设计输入**: `设计文档/特效和UI/BladeAscendant_SkillVFX_Design_v2.md`

---

## 1. 目标

将 V2 特效设计文档落实到现有渲染引擎，实现“可执行事件契约 + RenderGraph 合同接入 + Tier 回退”。

## 2. Data Model（渲染侧）

```cpp
enum class SkillVfxEventType : uint8_t {
  CastStart,
  CastImpact,
  TriggerProc,
  EmpoweredConsume,
  BuffEnter,
  BuffExit
};

struct SkillVfxEvent {
  uint32_t skillId;
  uint64_t castId;
  SkillVfxEventType type;
  Vector2 origin;
  Vector2 target;
  Tag effectiveTags;
  uint32_t nodeRoleMask;
  uint8_t qualityTier;
  float intensity;
};
```

- 事件来源：`SkillSystem`
- 事件消费：`GPUSkillEffectSystem` / `SwordIntentVisualSystem`
- 事件应保持“无判定副作用”（只负责视觉）

## 3. RenderGraph 合同

- `VFXPass`
  - Read: `SceneDepth`（可选）
  - Write: `SceneHdrColor`
  - Owner: `VFX`
- `DistortionPass`
  - Read: `PostProcessLdrColor`
  - Write: `DistortionLdrColor`
  - Owner: `Distortion`

禁止项：

- 禁止技能特效路径直接写 `FBO 0`
- 禁止绕开 RenderGraph 手工插队渲染

## 4. GPU 资源策略

- 全局 SSBO binding 不新增（0-15 已满）
- 优先复用：
  - `SSBO_SKILL_EFFECTS`（binding 6）
  - `GPUParticle` / `GPUBeamInstance`
- 若需要额外数据：
  - 先复用字段
  - 再考虑 ABI 升级（并同步生成链与版本）

## 5. Quality Tier & Fallback

- `Low`：保留关键判读特效，禁用扭曲和高密粒子
- `Medium`：基础 GPU 特效，限制并发
- `High`：完整技能特效
- `Ultra`：高质量轨迹与后处理

降级顺序（必须一致）：

1. 降粒子发射率  
2. 关 Distortion  
3. 降 trail 采样  
4. 关次级发光层  

## 6. 验收标准

- [ ] 9 技能特效按 V2 文档落实（每技能至少主特效 + 1 关键反馈）
- [ ] RenderGraph ownership/contract 验证通过
- [ ] Low/Medium 回退下技能判读可用
- [ ] VFX 预算满足：常规 `<=0.80ms`，高压 `<=1.10ms`
- [ ] `build.bat` 编译通过（按用户指令可延后执行）

