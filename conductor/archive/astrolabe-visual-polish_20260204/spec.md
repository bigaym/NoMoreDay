# 🌌 星盘视觉修复与优化 - 技术规格书

> **Track ID**: `astrolabe-visual-polish_20260204`
> **优先级**: **HIGH**
> **状态**: 📝 设计中
> **基准文档**: 
>   - [星系天赋系统设计说明书 V1.0](../../../设计文档/星系天赋系统设计说明书.md)
>   - [重构规格说明书 V1.1](../../../conductor/archive/astrolabe-refactor_20260204/spec.md)

---

## 1. 概述 (Overview)

本 Track 旨在补全现有 Astrolabe 实现与设计规格之间的视觉差距。当前框架（六扇区布局、黑洞中心、本命星轨道）已基本实现，但节点状态渲染、视觉反馈、GPU 特效等关键要素缺失，需逐项修复。

### 1.1 问题清单 (Gap Analysis)

| 问题 ID | 类别 | 问题描述 | 设计书参考 |
|---------|------|----------|------------|
| **V-001** | 节点状态 | 所有节点渲染为纯色圆盘，缺乏 Locked/Available/Activated/Sealed 状态视觉差异 | Spec 5.3 |
| **V-002** | 光晕特效 | 无节点光晕/脉动效果 (GPU Shader) | Spec 8.1 |
| **V-003** | 节点尺寸 | Minor/Major/Core 节点尺寸区分不明显 | Spec 2.1 |
| **V-004** | 轨道视觉 | 扇区分界线过淡，轨道环辅助引导不足 | Spec 5.1 |
| **V-005** | 数据填充 | 节点数据稀疏（目前仅测试数据） | Spec 4.2 |
| **V-006** | 封印纹理 | Core 节点无封印效果表现 | Spec 5.3/8.1 |
| **V-007** | 进度环 | Activated 节点进度环视觉过弱 | Spec 5.3 |

### 1.2 修复目标

1. **节点状态驱动渲染**: 实现 5 种状态的独立视觉反馈（颜色 + 特效）。
2. **GPU Shader 光效**: 琥珀色脉动（Available）、蓝色内核发光（Activated）、金色光芒（FullyActivated）。
3. **轨道引导增强**: 加深扇区分割线对比度，添加淡色轨道环背景。
4. **节点尺寸层级**: 确保 Minor (10px) < Major (16px) < Core (22px) 视觉区分。
5. **封印纹理**: 通过 Shader 程序化生成 Sealed 状态的紫色纹理。

---

## 2. 技术方案 (Technical Solution)

### 2.1 节点状态渲染增强 (`AstrolabeRenderer::DrawNodes`)

当前实现已有状态判断逻辑，但渲染仅为纯色 `DrawCircle`。需增强为：

```cpp
// AstrolabeRenderer.cpp - DrawNodes 重构
void AstrolabeRenderer::DrawNodes(...) {
    for (const auto& [id, node] : graph.nodes) {
        auto status = AstrolabeSystem::getNodeStatus(graph, *comp, id);
        
        float r = getNodeRadius(node.type);
        if (id == hoveredNodeId) r *= 1.2f;
        
        // [NEW] 状态驱动渲染
        switch (status) {
            case NodeStatus::Locked:
                DrawCircle(node.x, node.y, r, Fade(GRAY, 0.25f * view.alpha));
                break;
                
            case NodeStatus::Available:
                // 琥珀色脉动光晕 (CPU fallback, 后续迁移至 GPU)
                float pulse = 0.6f + 0.4f * sinf(view.time * 3.0f);
                DrawCircle(node.x, node.y, r * 1.3f, Fade(GOLD, 0.2f * pulse * view.alpha));
                DrawCircle(node.x, node.y, r, Fade(ORANGE, view.alpha));
                break;
                
            case NodeStatus::Activated:
                DrawCircle(node.x, node.y, r, Fade(SKYBLUE, view.alpha));
                // 进度环
                float progress = (float)comp->getNodePoints(id) / node.maxPoints;
                DrawRing({node.x, node.y}, r+2, r+5, 0, 360*progress, 32, Fade(SKYBLUE, view.alpha));
                break;
                
            case NodeStatus::FullyActivated:
                DrawCircle(node.x, node.y, r * 1.1f, Fade(GOLD, 0.5f * view.alpha)); // Outer glow
                DrawCircle(node.x, node.y, r, Fade(GOLD, view.alpha));
                break;
                
            case NodeStatus::Sealed:
                DrawCircle(node.x, node.y, r, Fade(DARKPURPLE, 0.6f * view.alpha));
                // [FUTURE] GPU 程序化封印纹理
                break;
        }
    }
}
```

### 2.2 轨道视觉增强 (`AstrolabeRenderer::DrawOrbits`)

```cpp
// AstrolabeRenderer.cpp - DrawOrbits 增强
void AstrolabeRenderer::DrawOrbits(const AstrolabeView& view) {
    using namespace Constants::Astrolabe;
    
    // 轨道环 - 增强对比度
    Color orbitColor = Fade(WHITE, 0.15f * view.alpha);
    float lineThickness = 1.5f;
    
    DrawCircleLinesV({0, 0}, ORBIT_R1, orbitColor);
    DrawCircleLinesV({0, 0}, ORBIT_R2, orbitColor);
    DrawCircleLinesV({0, 0}, ORBIT_R3, orbitColor);
    DrawCircleLinesV({0, 0}, ORBIT_R4, orbitColor);
    
    // 扇区分界线 - 加深
    Color sectorColor = Fade(GOLD, 0.2f * view.alpha);
    for (int i = 0; i < PROFESSION_COUNT; ++i) {
        float angle = TalentLayoutService::getSectorCenterAngle((ProfessionID)i) + 30.0f;
        float rad = angle * DEG2RAD;
        Vector2 end = { cosf(rad) * ORBIT_R4 * 1.3f, sinf(rad) * ORBIT_R4 * 1.3f };
        DrawLineEx({0, 0}, end, 2.0f, sectorColor);
    }
}
```

