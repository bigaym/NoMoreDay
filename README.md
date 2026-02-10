# NoMoreDay

**NoMoreDay** 是一款基于 **C++20** 和 **ECS (Entity Component System)** 架构开发的高性能 **2D 暗黑Like (Diablo-like) Roguelite ARPG**。
项目采用 **数据导向设计 (Data-Oriented Design)** 与 **GPGPU 加速** 技术，在普通硬件上实现了同屏 **10,000+** 活跃实体的实时战斗，呈现极致的末日压迫感与深度的装备驱动体验。

---

## 🌌 游戏愿景 (Vision)

- **风格**: 结合了东方修真与西方末日废土的 **“赛博仙侠 (Cyber Cultivation)”** 风格。
- **核心体验**:
    - **海量尸潮**: 利用 GPU 实体渲染 (MDI)、Boids 群集算法与 GPU 流场寻路 (Flow Field)，模拟万级怪物的流体般涌动。
    - **深度构建 (Build)**: 结合暗黑类的词缀系统、技能进化树 (Skill Spec)、进阶专精 (Masteries) 与虚空星盘 (Astrolabe)。
    - **硬核战斗**: 双摇杆射击 (Twin-Stick) 操作，强调高频走位、技能形态选择与资源循环管理。

## 🛠 技术栈 (Tech Stack)

项目采用高性能混合架构，核心逻辑与表现层严格分离。

| 模块 | 选型 | 说明 |
| :--- | :--- | :--- |
| **语言标准** | **C++20** | 广泛使用 Concepts (约束)、`constexpr` 与高效异步模型。 |
| **ECS 框架** | **EnTT** | 业界领先的 C++ ECS 库，保证数据局部性 (Cache Locality)。 |
| **渲染后端** | **Raylib + OpenGL** | 使用 MDI (Multi-Draw Indirect) 与 GPU Instancing 进行海量渲染。 |
| **并发调度** | **Taskflow** | 基于 DAG 的多线程任务流水线，优化物理、AI 与逻辑更新。 |
| **SIMD 加速** | **xsimd** | 向量化数学运算，支撑 `SIMDSpatialGrid` 高效碰撞检测。 |
| **物理引擎** | **自定义解算器** | 针对海量实体的简化物理模型，支持流体碰撞与群集避障。 |
| **内存管理** | **mimalloc** | 优化高频内存分配，主循环逻辑实现 **零堆分配**。 |
| **数据系统** | **nlohmann/json** | 全数据驱动配置 (技能、物品、生物群系、掉落)。 |

## 🏗 架构概览 (Architecture)

### 核心系统 (Core Infrastructure)
*   **状态管理 (StateManager)**: 基于栈的多状态管理，支持无缝切场与 UI 动态覆盖。
*   **GPU 渲染管线**:
    *   **MDIRenderer**: 通过 Multi-Draw Indirect 减少 Draw Call，大幅提升绘制效率。
    *   **GPUParticleSystem**: Compute Shader 驱动的 20万级粒子系统。
    *   **GPUSkillEffectSystem**: 高性能技能视觉管线，支持各种复杂的剑气与特效。
*   **物理与寻路**:
    *   **SIMDSpatialGrid**: 基于空间网格的 SIMD 加速碰撞检测。
    *   **GPUFlowFieldSystem**: 万级单位共享的流场寻路，支持动态避障。

### 核心玩法系统
*   **战斗流水线 (Damage Pipeline)**: 严谨的 5 步伤害计算（基础 -> 转换 -> 增伤 -> 独立乘区 -> 结算）。
*   **职业与成长**:
    *   **剑修 (Sword Cultivator)**: 当前核心职业，拥有 9 种基础剑法及 3 种进阶专精。
    *   **技能进化树 (Skill Spec)**: 深度技能改造，改变技能形态、Tags 及元素属性。
    - **虚空星盘 (Astrolabe)**: 账号级共享的全局天赋网，跨角色属性集成。
*   **物品与装备**:
    *   **词缀系统 (Affix)**: 支持前缀、后缀、传奇词缀 (Legendary) 与融合机制 (LP)。
    *   **维度拼接 (Mosaic)**: 独特的地图生成机制，通过收集地图碎片拼接 3x3 关卡。
    *   **符文之语 (Runeword)**: 深度装备自定义，通过组合符文解锁隐藏属性。
*   **世界系统**:
    *   **生物群系 (Biome System)**: 支持 27 种不同风格的群落，拥有专属怪物池、环境效果与特殊物理机制。
    *   **宿敌进化 (Nemesis)**: 击败玩家的怪物理会进化，获得独特的抗性与反制 AI。

## 📂 目录结构 (Project Structure)

```text
NoMoreDay/
├── assets/                 # 游戏资源
│   ├── data/              # 核心配置文件 (JSON: 技能、装备、群系、掉落表)
│   ├── shaders/           # 渲染脚本 (Compute/Vertex/Fragment GLSL)
│   ├── textures/          # 贴图与纹理集
│   └── generated/         # 自动生成的资源占位符与导出物
├── src/                    # C++ 源代码
│   ├── app/               # 应用入口、主循环、全局上下文 (Game/Settings)
│   ├── core/              # 底层工具 (Logging, Math, Threading, SIMD Utils)
│   ├── engine/            # 引擎模块 (Input, Resource, Scene, Physics, Render)
│   └── game/              # 游戏业务逻辑
│       ├── components/    # ECS 组件定义 (POD 数据)
│       ├── data/          # 注册表与静态数据定义 (Registry)
│       └── systems/       # ECS 系统实现 (Combat, Item, AI, Skill, World...)
├── scripts/                # 工具脚本 (Python: 资源管线、数据生成、翻译)
├── conductor/              # 自动化开发追踪与计划
├── 设计文档/               # 游戏策划与技术实现规格书
└── CMakeLists.txt          # 跨平台构建配置
```

## 🚀 构建与运行 (Build & Run)

### 依赖要求
- **C++20** (推荐 MSVC 19.30+ 或 GCC 11+)
- **CMake 3.20+**
- **Vulkan/OpenGL 4.3+** 支持 (Compute Shader 必需)

### Windows (推荐)
```powershell
.\build.bat
```

### 跨平台手动构建
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```


*NoMoreDay - 2026. Designed for performance, built for darkness.*
