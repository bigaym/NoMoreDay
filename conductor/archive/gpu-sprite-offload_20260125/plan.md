# GPU 渲染卸载 - 实现计划
**Track:** `gpu-sprite-offload`
**Date:** 2026-01-25
**Spec:** `spec.md`

---

## 计划总览

| Phase | 绰号 | 核心目标 | 预估工时 | 状态 |
|-------|------|---------|---------|------|
| 1 | **The Foundation** | 纹理数组基础设施 | 4h | [x] Done |
| 2 | **The Texture Path** | MDI Shader 纹理采样支持 | 3h | [x] Done |
| 3 | **The Sync** | GPUEntitySystem 同步精灵数据 | 2h | [x] Done |
| 4 | **The Status Visuals** | 状态效果 Shader 集成 | 3h | [x] Done |
| 5 | **The Cleanup** | 遗留代码移除与优化 | 2h | [x] Done |
| 6 | **The Audit** | 全量验证与性能基准 | 2h | [x] Done |

**总预估工时:** 16h (约 2 工作日)

---

## Phase 1: The Foundation (纹理数组基础设施) - COMPLETED

**目标:** 创建 `GL_TEXTURE_2D_ARRAY` 资源加载与管理基础设施。

- [x] 在 `GPUData.hpp` 添加 `Constants::GPU` 命名空间，定义纹理常量
- [x] 在 `ResourceManager.hpp` 声明 `loadTextureArray()`、`getTextureLayerIndex()` 接口
- [x] 实现 `loadTextureArray()`: 使用 rlgl 创建纹理数组并上传图层
- [x] 实现 `getTextureLayerIndex()`: 维护 `name -> layer` 映射
- [x] 在 `Game::init()` 调用 `loadTextureArray()` 加载实体精灵图集

---

## Phase 2: The Texture Path (MDI Shader 纹理采样) - COMPLETED

**目标:** 扩展 MDI Shader 以支持纹理数组采样。

- [x] 修改 `entity_mdi.frag`: 添加 `uniform sampler2DArray entityTextures`
- [x] 修改 `entity_mdi.frag`: 实现纹理采样与 Alpha Cutout
- [x] 修改 `entity_mdi.vert`: 更新 `InstanceData` 对齐，支持 `int type`
- [x] 修改 `MDIRenderer::Render()`: 绑定纹理数组至 Sampler

---

## Phase 3: The Sync (GPUEntitySystem 同步精灵数据) - COMPLETED

**目标:** 从 `SpriteComponent` 读取纹理层索引并同步到 GPU。

- [x] 扩展 `SpriteComponent`: 新增 `textureLayerIndex` 字段
- [x] 修改 `GPUEntitySystem::Update()`: 同步 `textureLayerIndex` 到 `GPUEntity.type`
- [x] 修改 `EnemySpawnSystem`: 在生成敌人时通过种族/变体计算并设置图层索引
- [x] 移除 `GPUEntitySystem` 中对精灵实体的强制 `NO_RENDER` 标记

---

## Phase 4: The Status Visuals (状态效果 Shader 集成) - COMPLETED

**目标:** 将冰冻、燃烧等状态效果集成到 MDI Shader。

- [x] 扩展 `GPUVisualStats`: 添加 `activeStatusMask` 和 `statusTimer`
- [x] 修改 `GPUEntitySystem::Update()`: 同步 `ActiveEffectsComponent` 到 GPU
- [x] 修改 `entity_mdi.frag`: 添加状态视觉叠加逻辑（脉冲蓝光、燃烧闪烁）

---

## Phase 5: The Cleanup (遗留代码移除) - COMPLETED

**目标:** 删除 CPU 渲染路径的遗留代码，精简代码库。

- [x] `RenderSystem.cpp`: 卸载具有 GPUIndex 的实体精灵渲染
- [x] `EnemySpawnSystem.cpp`: 移除内部纹理加载逻辑，由全局纹理数组替代

---

## Phase 6: The Audit (全量验证与性能基准) - PENDING

**目标:** 完成全量测试与性能基准验证。

- [x] 运行全量单元测试
- [x] 运行集成测试：验证纹理显示正确且无崩溃
- [x] 性能基准测试：验证 Draw Calls 显著下降
- [x] RenderDoc 验证

---

## 当前进度

- [x] Phase 1: 已完成
- [x] Phase 2: 已完成
- [x] Phase 3: 已完成
- [x] Phase 4: 已完成
- [x] Phase 5: 已完成
- [x] Phase 6: 已完成

**备注:** 所有的核心代码修改已实施。玩家现在应该能看到敌人通过 MDI 高效渲染，并具有动态的状态视觉效果。