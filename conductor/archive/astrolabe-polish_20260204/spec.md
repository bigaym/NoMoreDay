# 🌌 星系天赋系统审计修复 - 技术规格书

> **Track ID**: `astrolabe-polish_20260204`
> **依赖前置**: `astrolabe-refactor_20260204` (已归档)
> **审计来源**: Architecture Audit Report 2026-02-04
> **状态**: 📝 设计中

---

## 1. 概述

本 Track 旨在修复 `astrolabe-refactor` 重构后遗留的架构缺陷和功能缺失，确保系统达到设计文档 (`星系天赋系统设计说明书.md`) 的全部验收标准。

### 1.1 修复目标

| ID | 问题 | 风险等级 | 优先级 |
|----|------|----------|--------|
| **FIX-1** | 誓约机制无二次确认 | 🔴 高 | **P0 CRITICAL** |
| **FIX-2** | 测试数据 ID 不一致 | 🔴 高 | **P0 CRITICAL** |
| **FIX-3** | 核心系统逻辑无单元测试 | 🟠 中高 | **P1 HIGH** |
| **FIX-4** | 解锁失败无 UI 反馈 | 🟡 中 | **P2 MEDIUM** |
| **FIX-5** | GPU Shader 特效未实现 | 🟡 中 | **P3 LOW** |
| **FIX-6** | 冗余 API 清理 | 🟢 低 | **P3 LOW** |

### 1.2 不在范围内

- 新增职业/节点内容
- 存档兼容性（已确认无需兼容）
- 本地化支持

---

## 2. FIX-1: 誓约二次确认机制

### 2.1 问题描述

设计文档要求：
> "点击即选定" 存在误操作风险 → 必须实现 **长按 2 秒确认** + **弹出对话框**

当前实现 (`UIAstrolabe.cpp:172-176`)：
```cpp
if (hoveredStar && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && ...) {
    AstrolabeSystem::takeVow(registry, player, hoveredStar->profession);
    // 直接执行，无确认
}
```

### 2.2 技术方案

#### 2.2.1 新增组件：誓约确认状态

```cpp
// UIAstrolabe.hpp - 新增静态成员
class UIAstrolabe {
    // ... existing ...
    
    // 誓约确认状态
    static ProfessionID s_pendingVowProfession;
    static float s_vowHoldProgress;         // 0.0 ~ 1.0
    static constexpr float VOW_HOLD_DURATION = 2.0f;  // 2 秒长按
    static bool s_showVowDialog;
};
```

#### 2.2.2 誓约对话框渲染

