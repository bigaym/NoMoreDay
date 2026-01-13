# Portal System Implementation Plan

## Goal

实现完整的传送门机制，包括：

1. **回城传送门 (Town Portal)** - 玩家主动召唤，双向传送
2. **传送门可视化 (Portal VFX)** - 纯代码渲染，无需外部资源
3. **传送门选择 UI (Portal Selection)** - 3选1机制与维度拼接集成
4. **关卡出口 (Dungeon Exit)** - 自动生成下层传送门

---

## Proposed Changes

### Component: Portal Data Structures

#### [MODIFY] [PortalComponent](file:///f:/NoMoreDay/src/game/components/MapComponent.hpp)

扩展现有组件以支持更多传送门类型：

```cpp
enum class PortalType : uint8_t {
    Dungeon,      // 地下城入口/出口
    Town,         // 回城门
    Boss,         // BOSS房入口
    Return        // 返回门（双向）
};

struct PortalComponent {
    PortalType type = PortalType::Dungeon;
    std::string targetBiome;
    int targetLevel = 1;
    std::string targetEntranceId = "start";
    bool isOneWay = false;
    bool isActive = true;
    
    // For Town Portal (return functionality)
    std::string originBiome;      // 原始生物群系
    int originLevel = 0;          // 原始层数
    Vector2 originPosition = {0, 0}; // 原始位置
    
    // Visual
    float animationTimer = 0.0f;  // 用于渲染动画
    float radius = 30.0f;         // 视觉半径
};
```

---

#### [NEW] [TownPortalSkill.hpp](file:///f:/NoMoreDay/src/game/components/TownPortalSkill.hpp)

回城技能状态组件：

```cpp
struct TownPortalCastingComponent {
    float castTime = 3.0f;        // 吟唱时间
    float elapsedTime = 0.0f;     // 已过时间
    bool isCasting = false;
    Vector2 castPosition = {0, 0}; // 开始施法的位置
};
```

---

### System: Town Portal Logic

#### [MODIFY] [PortalSystem.hpp](file:///f:/NoMoreDay/src/game/systems/world/PortalSystem.hpp)

添加回城门施法和渲染功能：

```cpp
class PortalSystem {
public:
    PortalSystem(NoMoreDay::SceneManager& sceneManager);
    
    void Update(entt::registry& registry, float dt);
    void Render(entt::registry& registry, const Camera2D& camera); // NEW
    
    // Town Portal API
    static void StartTownPortalCast(entt::registry& registry, entt::entity caster);
    static void CancelTownPortalCast(entt::registry& registry, entt::entity caster);
    
private:
    void UpdateTownPortalCasting(entt::registry& registry, float dt);
    void SpawnTownPortal(entt::registry& registry, entt::entity caster);
    void RenderPortalEffect(const Position& pos, PortalType type, float animTimer);
    
    NoMoreDay::SceneManager& m_sceneManager;
};
```

---

#### [MODIFY] [PortalSystem.cpp](file:///f:/NoMoreDay/src/game/systems/world/PortalSystem.cpp)

实现以下功能：

1. **施法逻辑**：检测 `KEY_T` 输入，开始 3 秒吟唱
2. **中断机制**：移动或受击时取消吟唱
3. **生成回城门**：吟唱完成后创建 `PortalComponent` 实体
4. **双向传送**：进入城镇后，在城镇生成返回门

---

### System: Portal Visual Effects

#### [MODIFY] [PortalSystem.cpp](file:///f:/NoMoreDay/src/game/systems/world/PortalSystem.cpp)

纯代码渲染传送门效果：

```cpp
void PortalSystem::RenderPortalEffect(const Position& pos, PortalType type, float animTimer) {
    // 1. 外圈光环 (DrawRing)
    Color ringColor = (type == PortalType::Town) ? GOLD : PURPLE;
    float pulse = 1.0f + 0.1f * sinf(animTimer * 4.0f);
    DrawRing({pos.x, pos.y}, 25.0f * pulse, 30.0f * pulse, 0, 360, 32, ColorAlpha(ringColor, 0.6f));
    
    // 2. 内部旋涡 (GPU Particles)
    auto& particleSys = NoMoreDay::systems::GPUParticleSystem::Get();
    if (fmodf(animTimer, 0.05f) < 0.016f) { // 每0.05秒发射一次
        float angle = animTimer * 3.0f;
        for (int i = 0; i < 3; ++i) {
            components::GPUParticle p;
            float a = angle + i * (2.0f * PI / 3.0f);
            p.position = {pos.x + cosf(a) * 20.0f, pos.y + sinf(a) * 20.0f};
            p.velocity = {-sinf(a) * 30.0f, cosf(a) * 30.0f};
            p.color = ringColor;
            p.lifetime = 0.8f;
            p.maxLifetime = 0.8f;
            p.scale = 3.0f;
            particleSys.Emit(p);
        }
    }
    
    // 3. 中心光点
    DrawCircle((int)pos.x, (int)pos.y, 8.0f, ColorAlpha(WHITE, 0.8f));
}
```

