# Modular Architecture Split: exe + lib + dll

## 1. Problem Summary

当前 `NoMoreDayCore` 是一个**单体静态库**，包含 `src/` 下除 `main.cpp` 和 `SkillBehaviors` 外的所有代码。

### 1.1 三大痛点

| 痛点 | 根因 | 影响 |
|------|------|------|
| **Header 级联重编** | `game/components/Common.hpp` 被 engine/ 和 game/ 同时 include | 修改任何常用组件触发 300+ 文件重编 |
| **engine/game 双向耦合** | engine/render 包含 game/components, game/ 又包含 engine/render | 无法独立编译任一层 |
| **链接时间长** | 所有代码在一个巨型 lib 中链接 | 每次修改都要重链整个 lib |

### 1.2 关键耦合链

```
# engine/ → game/ (HEADER 级，触发级联重编)
engine/render/GPUEntitySystem.hpp → game/components/Common.hpp
engine/render/UIRenderer.hpp      → game/components/ItemComponent.hpp
engine/render/UIRenderer.hpp      → game/systems/ui/UIContext.hpp
engine/scene/SceneManager.hpp     → game/systems/world/LevelManager.hpp
engine/physics/SpatialGrid.hpp    → game/components/Common.hpp
engine/persistence/SaveManager.hpp → game/data/SaveData.hpp

# game/ → engine/ (~92 处引用)
game/systems/skill/SkillSystem.cpp → engine/render/GPUParticleSystem.hpp
game/systems/combat/CombatSystem.cpp → engine/render/RenderSystem.hpp
game/states/*.hpp → engine/scene/State.hpp

# core/ → game/ (1 处违规)
core/math/PhysicsUtils.hpp → game/components/Common.hpp
```

---

## 2. 目标架构

### 2.1 最终目标（Step 3 完成后）

```
NoMoreDay.exe                      # [EXE] 仅 main.cpp
  └── NoMoreDayApp.lib             # [LIB] Game, SharedContext, Settings
       └── NoMoreDayGame.lib       # [LIB] 游戏逻辑 (变化最频繁)
            └── NoMoreDayEngine.lib # [LIB] 引擎层 (render/physics/scene/input/vfx/persistence)
                 └── NoMoreDayCore.dll  # [DLL] 稳定层 (core/audio/resource)
                      └── NoMoreDayTypes.lib  # [LIB] 公共类型 (纯 header)
                           └── third_party/ (静态链接)
```

依赖方向严格单向：`Types → Core → Engine → Game → App → Exe`

### 2.2 库职责边界