```cpp
// UIAstrolabe.cpp - 新增函数
void UIAstrolabe::DrawVowDialog(entt::registry& registry, entt::entity player, const ProfessionStar& star) {
    // 对话框尺寸
    float w = 500.0f * scale;
    float h = 280.0f * scale;
    float x = (screenWidth - w) / 2;
    float y = (screenHeight - h) / 2;
    
    // 背景
    DrawRectangleRec({x, y, w, h}, Fade(BLACK, 0.95f * s_alpha));
    DrawRectangleLinesEx({x, y, w, h}, 2.0f, Fade(GOLD, s_alpha));
    
    // 标题
    DrawTextEx(font, "⚠️ 深渊凝视 (The Vow)", {x + 20, y + 20}, 28 * scale, 1, GOLD);
    
    // 职业信息
    DrawTextEx(font, TextFormat("你即将与 [%s] 职业建立不可逆转的誓约。", star.name_key.c_str()), 
               {x + 20, y + 70}, 20 * scale, 1, WHITE);
    
    // 警示文本
    DrawTextEx(font, "• 解锁所有该职业的核心天赋 (Core Nodes)", {x + 40, y + 110}, 18 * scale, 1, GREEN);
    DrawTextEx(font, "• 其他职业的核心天赋将被永久封印", {x + 40, y + 140}, 18 * scale, 1, RED);
    
    // 长按确认按钮
    Rectangle confirmBtn = {x + w/2 - 150*scale, y + h - 90*scale, 300*scale, 50*scale};
    bool hover = CheckCollisionPointRec(GetMousePosition(), confirmBtn);
    
    // 进度条
    DrawRectangleRec(confirmBtn, Fade(DARKGRAY, 0.8f * s_alpha));
    DrawRectangleRec({confirmBtn.x, confirmBtn.y, confirmBtn.width * s_vowHoldProgress, confirmBtn.height}, 
                     Fade(GOLD, s_alpha));
    DrawRectangleLinesEx(confirmBtn, 2.0f, Fade(hover ? GOLD : GRAY, s_alpha));
    
    const char* btnText = s_vowHoldProgress > 0.0f 
        ? TextFormat("长按确认... (%.1fs)", VOW_HOLD_DURATION * (1.0f - s_vowHoldProgress))
        : "长按此处确认誓约 (2秒)";
    DrawTextEx(font, btnText, {confirmBtn.x + 30*scale, confirmBtn.y + 15*scale}, 18 * scale, 1, WHITE);
    
    // 取消按钮
    Rectangle cancelBtn = {x + w/2 - 60*scale, y + h - 30*scale, 120*scale, 28*scale};
    if (CheckCollisionPointRec(GetMousePosition(), cancelBtn)) {
        DrawTextEx(font, "[ 取消 ]", {cancelBtn.x, cancelBtn.y}, 16 * scale, 1, YELLOW);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            s_showVowDialog = false;
            s_vowHoldProgress = 0.0f;
        }
    } else {
        DrawTextEx(font, "[ 取消 ]", {cancelBtn.x, cancelBtn.y}, 16 * scale, 1, GRAY);
    }
    
    // 长按逻辑
    if (hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        s_vowHoldProgress += dt / VOW_HOLD_DURATION;
        if (s_vowHoldProgress >= 1.0f) {
            AstrolabeSystem::takeVow(registry, player, star.profession);
            s_showVowDialog = false;
            s_vowHoldProgress = 0.0f;
        }
    } else {
        s_vowHoldProgress = std::max(0.0f, s_vowHoldProgress - dt * 2.0f);  // 快速衰减
    }
}
```

#### 2.2.3 交互流程修改

```cpp
// UIAstrolabe.cpp:DrawInternal() - 修改 ProfessionStar 点击逻辑
// BEFORE:
// if (hoveredStar && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && astroComp && s_alpha > 0.9f) {
//     if (!astroComp->hasVow()) {
//         AstrolabeSystem::takeVow(registry, player, hoveredStar->profession);
//     }
// }

// AFTER:
if (hoveredStar && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && astroComp && s_alpha > 0.9f) {
    if (!astroComp->hasVow()) {
        s_pendingVowProfession = hoveredStar->profession;
        s_showVowDialog = true;
        s_vowHoldProgress = 0.0f;
    }
}

// 在 DrawInternal 末尾添加对话框渲染
if (s_showVowDialog && astroComp && !astroComp->hasVow()) {
    const auto& star = graph.professionStars[(int)s_pendingVowProfession];
    DrawVowDialog(registry, player, star);
}
```

### 2.3 验收标准

- [ ] **AC-VOW-1**: 点击本命星弹出确认对话框
- [ ] **AC-VOW-2**: 必须长按 2 秒才能完成誓约
- [ ] **AC-VOW-3**: 松手后进度条衰减
- [ ] **AC-VOW-4**: 点击"取消"关闭对话框
- [ ] **AC-VOW-5**: 对话框显示警示文本

---

## 3. FIX-2: 测试数据 ID 一致性

### 3.1 问题描述

`TalentLayoutTests.cpp:77-106` 引用节点 ID `1001`, `3001`，但 `profession_talents.json` 实际 ID 为 `1100-6300`。

### 3.2 技术方案

更新测试文件使用正确的节点 ID：

