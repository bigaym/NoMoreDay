# GPU 渲染卸载 - 实现计划
**Track:** `gpu-sprite-offload`
**Date:** 2026-01-25
**Spec:** `spec.md`

---

## 计划总览

| Phase | 绰号 | 核心目标 | 预估工时 | 依赖 |
|-------|------|---------|---------|------|
| 1 | **The Foundation** | 纹理数组基础设施 | 4h | 无 |
| 2 | **The Texture Path** | MDI Shader 纹理采样支持 | 3h | Phase 1 |
| 3 | **The Sync** | GPUEntitySystem 同步精灵数据 | 2h | Phase 2 |
| 4 | **The Status Visuals** | 状态效果 Shader 集成 | 3h | Phase 3 |
| 5 | **The Cleanup** | 遗留代码移除与优化 | 2h | Phase 4 |
| 6 | **The Audit** | 全量验证与性能基准 | 2h | Phase 5 |

**总预估工时:** 16h (约 2 工作日)

---

## Phase 1: The Foundation (纹理数组基础设施)

**目标:** 创建 `GL_TEXTURE_2D_ARRAY` 资源加载与管理基础设施。

### 任务列表

| ID | 任务描述 | 预估 |
|----|---------|------|
| 1.1 | 在 `GPUData.hpp` 添加 `Constants::GPU` 命名空间，定义纹理常量 | 15m |
| 1.2 | 在 `ResourceManager.hpp` 声明 `LoadTextureArray()`、`GetTextureLayerIndex()` 接口 | 20m |
| 1.3 | 实现 `LoadTextureArray()`: 遍历路径列表，加载 stb_image，创建 `glTexImage3D` | 1.5h |
| 1.4 | 实现 `GetTextureLayerIndex()`: 维护 `name -> layer` 映射 | 20m |
| 1.5 | 在 `ResourceManager::Init()` 调用 `LoadTextureArray()` 加载 `assets/textures/entities/*.png` | 30m |
| 1.6 | 编写单元测试: `ResourceManagerTextureArrayTest.cpp` | 45m |

**验证目标:**
- `LoadTextureArray()` 返回有效 OpenGL ID
- RenderDoc 确认纹理数组正确创建 (Layer Count > 0)
- 测试: 加载 3 张测试纹理，验证层索引正确

**风险点:**
- stb_image 加载后需强制转换为 128x128 (或警告拒绝)

---

## Phase 2: The Texture Path (MDI Shader 纹理采样)

**目标:** 扩展 MDI Shader 以支持纹理数组采样。

### 任务列表

| ID | 任务描述 | 预估 |
|----|---------|------|
| 2.1 | 修改 `entity_mdi.frag`: 添加 `uniform sampler2DArray entityTextures` | 15m |
| 2.2 | 修改 `entity_mdi.frag`: `main()` 中根据 `vTextureIndex >= 0` 选择纹理采样或 SDF | 45m |
| 2.3 | 修改 `entity_mdi.vert`: 确保 `vTextureIndex` 作为 `flat int` 传递 | 15m |
| 2.4 | 修改 `MDIRenderer::Init()`: 获取 `entityTextures` uniform location | 20m |
| 2.5 | 修改 `MDIRenderer::Render()`: 绑定 `ResourceManager::GetEntityTextureArray()` 至 Sampler | 30m |
| 2.6 | 集成测试: 创建测试实体 (`type = 0`)，验证渲染出纹理数组第 0 层 | 45m |

**验证目标:**
- Shader 编译成功，无链接错误
- 测试实体显示正确纹理而非红色 SDF 圆
- RenderDoc 确认 `glBindTexture(GL_TEXTURE_2D_ARRAY, ...)` 调用

**Scoped 验证:** `MDIRenderer::Render` 无额外性能开销 (< 0.5ms 增量)

---

## Phase 3: The Sync (GPUEntitySystem 同步精灵数据)

**目标:** 从 `SpriteComponent` 读取纹理层索引并同步到 GPU。

### 任务列表

| ID | 任务描述 | 预估 |
|----|---------|------|
| 3.1 | 扩展 `SpriteComponent`: 新增 `textureLayerIndex` 字段 (通过 ResourceManager 转换) | 30m |
| 3.2 | 修改 `GPUEntitySystem::Update()`: 读取 `SpriteComponent.textureLayerIndex` 写入 `GPUEntity.type` | 30m |
| 3.3 | **核心修改**: 移除 `Update()` 中对带 `SpriteComponent` 实体的 `GPU_ENTITY_FLAG_NO_RENDER` 标记 | 20m |
| 3.4 | 修改 `EntityFactory` / 实体创建流程: 确保 `SpriteComponent` 初始化时查询 `textureLayerIndex` | 30m |
| 3.5 | 集成测试: 生成 100 个敌人实体，验证全部通过 MDI 渲染 | 30m |

**验证目标:**
- `GPU_ENTITY_FLAG_NO_RENDER` 统计数量大幅下降 (> 90% 实体不再携带该 Flag)
- 游戏中敌人实体显示正确纹理
- 性能日志: Draw Calls 下降

---

## Phase 4: The Status Visuals (状态效果 Shader 集成)

**目标:** 将冰冻、燃烧等状态效果集成到 MDI Shader。

### 任务列表