| 目标 | 类型 | 内容 | 变动频率 | 大小估计 |
|------|------|------|---------|---------|
| `NoMoreDayTypes` | static lib | 基础 ECS 组件(Position/Velocity)、常量、枚举、基础接口抽象 | ⭐ 极低 | 纯 header |
| `NoMoreDayCore` | **DLL** | core/(logging/math/threading/utils), engine/audio, engine/resource | ⭐ 极低 | ~30 .cpp |
| `NoMoreDayEngine` | static lib | engine/render, engine/physics, engine/scene, engine/input, engine/vfx, engine/persistence | ⭐⭐ 中 | ~120 .cpp |
| `NoMoreDayGame` | static lib | game/ 全部(components/systems/states/data/combat_v2) | ⭐⭐⭐ 高 | ~200 .cpp |
| `NoMoreDayApp` | static lib | app/Game, app/SharedContext, app/Settings | ⭐⭐ 中 | 3 .cpp |
| `SkillBehaviors` | OBJECT lib | (保持现状) game/systems/skill/behaviors/* | ⭐⭐ 中 | 21 .cpp |
| `NoMoreDay` | **EXE** | src/app/main.cpp | ⭐ 极低 | 1 .cpp |

---

## 3. Step 1: 公共类型提取（消除 engine→game 头文件依赖）

### 3.1 新增目录 `src/types/`

从 `game/components/Common.hpp` 中提取出所有**被 engine 层依赖的类型**：

```
src/types/
├── CommonComponents.hpp    # Position, Velocity, Rotation, PrevPosition
│                           # SpriteComponent, PlayerTag, PhaseTag
│                           # ColorComponent, HealthComponent
│                           # InputComponent, WeaponComponent
│                           # GPUIndex, Radius, ColliderType, ColliderComponent
│                           # KilledTag, PersistentTag, LocalLevelTag
│                           # DormantTag, DirtyTransform, LootTag
│                           # BarrierComponent, LabelCacheComponent
│                           # MovementAccumulator, VisionComponent
│                           # GoldComponent, TextureIDComponent
│                           # IDComponent, DelayedDestroyComponent
├── GameConstants.hpp       # 所有 NoMoreDay::Constants::* 命名空间
│                           # 包括 World, Generator, AI, Combat, Items
│                           # Enemy, Item, Astrolabe, Skill, Render, Visuals
│                           # Movement, Physics, StashConfig
├── BasicTypes.hpp          # BiomeStyle, BiomeFeature, BiomeID 枚举
│                           # ToBiomeFeatureMask / HasBiomeFeature 辅助
```

### 3.2 变更清单

#### 修改的文件

| 文件 | 变更 |
|------|------|
| `src/game/components/Common.hpp` | 删除提取的内容，保留 game-specific 组件。改为 `#include "types/CommonComponents.hpp"` 和 `#include "types/GameConstants.hpp"` |
| `src/core/math/PhysicsUtils.hpp` | 将 `#include "game/components/Common.hpp"` 替换为 `#include "types/CommonComponents.hpp"` |
| `src/engine/render/GPUEntitySystem.hpp` | 将 `#include "game/components/Common.hpp"` 替换为 `#include "types/CommonComponents.hpp"` |
| `src/engine/render/UIRenderer.hpp` | 保留 `ItemComponent.hpp` 和 `Buff.hpp`（game-specific），但 `UIContext.hpp` 可考虑提取颜色常量到 `types/` |
| `src/engine/render/GPUSkillEffectSystem.hpp` | 将 `#include "game/components/SkillVfxEvent.hpp"` 替换为 `#include "types/CommonComponents.hpp"`（如果只需要公共类型） |
| `src/engine/physics/SpatialGrid.hpp` | 将 `#include "game/components/Common.hpp"` 替换为 `#include "types/CommonComponents.hpp"` |
| `src/engine/scene/SceneManager.hpp` | 保留 `LevelManager.hpp`（game-specific），但 `MosaicData.hpp` 可能需要抽象接口 |
| `src/pch.hpp` | 将 `game/components/Common.hpp` 替换为 `types/CommonComponents.hpp` |
| `src/CMakeLists.txt`（如有局部 PCH）| 同步变更 |

#### 无需变更的文件

- `engine/input/InputSystem.cpp` — 只在 .cpp 中包含 game/，不受影响
- `engine/persistence/SaveManager.cpp` — 同上
- 所有 `game/*` 文件 — 通过 `Common.hpp` 间接包含 `types/`，无需改动

### 3.3 效果

```
# 变更前: 修改 game/components/Common.hpp
game/components/Common.hpp → engine/render/GPUEntitySystem.hpp (强制重编)
                           → engine/physics/SpatialGrid.hpp (强制重编)
                           → engine/render/GPUSkillEffectSystem.hpp (强制重编)
                           → pch.hpp (强制全量重编)

# 变更后: 修改 game/components/Common.hpp
game/components/Common.hpp → 仅 game/ 内部文件重编
types/CommonComponents.hpp → 仅当 types/ 变更时才触发重编
```

---

## 4. Step 2: 多静态库拆分

### 4.1 拆分后依赖图

```
NoMoreDay.exe
  └── NoMoreDayApp.lib [PRIVATE]
       └── NoMoreDayGame.lib [PUBLIC]
            ├── NoMoreDayEngine.lib [PUBLIC]
            │    ├── NoMoreDayCore.lib [PUBLIC]
            │    │    └── NoMoreDayTypes.lib [PUBLIC]
            │    └── third_party (raylib, glfw, spdlog, EnTT, Taskflow, xsimd)
            └── SkillBehaviors.obj [PUBLIC]
                 └── (same third_party + NoMoreDayTypes)
```

### 4.2 CMakeLists.txt 变更

#### 源文件分配逻辑

```cmake
# === TYPES (header-only static lib) ===
add_library(NoMoreDayTypes INTERFACE)
target_include_directories(NoMoreDayTypes INTERFACE src/types)

# === CORE (stable layer) ===
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    "src/core/*.cpp"
    "src/engine/audio/*.cpp"
    "src/engine/resource/*.cpp"
)
add_library(NoMoreDayCore STATIC ${CORE_SOURCES})
target_link_libraries(NoMoreDayCore
    PUBLIC NoMoreDayTypes raylib spdlog::spdlog
    PRIVATE NoMoreDayAnalyzeFlags
)
target_include_directories(NoMoreDayCore PUBLIC src)
# ... PCH, 编译选项同现状

# === ENGINE (render/physics/scene/input/vfx/persistence) ===
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "src/engine/render/*.cpp"
    "src/engine/physics/*.cpp"
    "src/engine/scene/*.cpp"
    "src/engine/input/*.cpp"
    "src/engine/vfx/*.cpp"
    "src/engine/persistence/*.cpp"
)
add_library(NoMoreDayEngine STATIC ${ENGINE_SOURCES})
target_link_libraries(NoMoreDayEngine
    PUBLIC NoMoreDayCore NoMoreDayTypes
    PRIVATE NoMoreDayAnalyzeFlags
)

# === GAME (game-specific logic) ===
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "src/game/*.cpp"
)
# 排除 SkillBehaviors
foreach(SKILL_SRC ${SKILL_BEHAVIORS_SOURCES})
    list(REMOVE_ITEM GAME_SOURCES ${SKILL_SRC})
endforeach()
add_library(NoMoreDayGame STATIC ${GAME_SOURCES})
target_link_libraries(NoMoreDayGame
    PUBLIC NoMoreDayEngine NoMoreDayCore NoMoreDayTypes SkillBehaviors
    PRIVATE NoMoreDayAnalyzeFlags
)

# === APP (orchestration) ===
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "src/app/*.cpp"
)
list(REMOVE_ITEM APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/app/main.cpp")
add_library(NoMoreDayApp STATIC ${APP_SOURCES})
target_link_libraries(NoMoreDayApp
    PUBLIC NoMoreDayGame NoMoreDayEngine NoMoreDayCore NoMoreDayTypes
    PRIVATE NoMoreDayAnalyzeFlags
)

# === EXE ===
add_executable(NoMoreDay src/app/main.cpp)
target_link_libraries(NoMoreDay PRIVATE NoMoreDayApp NoMoreDayAnalyzeFlags)
```

#### 编译选项传播策略

```cmake
# 编译选项不再全部放在 NoMoreDayCore 上
# 改为按需分配:

# 给所有 lib 统一设置
foreach(TARGET IN ITEMS NoMoreDayTypes NoMoreDayCore NoMoreDayEngine NoMoreDayGame NoMoreDayApp)
    target_compile_options(${TARGET} PUBLIC /arch:AVX2 /Oi /Ot /Ob2 /GF)
    target_compile_definitions(${TARGET} PUBLIC WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
    target_compile_definitions(${TARGET} PUBLIC GRAPHICS_API_OPENGL_43)
endforeach()
```

### 4.3 各 lib 编译开销估计

| 模块 | .cpp 数量 | 典型重编时间 | 链接产物大小 |
|------|-----------|-------------|-------------|
| `NoMoreDayTypes` | 0 (header) | 0s | ~0 |
| `NoMoreDayCore` | ~30 | ~5s | ~5MB .lib |
| `NoMoreDayEngine` | ~120 | ~20s | ~40MB .lib |
| `NoMoreDayGame` | ~200 | ~30s | ~60MB .lib |
| `NoMoreDayApp` | ~3 | ~1s | ~1MB .lib |
| `NoMoreDay.exe` | 1 | ~0.5s | ~1MB .exe |

**编译加速场景对比**：

| 场景 | 当前 (monolithic) | Step 2 后 | 加速比 |
|------|------------------|-----------|--------|
| 修改 game/ 一个 .cpp | 编译 1 个文件 + 重链全部 | 编译 1 个文件 + 重链 Game.lib + 重链 exe | ~2x |
| 修改 game/ 一个 .hpp | 级联重编 engine + game + 重链全部 | 仅重编 game/ 的文件 + 重链 Game.lib + exe | ~5-10x |
| 修改 engine/render/ | 编译 1 个文件 + 重链全部 | 编译 1 个文件 + 重链 Engine.lib + Game.lib + App.lib + exe | ~1.5x |
| 修改 types/ | 全量重编 | 所有 lib 增量重编 | ~2-3x |

---

## 5. Step 3: 稳定层 DLL 化

### 5.1 选择 NoMoreDayCore 做 DLL 的理由

| 条件 | 判断 |
|------|------|
| **变动频率低** | core/工具/数学、engine/audio、engine/resource 几乎不变 |
| **内部依赖少** | 只依赖 types/ 和 third_party，不依赖 engine/game |
| **体积适中** | ~30 .cpp，DLL 管理开销合理 |
| **接口稳定** | Logger、ResourceManager、AudioSystem 接口已稳定 |
| **多消费者** | 被 engine、game、app、tests 共享 |

### 5.2 DLL 导出宏

```cpp
// src/types/Export.hpp (新建)
#pragma once

#ifdef NMD_CORE_EXPORTS
    #define NMD_CORE_API __declspec(dllexport)
#else
    #define NMD_CORE_API __declspec(dllimport)
#endif
```

在 `NoMoreDayCore` 的 PUBLIC 接口上标记：

```cpp
// core/logging/Logger.hpp
#include "types/Export.hpp"

class NMD_CORE_API Logger {
    static void Init();
    static void Shutdown();
    // ...
};

// engine/resource/ResourceManager.hpp
class NMD_CORE_API ResourceManager {
    // ...
};

// engine/audio/AudioSystem.hpp
class NMD_CORE_API AudioSystem {
    // ...
};
```

### 5.3 DLL 构建 CMake 变更

```cmake
# NoMoreDayCore 改为 SHARED
add_library(NoMoreDayCore SHARED ${CORE_SOURCES})
target_compile_definitions(NoMoreDayCore PRIVATE NMD_CORE_EXPORTS)

# 链接 DLL 时需要 .lib 导入库
# NoMoreDayCore 编译输出:
#   bin/NoMoreDayCore.dll   → 运行时加载
#   bin/NoMoreDayCore.lib   → 导入库 (给消费者链接)
#   bin/NoMoreDayCore.pdb   → 调试符号
```

其他 lib 无需改动——它们通过 `target_link_libraries(NoMoreDayCore)` 自动使用导入库。

### 5.4 DLL 部署

```cmake
# 已在 build.bat 后期复制 DLL
# spdlog.dll 复制逻辑保持，增加:
add_custom_command(TARGET NoMoreDay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:NoMoreDayCore>
    "${OUTPUT_BIN_DIR}/"
    COMMENT "Copying NoMoreDayCore.dll to bin directory..."
)
```

### 5.5 注意事项

| 关注点 | 处理方式 |
|--------|---------|
| **STL 跨 DLL 边界** | MSVC 保证同 CRT 版本的 DLL 间 STL ABI 兼容。保持所有目标使用 `/MD`（动态 CRT） |
| **全局/静态变量** | DLL 和 EXE 各有独立实例。`Singleton` 模式需注意。建议迁移到依赖注入（SharedContext） |
| **异常跨边界** | MSVC 支持 `/EHsc` 下异常跨 DLL 边界。启用 `/EHsc` 即可 |
| **模板跨 DLL** | 模板在头文件中实例化，使用者自己实例化。无问题 |
| **虚函数跨 DLL** | 接口类保持在 header-only 的 types/ 中，virtual dtor 在 DLL 中实现即可 |

---

## 6. 接口抽象（可选强化）

对于 `engine/scene/SceneManager → LevelManager` 这种强耦合，可引入抽象接口进一步提升隔离性：

```cpp
// types/ILevelManager.hpp (在 types/ 中，无 game/ 依赖)
namespace NoMoreDay {
struct LevelTransitionInfo {};
class ILevelManager {
public:
    virtual ~ILevelManager() = default;
    virtual void LoadLevel(BiomeID biome, int level) = 0;
    virtual LevelTransitionInfo GetTransitionInfo() = 0;
};
}
```

```cpp
// game/systems/world/LevelManager.hpp
#include "types/ILevelManager.hpp"
class LevelManager : public NoMoreDay::ILevelManager { ... };
```

这样 `engine/scene/SceneManager` 可以只依赖 `ILevelManager` 接口，完全不依赖 `game/`。

---

## 7. 实施路线图

### 实施顺序（建议分支策略：每个 Step 一个独立分支）

```
Step 1: types-extraction
  ├── 创建 src/types/ 目录及文件
  ├── 修改所有引用位置
  ├── 验证编译通过 + 测试通过
  └── 合并

Step 2: multi-lib-split
  ├── 重构 CMakeLists.txt（新加 lib 目标）
  ├── 修改 build.bat（可选，保持兼容）
  ├── 调整 PCH 分配
  ├── 验证编译通过 + 测试通过
  └── 合并

Step 3: core-dll
  ├── 添加导出宏
  ├── 修改 Core 为 SHARED
  ├── 调整 DLL 部署
  ├── 验证运行时正确
  └── 合并
```

### 风险与回退

| 风险 | 概率 | 缓解措施 |
|------|------|---------|
| Step 2 后链接顺序问题 | 低 | CMake target_link_libraries 自动处理 |
| Step 3 后 DLL 加载失败 | 中 | 确保 PATH/部署目录正确，在 build.bat 中添加验证 |
| 跨 DLL 全局状态问题 | 中 | 审计所有 Singleton 模式，优先改为 SharedContext 注入 |
| 编译时间反而增加 | 低 | 增量编译更快，全量编译因并行而更快 |

---

## 8. 验收标准

1. **编译通过**: `build.bat` Debug/Release/RelWithDebInfo 全部通过
2. **测试通过**: `ctest -C Release` 全部通过
3. **运行时正常**: 游戏可启动、加载、进入关卡
4. **验证加速**: 修改 `game/systems/combat/CombatSystem.cpp` 后增量编译时间 < 当前时间 50%
5. **修改级联验证**: 修改 `game/components/Common.hpp` 后不应触发 engine/render/ 文件重编（Step 1 验收点）