```cpp
// tests/unit/TalentLayoutTests.cpp - 修改
TEST_CASE("[Integration] TalentLoader - Load Profession Talents") {
    TalentGraph graph;
    bool success = TalentLoader::LoadProfessionTalents("assets/data/profession_talents.json", graph);
    CHECK(success);
    
    if (success) {
        // 使用实际存在的 ID: 1100 (Blade Ascendant Minor 1)
        const auto* node1100 = graph.findNode(1100);
        REQUIRE(node1100 != nullptr);
        CHECK(node1100->profession == ProfessionID::BladeAscendant);
        CHECK(node1100->tier == 1);
        CHECK(node1100->type == TalentNodeType::Minor);
        CHECK(node1100->maxPoints == 5);
        
        // 使用实际存在的 Core ID: 1300 (Blade Ascendant Core)
        const auto* node1300 = graph.findNode(1300);
        REQUIRE(node1300 != nullptr);
        CHECK(node1300->type == TalentNodeType::Core);
    }
}
```

### 3.3 验收标准

- [ ] **AC-TEST-1**: `TalentLayoutTests.cpp` 编译通过
- [ ] **AC-TEST-2**: 所有测试用例 PASS

---

## 4. FIX-3: AstrolabeSystem 单元测试

### 4.1 问题描述

核心系统 `AstrolabeSystem` 无任何单元测试覆盖。

### 4.2 技术方案

创建 `tests/unit/AstrolabeSystemTests.cpp`：

