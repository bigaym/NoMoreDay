# 剑修技能特效设计文档 V2（渲染可执行版）

> 文档ID: `blade_ascendant_skill_vfx_design_v2_20260221`  
> 日期: 2026-02-21  
> 关联系统: `SkillSystem` / `GPUSkillEffectSystem` / `VFXPass` / `RenderGraph`  
> 依赖约束: `GPU_Rendering_Quick_Reference.md`

---

## 1. 目标与边界

### 1.1 目标

- 为剑修 9 个主动技能提供可实现、可验收、可回退的特效规格。
- 规格直接用于 Conductor tracks，不在实现阶段临时补协议。
- 保持现有渲染引擎硬约束：
  - 非 `CompositePass` 禁止写 `FBO 0`
  - 全局 SSBO binding `0-15` 已满，不新增全局 binding
  - 维持帧序：`Input -> Player Movement -> AI -> Combat -> Spatial Grid Rebuild -> Physics -> Render`

### 1.2 非目标

- 不在本次引入新的全局渲染子系统。
- 不引入与技能逻辑强耦合的“特效判定”逻辑（特效仅消费事件，不反向决定战斗结果）。

---

## 2. 视觉方向（Style Direction）

- 主风格：`灵动水墨 + 剑气切线`
- 颜色主轴：
  - 主能量：`#9FE8FF`（青白）
  - 高光：`#FFFFFF`
  - 阴影：`#0E1318`
- 形态关键词：
  - 快速切线、残影、旋转条带、压缩后爆发
- 镜头反馈：
  - 只允许短脉冲（`<= 120ms`）屏幕抖动
  - 禁止持续性强晕眩闪烁

---

## 3. 技能特效事件契约