### 2.3 节点尺寸辅助函数

```cpp
// AstrolabeRenderer.hpp - 新增
namespace AstrolabeRenderer {
    inline float getNodeRadius(TalentNodeType type) {
        using namespace Constants::Astrolabe;
        switch (type) {
            case TalentNodeType::Minor: return NODE_RADIUS_MINOR;
            case TalentNodeType::Major: return NODE_RADIUS_MAJOR;
            case TalentNodeType::Core:  return NODE_RADIUS_CORE;
            default: return NODE_RADIUS_MINOR;
        }
    }
}
```

### 2.4 数据填充 (profession_talents.json)

需为 6 个职业填充测试节点数据（每职业至少 5 个 Tier 1 Minor + 2 个 Tier 2 Major + 1 个 Tier 3 Core），确保视觉布局可验证。

```json
{
  "version": 1,
  "profession_stars": [
    { "profession": 0, "name_key": "剑修之心", "desc_key": "以剑入道" },
    { "profession": 1, "name_key": "法师之魂", "desc_key": "掌控元素" },
    { "profession": 2, "name_key": "祭祀之心", "desc_key": "治疗与诅咒" },
    { "profession": 3, "name_key": "骑士之魂", "desc_key": "铁壁守护" },
    { "profession": 4, "name_key": "游侠之心", "desc_key": "远近皆宜" },
    { "profession": 5, "name_key": "狂战之魂", "desc_key": "以血为代价" }
  ],
  "nodes": [
    // 剑修 (profession: 0)
    { "id": 1001, "profession": 0, "type": "Minor", "tier": 1, "sector_index": 0, "max_points": 5, "name_key": "强壮", "modifiers": [] },
    { "id": 1002, "profession": 0, "type": "Minor", "tier": 1, "sector_index": 1, "max_points": 5, "name_key": "敏锐", "modifiers": [] },
    { "id": 1003, "profession": 0, "type": "Minor", "tier": 1, "sector_index": 2, "max_points": 5, "name_key": "坚韧", "modifiers": [] },
    { "id": 1004, "profession": 0, "type": "Minor", "tier": 1, "sector_index": 3, "max_points": 5, "name_key": "迅捷", "modifiers": [] },
    { "id": 1005, "profession": 0, "type": "Minor", "tier": 1, "sector_index": 4, "max_points": 5, "name_key": "精准", "modifiers": [] },
    { "id": 1010, "profession": 0, "type": "Major", "tier": 2, "sector_index": 0, "max_points": 3, "name_key": "剑气凌厉", "modifiers": [] },
    { "id": 1011, "profession": 0, "type": "Major", "tier": 2, "sector_index": 1, "max_points": 3, "name_key": "剑心通明", "modifiers": [] },
    { "id": 1020, "profession": 0, "type": "Core", "tier": 3, "sector_index": 0, "max_points": 1, "name_key": "剑道真意", "modifiers": [] }
    // ... 其余职业类似结构 (省略)
  ]
}
```

---

## 3. 文件变更范围 (Scope)

| 文件 | 变更类型 | 描述 |
|------|----------|------|
| `src/game/systems/ui/AstrolabeRenderer.cpp` | 修改 | DrawNodes/DrawOrbits 视觉增强 |
| `src/game/systems/ui/AstrolabeRenderer.hpp` | 修改 | 新增辅助函数 |
| `assets/data/profession_talents.json` | 修改 | 填充测试节点数据 |
| `assets/shaders/talent_node.fs` | 新增 (Phase 2) | GPU 节点特效 Shader |

---

## 4. 验收标准 (Acceptance Criteria)

- [x] **AC1**: Locked 节点呈暗灰色 (`Fade(GRAY, 0.25f)`)，无光晕。
- [x] **AC2**: Available 节点呈琥珀/金色，有可见的脉动光晕效果。
- [x] **AC3**: Activated 节点呈天蓝色，有清晰的进度环显示当前点数。
- [x] **AC4**: FullyActivated 节点呈金色，有外发光效果。
- [x] **AC5**: Sealed (Core, 非主修) 节点呈暗紫色。
- [x] **AC6**: Minor < Major < Core 节点尺寸视觉可区分。
- [x] **AC7**: 轨道环和扇区分界线清晰可见。
- [x] **AC8**: 加载 `profession_talents.json` 后，6 个职业扇区均有节点分布。

---

## 5. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| **性能** | 大量节点脉动可能增加渲染开销 | 先实现 CPU fallback，Phase 2 迁移至 GPU Shader |
| **视觉拥挤** | Tier 3 节点过多时重叠 | 动态调整扇区内角度分布 (TalentLayoutService) |
| **数据依赖** | 无真实技能数据填充 | 使用占位符 modifiers，确保布局可验证 |

---

*规格版本: 1.0*
*创建时间: 2026-02-04*