```cpp
#include "doctest.h"
#include "game/systems/skill/AstrolabeSystem.hpp"
#include "game/data/TalentData.hpp"
#include <entt/entt.hpp>

using namespace NoMoreDay;

// 辅助函数：创建测试用 TalentGraph
TalentGraph CreateTestGraph() {
    TalentGraph graph;
    
    // Blade Ascendant Tier 1 Minor
    AstrolabeTalentNode n1;
    n1.id = 1100; n1.profession = ProfessionID::BladeAscendant; 
    n1.tier = 1; n1.type = TalentNodeType::Minor; n1.maxPoints = 5;
    graph.nodes[1100] = n1;
    
    // Blade Ascendant Tier 2 Major
    AstrolabeTalentNode n2;
    n2.id = 1200; n2.profession = ProfessionID::BladeAscendant; 
    n2.tier = 2; n2.type = TalentNodeType::Major; n2.maxPoints = 3;
    graph.nodes[1200] = n2;
    
    // Blade Ascendant Tier 3 Core
    AstrolabeTalentNode n3;
    n3.id = 1300; n3.profession = ProfessionID::BladeAscendant; 
    n3.tier = 3; n3.type = TalentNodeType::Core; n3.maxPoints = 1;
    graph.nodes[1300] = n3;
    
    // Mage Core (for testing cross-profession seal)
    AstrolabeTalentNode n4;
    n4.id = 2300; n4.profession = ProfessionID::Mage; 
    n4.tier = 3; n4.type = TalentNodeType::Core; n4.maxPoints = 1;
    graph.nodes[2300] = n4;
    
    return graph;
}

TEST_SUITE("AstrolabeSystemTests") {

    TEST_CASE("[Unit] Tier 1 Unlock - No Affinity Required") {
        auto graph = CreateTestGraph();
        AstrolabeComponent comp;
        comp.available_points = 10;
        
        // Tier 1 should be unlockable with 0 affinity
        CHECK(AstrolabeSystem::canUnlockNode(graph, comp, 1100) == true);
    }
    
    TEST_CASE("[Unit] Tier 2 Unlock - Affinity Threshold") {
        auto graph = CreateTestGraph();
        AstrolabeComponent comp;
        comp.available_points = 10;
        
        // Tier 2 requires 10 affinity
        comp.professionAffinity[0] = 9;
        CHECK(AstrolabeSystem::canUnlockNode(graph, comp, 1200) == false);
        
        comp.professionAffinity[0] = 10;
        CHECK(AstrolabeSystem::canUnlockNode(graph, comp, 1200) == true);
    }
    
    TEST_CASE("[Unit] Core Unlock - Requires Vow") {
        auto graph = CreateTestGraph();
        AstrolabeComponent comp;
        comp.available_points = 10;
        comp.professionAffinity[0] = 30;  // Exceeds Tier 3 threshold
        
        // Core node requires vow
        CHECK(AstrolabeSystem::canUnlockNode(graph, comp, 1300) == false);
        
        // After vow
        comp.mainProfession = 0;  // Blade Ascendant
        CHECK(AstrolabeSystem::canUnlockNode(graph, comp, 1300) == true);
        
        // Cross-profession Core should be sealed
        CHECK(AstrolabeSystem::canUnlockNode(graph, comp, 2300) == false);
    }
    
    TEST_CASE("[Unit] Take Vow - Once Only") {
        entt::registry registry;
        auto player = registry.create();
        registry.emplace<AstrolabeComponent>(player);
        
        CHECK(AstrolabeSystem::canTakeVow(registry.get<AstrolabeComponent>(player), ProfessionID::BladeAscendant) == true);
        
        bool result = AstrolabeSystem::takeVow(registry, player, ProfessionID::BladeAscendant);
        CHECK(result == true);
        CHECK(registry.get<AstrolabeComponent>(player).mainProfession == 0);
        
        // Second vow should fail
        CHECK(AstrolabeSystem::canTakeVow(registry.get<AstrolabeComponent>(player), ProfessionID::Mage) == false);
        result = AstrolabeSystem::takeVow(registry, player, ProfessionID::Mage);
        CHECK(result == false);
    }
    
    TEST_CASE("[Unit] Add Point - Affinity Accumulation") {
        entt::registry registry;
        auto player = registry.create();
        registry.emplace<AstrolabeComponent>(player);
        registry.get<AstrolabeComponent>(player).available_points = 5;
        
        // Need to emplace CombatStats for AttributePipeline
        registry.emplace<CombatStats>(player);
        
        auto graph = CreateTestGraph();
        
        // Add 5 points to node 1100
        for (int i = 0; i < 5; ++i) {
            bool added = AstrolabeSystem::addPointToNode(registry, player, graph, 1100);
            CHECK(added == true);
        }
        
        auto& comp = registry.get<AstrolabeComponent>(player);
        CHECK(comp.nodePoints[1100] == 5);
        CHECK(comp.professionAffinity[0] == 5);
        CHECK(comp.available_points == 0);
        
        // 6th point should fail (max reached)
        bool added = AstrolabeSystem::addPointToNode(registry, player, graph, 1100);
        CHECK(added == false);
    }
    
    TEST_CASE("[Unit] Node Status - All States") {
        auto graph = CreateTestGraph();
        AstrolabeComponent comp;
        comp.available_points = 10;
        
        // Locked (no points, Tier 2)
        CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 1200) == AstrolabeSystem::NodeStatus::Locked);
        
        // Available (Tier 1)
        CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 1100) == AstrolabeSystem::NodeStatus::Available);
        
        // Activated (partial points)
        comp.nodePoints[1100] = 2;
        CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 1100) == AstrolabeSystem::NodeStatus::Activated);
        
        // Fully Activated
        comp.nodePoints[1100] = 5;
        CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 1100) == AstrolabeSystem::NodeStatus::FullyActivated);
        
        // Sealed (Core, wrong vow)
        comp.mainProfession = 0;  // Blade Ascendant
        comp.professionAffinity[1] = 30;  // Mage affinity
        CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 2300) == AstrolabeSystem::NodeStatus::Sealed);
    }
}
```

### 4.3 CMakeLists.txt 更新

```cmake
# tests/CMakeLists.txt - 添加测试文件
set(TEST_SOURCES
    # ... existing ...
    unit/AstrolabeSystemTests.cpp
)
```

### 4.4 验收标准

- [ ] **AC-ASYS-1**: 创建 `AstrolabeSystemTests.cpp`
- [ ] **AC-ASYS-2**: 所有 6 个测试用例 PASS
- [ ] **AC-ASYS-3**: 集成到 CMakeLists.txt

---

## 5. FIX-4: 解锁失败 UI 反馈

### 5.1 问题描述

`addPointToNode()` 失败时无任何 UI 反馈，玩家不知道为何无法点亮节点。

### 5.2 技术方案

#### 5.2.1 添加失败原因枚举