### 3.1 统一事件结构

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
  uint32_t nodeRoleMask;      // Keystone/Trigger/Synergy/Transmuter bit mask
  uint8_t qualityTier;        // Low/Medium/High/Ultra
  float intensity;            // 0.0 - 1.0
};
```

### 3.2 事件生成规则

- `SkillSystem` 在 `Preparing/Casting/Settle` 关键节点发事件。
- `Trigger` 节点额外发 `TriggerProc`，并包含触发来源 `castId`。
- 剑意满层消耗时必须发 `EmpoweredConsume`，避免特效和逻辑错位。

---

## 4. 分技能特效规格（3.1 - 3.9）

> 每技能定义：主特效、触发特效、性能上限（并发）、回退表现

### 4.1 3.1 流云刺（Flowing Thrust）

- 主特效：
  - 角色前冲残影条带（Trail）
  - 起点与终点短脉冲冲击圈
- 触发特效：
  - `TriggerProc` 时追加一次细剑气闪切
- 并发上限：`trail instances <= 24`
- Low回退：只保留终点冲击圈，不绘制残影条带

### 4.2 3.2 裂空斩（Rending Wave）

- 主特效：
  - 新月剑波网格 + 边缘菲涅耳
  - 轻度屏幕扭曲（仅 High/Ultra）
- 触发特效：
  - 回响斩以低亮度版本渲染（避免双重过曝）
- 并发上限：`projectile vfx <= 32`
- Low回退：禁用扭曲，仅保留发光边缘

### 4.3 3.3 灵剑决（Blade Formation）

- 主特效：
  - 灵剑实例化环绕（Instanced）
  - 发射时短尾迹
- 触发特效：
  - 巨剑触发时叠加短时环形震荡
- 并发上限：`spirit swords visual <= 16`
- Low回退：灵剑数量视觉减半，禁用尾迹

### 4.4 3.4 剑气护体（Blade Ward）

- 主特效：
  - 三向旋转护体剑环
  - 拦截触发火花
- 并发上限：`ward ring emitters <= 6`
- Low回退：仅显示简化圆环 + 命中闪光

### 4.5 3.5 万剑归宗（Infinite Blades）

- 主特效：
  - 区域预警 Decal
  - 批量下落飞剑粒子
- 并发上限：`rain particles <= 4096`
- Low回退：预警保留，下落粒子减到 `<= 1024`

### 4.6 3.6 剑阵·诛仙（Sword Array）

- 主特效：
  - 地面法阵纹理 + 边界剑柱
  - 阵内随机切线闪击
- 并发上限：`active arrays <= 4`
- Low回退：禁用随机闪击，仅保留阵环

### 4.7 3.7 心剑·无影（Mind Blade）

- 主特效：
  - 高频细束射线
  - 命中点收束火花
- 并发上限：`beam instances <= 64`
- Low回退：射线宽度固定，不做流动纹理

### 4.8 3.8 御剑·回旋（Blade Boomerang）

- 主特效：
  - 旋转飞剑 + 回返螺旋尾迹
  - 回返点冲击环
- 并发上限：`boomerang trail <= 20`
- Low回退：保留旋转本体，尾迹降采样

### 4.9 3.9 绝影绝剑（Phantom Trance）

- 主特效：
  - 入场隐化（Dissolve）+ 退场爆发
  - 逆脉状态下暗化边缘 + 高频脉冲
- 并发上限：`phantom overlays <= 4`
- Low回退：禁用全屏暗角，仅保留角色边缘光

---

## 5. RenderGraph 接入规范

### 5.1 Pass 责任

- 事件消费与实例准备：`GPUSkillEffectSystem::Update`
- 绘制：`VFXPass`（写 `SceneHdrColor`）
- 屏幕扭曲：`DistortionPass`（仅读取/写后处理目标）

### 5.2 Owner/Resource 声明（必须）

- `VFXPass`
  - Read: `SceneDepth`（可选）
  - Write: `SceneHdrColor`
  - Owner: `VFX`
- `DistortionPass`
  - Read: `PostProcessLdrColor`
  - Write: `DistortionLdrColor`
  - Owner: `Distortion`

### 5.3 禁止项

- 禁止在技能特效路径直接调用默认 framebuffer。
- 禁止跳过 `RenderGraph` 直接改 pass 顺序。

---

## 6. GPU 数据与 ABI 策略

- 优先复用：
  - `GPUSkillEffect`（SSBO 6）
  - `GPUParticle` / `GPUBeamInstance`
- 若 `GPUSkillEffect` 字段不足：
  - 先做字段复用方案评审
  - 必要时升级 ABI 版本并更新：
    - `GPUData.hpp`
    - `tools/render_abi/abi_manifest.json`
    - `assets/shaders/generated/gpu_abi.glslinc`
- 禁止手写 GLSL ABI struct

---

## 7. Tier 与回退矩阵

| Tier | 特效策略 | 约束 |
|---|---|---|
| Low | 关键判读特效保留，关闭扭曲/高密粒子 | 保障可玩性优先 |
| Medium | 开启基础GPU特效，限制并发与采样质量 | 帧时间稳定优先 |
| High | 完整技能特效 + 轻度后处理 | 平衡表现与性能 |
| Ultra | 全特效 + 高质量轨迹与扭曲 | 受预算门控 |

回退触发顺序：

1. 降低粒子发射率  
2. 关闭 Distortion  
3. 降低尾迹采样点  
4. 关闭次级发光层  

---

## 8. 性能预算（技能特效部分）

| 项目 | 常规预算 | 高压预算 |
|---|---:|---:|
| Sword Intent 特效 | <= 0.15ms | <= 0.25ms |
| 单技能平均特效 | <= 0.20ms | <= 0.35ms |
| VFXPass 总体 | <= 0.80ms | <= 1.10ms |
| DistortionPass（开启时） | <= 0.20ms | <= 0.35ms |

---

## 9. 验收清单（用于 Track Gate）

- [ ] 9 技能主特效可见且可区分
- [ ] Trigger/Empowered 的视觉反馈与逻辑事件一一对应
- [ ] RenderGraph ownership/contract 验证通过
- [ ] Low/Medium 回退下技能判读不丢失
- [ ] 特效预算满足第 8 节阈值

---

## 10. 与 Track 的映射

- `blade_ascendant_vfx_design_freeze_20260221`
  - 冻结本设计文档和事件契约
- `blade_ascendant_skill_rendering_integration_20260221`
  - 按本文件第 4/5/6/7 节落地
- `blade_ascendant_skill_validation_gate_20260221`
  - 按本文件第 8/9 节验收