| ID | 任务描述 | 预估 |
|----|---------|------|
| 4.1 | 扩展 `GPUVisualStats`: 添加 `activeStatusMask` 和 `statusTimer` 字段 (复用 Padding) | 20m |
| 4.2 | 修改 `GPUEntitySystem::Update()`: 从 `StatusEffectComponent` 读取状态，写入 `GPUVisualStats` | 45m |
| 4.3 | 修改 `entity_mdi.vert`: 新增 `vStatusMask`, `vStatusTimer` out 变量 | 15m |
| 4.4 | 修改 `entity_mdi.frag`: 添加状态视觉叠加逻辑 (冰冻蓝光、燃烧闪烁) | 45m |
| 4.5 | 移除 `RenderSystem.cpp` 中独立的状态 Overlay 渲染逻辑 (如有) | 30m |
| 4.6 | 视觉测试: 对实体施加冰冻效果，确认 Shader 输出蓝色光晕 | 30m |

**验证目标:**
- 冰冻状态实体显示脉冲蓝光
- 燃烧状态实体显示橙红闪烁
- RenderDoc: 无额外 Overlay Draw Calls

**Scoped 验证:** 无明显帧率影响 (< 0.2ms 增量)

---

## Phase 5: The Cleanup (遗留代码移除)

**目标:** 删除 CPU 渲染路径的遗留代码，精简代码库。

### 任务列表

| ID | 任务描述 | 预估 |
|----|---------|------|
| 5.1 | `RenderSystem.cpp`: 移除/条件化 CPU 精灵渲染循环 (保留对非标尺寸资源的回退) | 45m |
| 5.2 | `PopupRenderer.cpp`: 审查并移除任何遗留 CPU Draw 代码 | 30m |
| 5.3 | `GPUEntitySystem.cpp`: 移除 `RenderLegacy()` 函数 (如不再需要) | 20m |
| 5.4 | 代码审查: 全局搜索 `GPU_ENTITY_FLAG_NO_RENDER` 使用点，确保语义正确 | 25m |

**验证目标:**
- 编译通过，无新 Warning
- 游戏运行正常，无视觉回归
- 代码行数减少

---

## Phase 6: The Audit (全量验证与性能基准)

**目标:** 完成全量测试与性能基准验证。

### 任务列表

| ID | 任务描述 | 预估 |
|----|---------|------|
| 6.1 | 运行全量单元测试 `./build/bin/NoMoreDayTests.exe` | 20m |
| 6.2 | 运行集成测试: 战斗场景 (万级敌人) 验证无崩溃 | 30m |
| 6.3 | 性能基准测试: 记录 `RenderSystem::Render` 耗时，对比优化前基线 | 30m |
| 6.4 | RenderDoc 帧分析: 验证 Draw Calls、纹理绑定、Overdraw | 20m |
| 6.5 | 运行 `code-risk-analyzer` 技能审计 | 20m |
| 6.6 | 更新 `tracks.md` 标记 Track 完成 | 10m |

**验收指标:**

| 指标 | Before | After | Target |
|-----|--------|-------|--------|
| Draw Calls (10k Entities) | ~1000 | **< 10** | ✅ |
| GPU-Rendered Entity Rate | ~30% | **> 95%** | ✅ |
| `RenderSystem::Render` 耗时 | ~4ms | **< 1.5ms** | ✅ |
| Overdraw (Status Overlays) | > 1.5x | **~ 1.0x** | ✅ |
| All Tests Passed | - | **Yes** | ✅ |

---

## 依赖链图示

```
Phase 1 (Foundation)
    |
    v
Phase 2 (Texture Path)
    |
    v
Phase 3 (Sync)
    |
    v
Phase 4 (Status Visuals)
    |
    v
Phase 5 (Cleanup)
    |
    v
Phase 6 (Audit)
```

---

## 风险监控点

| 阶段 | 潜在风险 | 熔断条件 | 应对措施 |
|-----|---------|---------|---------|
| Phase 1 | 纹理加载失败 (路径/格式错误) | 测试覆盖率 < 80% | 添加详细日志，逐个验证资源文件 |
| Phase 2 | Shader 编译失败 (驱动兼容性) | 游戏无法启动 | 保留 SDF 回退，运行时检测 |
| Phase 3 | 同步延迟导致闪烁 | 视觉观测到明显抖动 | 复用 `prevPosition` 插值逻辑 |
| Phase 4 | 状态叠加视觉效果不协调 | 用户反馈负面 | 调整 Shader 参数，可设为可配置 |
| Phase 5 | 移除代码破坏未预见依赖 | 编译错误或运行时崩溃 | 增量移除，每步验证编译 |

---

## 当前进度

- [x] Phase 1: 已完成 - 纹理数组基础设施 (工具函数、标准化加载、索引映射已就绪)
- [x] Phase 2: 已完成 - Shader 支持纹理数组采样 (MDI Shader 已支持 sampler2DArray 采样)
- [>] Phase 3: 进行中 - GPUEntitySystem 同步精灵数据 (正在整合 SpriteComponent 映射)
- [>] Phase 4: 进行中 - 状态效果 Shader 集成 (Shader 端已完成，数据同步已部分接入)
- [ ] Phase 5: 待开始 - 移除 RenderLegacy 与 Cleanup
- [ ] Phase 6: 待开始 - 全量验证与 Audit

### 已验证结果:
- `GPUUtils::LoadTextureArray` 支持分层上传图像。
- `ResourceManager::loadTextureArray` 实现了自动尺寸缩放与 RGBA 转换。
- `entity_mdi.frag` 已成功实现 textureArray 采样与 Status Overlay 混合。
- `GPUEntitySystem` 已具备将实体 race 类型映射到纹理索引的能力。