```cpp
// AstrolabeSystem.hpp - 新增
enum class UnlockFailReason {
    Success,
    NoPoints,           // 星尘不足
    TierLocked,         // 亲和度不足
    CoreSealed,         // 核心节点需誓约
    MaxPointsReached,   // 节点已满
    NodeNotFound        // 节点不存在
};

static UnlockFailReason tryUnlockNode(
    const TalentGraph& graph,
    const AstrolabeComponent& comp,
    uint32_t nodeId,
    int requiredAffinity = nullptr  // OUT: 若 TierLocked，返回需求值
);
```

#### 5.2.2 UIAstrolabe 显示提示

```cpp
// UIAstrolabe.cpp - 添加静态成员
static std::string s_failMessage;
static float s_failMessageTimer = 0.0f;

// 在节点点击时
if (hoveredNode && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && astroComp && s_alpha > 0.9f) {
    int required = 0;
    auto reason = AstrolabeSystem::tryUnlockNode(graph, *astroComp, hoverId, &required);
    
    if (reason == UnlockFailReason::Success) {
        AstrolabeSystem::addPointToNode(registry, player, graph, hoverId);
    } else {
        switch(reason) {
            case UnlockFailReason::NoPoints:
                s_failMessage = "星尘不足!";
                break;
            case UnlockFailReason::TierLocked:
                s_failMessage = TextFormat("需要 %d 点亲和度 (当前: %d)", 
                    required, astroComp->getAffinity(hoveredNode->profession));
                break;
            case UnlockFailReason::CoreSealed:
                s_failMessage = "核心节点需先立下誓约!";
                break;
            case UnlockFailReason::MaxPointsReached:
                s_failMessage = "节点已达上限!";
                break;
            default:
                s_failMessage = "";
        }
        s_failMessageTimer = 2.0f;  // 显示 2 秒
    }
}

// 渲染失败提示
if (s_failMessageTimer > 0.0f) {
    s_failMessageTimer -= dt;
    float msgAlpha = std::min(1.0f, s_failMessageTimer);
    
    Vector2 textSize = MeasureTextEx(font, s_failMessage.c_str(), 24, 1);
    float x = (screenWidth - textSize.x) / 2;
    float y = screenHeight * 0.7f;
    
    DrawRectangleRec({x - 20, y - 10, textSize.x + 40, textSize.y + 20}, 
                     Fade(MAROON, 0.8f * msgAlpha * s_alpha));
    DrawTextEx(font, s_failMessage.c_str(), {x, y}, 24, 1, Fade(WHITE, msgAlpha * s_alpha));
}
```

### 5.3 验收标准

- [ ] **AC-FAIL-1**: 星尘不足时显示"星尘不足!"
- [ ] **AC-FAIL-2**: Tier 锁定时显示亲和度需求
- [ ] **AC-FAIL-3**: Core 封印时显示誓约提示
- [ ] **AC-FAIL-4**: 提示 2 秒后自动消失

---

## 6. FIX-5: GPU 节点特效 Shader (P3)

### 6.1 问题描述

Spec 要求节点使用 GPU Shader 渲染脉动、封印纹理等效果，当前使用 CPU `DrawCircle`。

### 6.2 技术方案

#### 6.2.1 创建 Shader 文件

