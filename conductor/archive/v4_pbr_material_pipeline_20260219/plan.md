# 2D PBR Material Pipeline 实施计划

> **Track ID**: `v4_pbr_material_pipeline_20260219`  
> **依赖 Spec**: `spec.md`  
> **依赖 Track**: `v4_preflight_v3_closure_20260219`, `v4_gpu_text_rendering_20260219`, `v4_gpu_loot_rendering_20260219`  
> **状态**: [x] Completed

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|---|---|---|---|
| Phase 1 | Schema & ABI | `GPUMaterialDataV3` + v2→v3 映射 | [x] |
| Phase 2 | 贴图管线 | Texture2DArray 分层 + 加载器升级 | [x] |
| Phase 3 | BRDF-Lite Shader | `entity_mdi.frag` + `particle.frag` | [x] |
| Phase 4 | 美术工具链 | Height/Normal/AO/Mask 脚本 | [x] |
| Phase 5 | 验证与内容 | 三类 Sprite 示例 + Tier 验证 | [x] |

---

## Phase 1: Schema & ABI

### Tasks
- [x] Task 1.1: 定义 `GPUMaterialDataV3` 结构 + `static_assert`
- [x] Task 1.2: ABI 工具链更新（GLSL 同步生成）
- [x] Task 1.3: `material_schema_version` 升级逻辑（v2→v3 自动映射）
- [x] Task 1.4: V2 JSON 兼容单测（缺失字段默认值，不崩溃）
- [x] Task 1.5: `GPU_ABI_VERSION` 递增至 4

### Verification
- [x] ABI layout 测试通过
- [x] V2 材质 JSON 加载不崩溃

---

## Phase 2: 贴图管线

### Tasks
- [x] Task 2.1: Texture2DArray 分层管理（Albedo/Normal/Mask/Detail）
- [x] Task 2.2: 贴图加载器升级（Normal/Mask 路径接入）
- [x] Task 2.3: `MaterialManager` 升级（`textureSlots` 映射 Array 层）
- [x] Task 2.4: Detail Normal 可选加载（Ultra）
- [x] Task 2.5: 双缓冲热重载路径沿用并验证

### Verification
- [x] 贴图 Array 正确绑定到 Shader
- [x] 热重载路径可用

---

## Phase 3: BRDF-Lite Shader

### Tasks
- [x] Task 3.1: 实现 `brdfLite()`（GGX + Schlick-GGX + Schlick Fresnel）
- [x] Task 3.2: 2D 修正（rim 抑制 + roughness bias）
- [x] Task 3.3: 集成到 `entity_mdi.frag`
- [x] Task 3.4: 集成到 `particle.frag`
- [x] Task 3.5: Tier 分支（Low/Med/High/Ultra）

### Verification
- [x] 同光异材路径可执行
- [x] Low 档回退不报错

---

## Phase 4: 美术工具链

### Tasks
- [x] Task 4.1: Height Map 脚本 `scripts/pbr_build_height.py`
- [x] Task 4.2: Normal Map 脚本 `scripts/pbr_build_normal.py`
- [x] Task 4.3: AO 脚本 `scripts/pbr_build_ao.py`
- [x] Task 4.4: Mask 打包脚本 `scripts/pbr_pack_mask.py`
- [x] Task 4.5: 美术指南文档 `设计文档/特效和UI/pbr_material_guidelines_v4.md`

### Verification
- [x] 至少 1 个 Sprite 通过工具链输出四层贴图
- [x] 提供 CI 入口 `scripts/pbr_pipeline_ci.py`

---

## Phase 5: 验证与内容

### Tasks
- [x] Task 5.1: 玩家角色 PBR 贴图样例
- [x] Task 5.2: 怪物 Sprite PBR 贴图样例
- [x] Task 5.3: 场景物体 PBR 贴图样例
- [x] Task 5.4: 全 Tier 路径验证（Low/Med/High/Ultra）
- [x] Task 5.5: ScenePass 开销验证（见 `MaterialLightingBenchmark`）

### Verification
- [x] 三类 Sprite 样例可用
- [x] 关键构建与测试通过
- [x] 性能标签测试已执行；非本任务阻塞项已记录