---

### System: Input Binding

#### [MODIFY] [GameplayState.cpp](file:///f:/NoMoreDay/src/game/states/GameplayState.cpp)

添加 `KEY_T` 绑定：

```cpp
// In OnUpdate():
if (IsKeyPressed(KEY_T) && !UISystem::State.isAnyPanelOpen()) {
    PortalSystem::StartTownPortalCast(m_context->registry, playerEntity);
}
```

---

### System: Scene Manager Integration

#### [MODIFY] [SceneManager.cpp](file:///f:/NoMoreDay/src/engine/scene/SceneManager.cpp)

在城镇生成返回门：

```cpp
void SceneManager::ApplyLoadedLevel() {
    // ... existing code ...
    
    // If transitioning FROM dungeon TO town, spawn return portal
    if (m_targetBiome == "town" && m_originBiome != "town") {
        auto returnPortal = m_registry.create();
        m_registry.emplace<LocalLevelTag>(returnPortal);
        m_registry.emplace<Position>(returnPortal, spawnX + 30.0f, spawnY);
        
        PortalComponent pc;
        pc.type = PortalType::Return;
        pc.targetBiome = m_originBiome;
        pc.targetLevel = m_originLevel;
        pc.originPosition = m_originPosition;
        pc.isActive = true;
        m_registry.emplace<PortalComponent>(returnPortal, pc);
    }
}
```

需要在 `SceneManager` 中添加：

- `m_originBiome`, `m_originLevel`, `m_originPosition` 成员变量
- 在 `RequestTransition` 时保存当前状态

---

## Verification Plan

### Automated Tests

目前没有发现针对 `PortalSystem` 的现有单元测试。建议新增以下测试：

#### [NEW] TestPortalSystem.cpp

测试文件位置：`tests/TestPortalSystem.cpp`

```cpp
// Test 1: Town portal casting starts correctly
TEST_CASE("Town Portal casting starts") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 100.0f, 100.0f);
    
    PortalSystem::StartTownPortalCast(registry, player);
    
    auto* casting = registry.try_get<TownPortalCastingComponent>(player);
    REQUIRE(casting != nullptr);
    REQUIRE(casting->isCasting == true);
}

// Test 2: Town portal spawns after cast completes
TEST_CASE("Town Portal spawns after cast") {
    // ... simulate 3 seconds of update ...
}
```

**运行命令**：

```powershell
cd f:\NoMoreDay\build\bin\tests
.\tests_runner.exe --run-portal-tests
```

---

### Manual Verification

由于传送门涉及视觉效果和场景切换，需要手动验证：

#### 测试步骤 1：回城门施法

1. 启动游戏：`.\build\bin\NoMoreDay.exe`
2. 进入 Gameplay 状态（开始新游戏）
3. 按下 `T` 键
4. **预期结果**：
   - 玩家脚下出现施法光圈
   - 3 秒后生成金色传送门
   - 玩家移动时施法被中断

#### 测试步骤 2：传送门交互

1. 走近生成的回城门（距离 < 20 单位）
2. **预期结果**：
   - 淡出过渡动画
   - 加载城镇地图
   - 城镇中玩家脚边有返回门

#### 测试步骤 3：返回门功能

1. 在城镇中走近返回门
2. **预期结果**：
   - 返回原来的地下城
   - 玩家出现在原始位置附近

#### 测试步骤 4：视觉效果

1. 观察传送门渲染
2. **预期结果**：
   - 光环有脉冲动画
   - 粒子绕圈旋转
   - 无外部资源加载错误

---

## Implementation Order

1. **Phase 1** (约 2 小时)：
   - 扩展 `PortalComponent`
   - 添加 `TownPortalCastingComponent`
   - 实现 `KEY_T` 绑定和施法逻辑

2. **Phase 2** (约 1.5 小时)：
   - 实现 `RenderPortalEffect` 纯代码渲染
   - 集成到 `GameplayState::OnRender`

3. **Phase 3** (约 1 小时)：
   - 修改 `SceneManager` 保存 origin 状态
   - 在城镇生成返回门

4. **Phase 4** (约 1 小时)：
   - 编写测试用例
   - 手动验证全流程