```glsl
// assets/shaders/talent_node.fs
#version 430 core

uniform float u_time;
uniform int u_status;       // 0=Locked, 1=Available, 2=Activated, 3=FullyActivated, 4=Sealed
uniform float u_progress;   // 0.0 ~ 1.0
uniform vec4 u_baseColor;
uniform vec2 u_center;
uniform float u_radius;

out vec4 FragColor;

void main() {
    vec2 uv = (gl_FragCoord.xy - u_center) / u_radius;
    float dist = length(uv);
    
    // Discard outside circle
    if (dist > 1.0) discard;
    
    vec4 color = u_baseColor;
    float glow = 0.0;
    float alpha = 1.0;
    
    // Soft edge
    alpha *= smoothstep(1.0, 0.85, dist);
    
    if (u_status == 1) { // Available - Amber pulse
        float pulse = sin(u_time * 3.0) * 0.3 + 0.7;
        glow = pulse * smoothstep(1.0, 0.5, dist) * 0.5;
        color = mix(color, vec4(1.0, 0.84, 0.0, 1.0), 0.5);
    }
    else if (u_status >= 2 && u_status <= 3) { // Activated
        // Progress ring
        float angle = atan(uv.y, uv.x);
        float normalizedAngle = (angle + 3.14159) / (2.0 * 3.14159);
        if (normalizedAngle < u_progress && dist > 0.7 && dist < 0.9) {
            color = vec4(0.5, 0.8, 1.0, 1.0);  // Sky blue
        }
        
        if (u_status == 3) { // FullyActivated - Golden rays
            float rays = abs(sin(atan(uv.y, uv.x) * 6.0 + u_time * 2.0));
            glow = rays * 0.3 * smoothstep(1.0, 0.3, dist);
            color = mix(color, vec4(1.0, 0.84, 0.0, 1.0), 0.6);
        }
    }
    else if (u_status == 4) { // Sealed - Purple seal pattern
        float seal = fract(sin(dot(uv * 4.0, vec2(12.9898, 78.233))) * 43758.5453);
        float sealMask = step(0.7, seal) * 0.3;
        color = mix(vec4(0.5, 0.0, 0.5, 1.0), vec4(0.2, 0.0, 0.3, 1.0), sealMask);
        
        // Animated seal lines
        float lines = abs(sin((uv.x + uv.y) * 10.0 + u_time));
        color.rgb += vec3(0.2, 0.0, 0.3) * lines * 0.3;
    }
    
    FragColor = vec4(color.rgb + vec3(glow), color.a * alpha);
}
```

### 6.3 验收标准

- [ ] **AC-GPU-1**: 创建 `talent_node.fs`
- [ ] **AC-GPU-2**: Available 节点有脉动效果
- [ ] **AC-GPU-3**: Sealed 节点有封印纹理
- [ ] **AC-GPU-4**: FullyActivated 有金色光芒

---

## 7. FIX-6: 冗余 API 清理 (P3)

### 7.1 问题描述

`AstrolabeSystem::applyTalentStats()` 定义但无调用者。

### 7.2 技术方案

移除函数或添加明确的 `[[deprecated]]` 标注：

```cpp
// 选项 A: 移除
// 从 AstrolabeSystem.hpp 和 .cpp 中删除 applyTalentStats

// 选项 B: 标注为备用 API
[[deprecated("Use AttributePipeline::Calculate directly. This is reserved for future manual refresh.")]]
static void applyTalentStats(entt::registry& registry, entt::entity player, const TalentGraph& graph);
```

### 7.3 验收标准

- [ ] **AC-API-1**: 移除或标注 `applyTalentStats`

---

## 8. 文件变更清单

| 文件 | 变更类型 | FIX |
|------|----------|-----|
| `src/game/systems/ui/UIAstrolabe.hpp` | MODIFY | FIX-1, FIX-4 |
| `src/game/systems/ui/UIAstrolabe.cpp` | MODIFY | FIX-1, FIX-4 |
| `src/game/systems/skill/AstrolabeSystem.hpp` | MODIFY | FIX-4, FIX-6 |
| `src/game/systems/skill/AstrolabeSystem.cpp` | MODIFY | FIX-4, FIX-6 |
| `tests/unit/TalentLayoutTests.cpp` | MODIFY | FIX-2 |
| `tests/unit/AstrolabeSystemTests.cpp` | CREATE | FIX-3 |
| `tests/CMakeLists.txt` | MODIFY | FIX-3 |
| `assets/shaders/talent_node.fs` | CREATE | FIX-5 |

---

## 9. 不可修改文件 (Forbidden)

- `src/game/data/TalentData.hpp` — 数据模型已稳定
- `src/game/components/Progression.hpp` — 组件结构已稳定
- `assets/data/profession_talents.json` — 数据内容已定稿

---

*Spec 版本: 1.0*
*最后更新: 2026-02-04*
