# NoMoreDay

**NoMoreDay** 是一款基于 **C++20** 和 **ECS (Entity Component System)** 架构开发的高性能 **2D 暗黑Like (Diablo-like) Roguelite ARPG**。
项目旨在通过数据导向设计 (Data-Oriented Design) 和 GPU 加速技术，在普通硬件上实现同屏 **10,000+** 单位的实时战斗，呈现末日尸潮的压迫感与深度的装备驱动体验。

---

## 🌌 游戏愿景 (Vision)

- **风格**: 末日 (Apocalyptic)、异界 (Other-worldly)、暗黑幻想 (Dark Fantasy)。
- **核心体验**:
    - **海量尸潮**: 利用 GPU 实体渲染、Boids 群集算法与 GPU 流场寻路 (Flow Field)，模拟成千上万怪物的流体般运动。
    - **深度构建 (Build)**: 结合暗黑类的装备词缀系统 (Affix System)、技能专精树与星盘天赋 (Astrolabe)。
    - **硬核战斗**: 双摇杆射击 (Twin-Stick) 操作，强调走位、技能释放时机与资源管理。

## 🛠 技术栈 (Tech Stack)

本项目采用 **混合架构 (Hybrid Architecture)**，核心为高性能 ECS。

| 模块 | 选型 | 说明 |
| :--- | :--- | :--- |
| **语言标准** | **C++20** | Modules, Concepts, Coroutines. |
| **ECS 框架** | **EnTT** | 业界最快的 C++ ECS 库，保证数据局部性 (Cache Locality)。 |
| **渲染后端** | **Raylib** | 轻量级 OpenGL 抽象，结合自定义 `rlgl` 指令进行 Compute Shader 和 Instancing 渲染。 |
| **并发调度** | **Taskflow** | 基于 DAG 的任务并行化，用于物理和逻辑更新。 |
| **SIMD 加速** | **xsimd** | 向量化数学运算，用于物理碰撞检测。 |
| **内存管理** | **mimalloc** | 微软高性能内存分配器，优化多线程内存分配。 |
| **数据序列化** | **nlohmann/json** | 灵活的 JSON 解析，用于数据驱动的配置。 |
| **日志系统** | **spdlog** | 快速、异步的 C++ 日志库。 |

## 🏗 架构概览 (Architecture)

### 核心系统
*   **ECS 架构**: 游戏逻辑完全基于 EnTT，组件 (Components) 为纯数据 (POD)，系统 (Systems) 负责逻辑处理。
*   **状态管理 (StateManager)**: 基于栈的游戏状态管理，支持状态叠加（如暂停菜单、UI 覆盖）。
*   **GPU 加速**:
    *   **GPUEntitySystem**: 使用 Compute Shader 处理数万实体的物理与渲染同步。
    *   **GPUFlowFieldSystem**: 基于 GPU 的流场寻路，支持海量单位的动态寻路。
    *   **GPUParticleSystem**: 高性能粒子特效系统。

### 玩法系统
*   **战斗系统**: 
    *   **DamagePipeline**: 五步伤害计算管道（基础 -> 转换 -> 增伤 -> 独立乘区 -> 结算）。
    *   **StatsSystem**: 复杂的属性计算，支持装备、Buff、天赋的动态修饰。
*   **物品与装备**:
    *   **ItemFactory**: 基于权重的随机掉落生成，支持词缀 (Prefix/Suffix) 和稀有度。
    *   **CraftingSystem**: 包含升级、混沌、洗练等功能的装备打造系统。
    *   **InventorySystem**: 支持背包整理、堆叠、装备槽位管理。
*   **成长系统**:
    *   **SkillSystem**: 主动技能、技能专精树 (Talent Tree)。
    *   **Astrolabe**: 类似 PoE 的星盘天赋系统，提供全局属性加成。

## 📂 目录结构 (Project Structure)

```text
NoMoreDay/
├── assets/                 # 游戏资源 (纹理、着色器、JSON配置)
│   ├── data/              # 数据表 (技能、掉落、生物群系等)
│   ├── shaders/           # GLSL 着色器 (Compute/Vertex/Fragment)
│   └── textures/          # 纹理素材
├── src/                    # C++ 源代码
│   ├── components/        # ECS 组件定义 (POD Structs)
│   ├── systems/           # ECS 系统逻辑 (Logic)
│   ├── core/              # 引擎基础设施 (Window, Resource, Input)
│   ├── states/            # 游戏状态 (Gameplay, Menu, Inventory)
│   └── tools/             # 工具类 (Logger, UUID)
├── scripts/                # Python 工具脚本 (资源管线)
├── conductor/              # 项目管理与开发追踪
├── 设计文档/               # 详细设计文档
└── CMakeLists.txt          # CMake 构建配置
```

## 构建要求

- C++20或更高版本
- CMake 3.10或更高版本
- 支持的平台: Windows, Linux, macOS

## 🚀 构建说明 (Build Instructions)

### Windows (快速构建)
```powershell
.\build.bat
```

### 手动构建 (CMake)
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## 项目状态

这是一个活跃开发中的项目，包含完整的游戏设计文档和开发计划。

## 🎮 控制说明 (Controls)

*   **移动**: `W`, `A`, `S`, `D` 或 鼠标左键点击
*   **冲刺**: `Shift` 或 `Space`
*   **技能**: `Q`, `W`, `E`, `R`, `鼠标右键`
*   **交互/拾取**: `F` (批量拾取), `鼠标左键` (点击物品)
*   **背包**: `I` 或 `Tab`
*   **角色面板**: `C`
*   **星盘**: `N`
*   **技能树**: `S`
*   **打造**: `K`
*   **暂停/菜单**: `ESC`

## 📝 开发规范 (Conventions)

*   **C++ Style**: 严格遵循 RAII，避免裸指针。组件必须是 POD 类型以最大化缓存命中率。
*   **Data-Driven**: 技能、怪物、掉落表等均通过 JSON 配置，便于策划和调整。
*   **Performance**: 核心循环中避免内存分配，使用对象池或预分配缓冲区。

## 许可证

请参阅项目中的许可证文件。

---

*Project NoMoreDay - 2026*