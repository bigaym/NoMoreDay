# NoMoreDay

**NoMoreDay** 是一款基于 **C++20** 开发的高性能 **2D 暗黑Like (Diablo-like) Roguelite ARPG**。
项目旨在通过数据导向设计 (Data-Oriented Design) 和 ECS 架构，在普通硬件上实现同屏 **10,000+** 单位的实时战斗，呈现末日尸潮的压迫感与深度的装备驱动体验。

---

## 🌌 游戏愿景 (Vision)

- **风格**: 末日 (Apocalyptic)、异界 (Other-worldly)、暗黑幻想 (Dark Fantasy)。
- **核心体验**:
    - **海量尸潮**: 利用 Boids 群集算法与空间哈希，模拟成千上万怪物的流体般运动。
    - **深度构建 (Build)**: 结合暗黑类的装备词缀系统与 Roguelite 的随机技能选择。
    - **硬核战斗**: 双摇杆射击 (Twin-Stick) 操作，强调走位与技能释放时机。

## ⚔️ 核心玩法 (Gameplay)

### 1. 战斗与探索
- **双摇杆操作**: WASD 移动，鼠标/右摇杆瞄准，快节奏的动作体验。
- **动态地图**: 基于改进型细胞自动机生成的随机地牢，配合战争迷雾系统。
- **怪物生态**: 7大种族（亡灵、异魔等），具备协同 AI（坦克阻挡、射手风筝、辅助治疗）。

### 2. 物品与成长 (Itemization & Progression)
- **装备驱动**: 
    - 丰富的词缀系统 (Prefix/Suffix)。
    - **稀有度分级**: 从普通到神话，包含独特的暗金装备 (Uniques)。
    - **工艺系统**: 符文之语 (Runewords) 与 锻造潜能 (Forging Potential)。
- **技能体系**: 武器决定主动技能，升级获取随机被动天赋 (Roguelite)。

### 3. 终局玩法 (Endgame)
- **避难所 (Sanctuary)**: 局外成长中心，升级铁匠铺、秘术师与仓库。
- **虚空星图 (The Void Atlas)**: 类似 PoE 的异界图鉴，通过增加“腐化值”挑战更高难度的地图词缀。

---

## 🛠 技术架构 (Technical Architecture)

本项目采用 **混合架构 (Hybrid Architecture)**，核心为高性能 ECS。

### 核心技术栈
| 模块 | 选型 | 说明 |
| :--- | :--- | :--- |
| **语言标准** | **C++20** | Modules, Concepts, Coroutines. |
| **ECS 框架** | **EnTT** | 业界最快的 C++ ECS 库，保证数据局部性。 |
| **空间索引** | **SpatialHashGrid** | 高效处理碰撞检测与邻居查询。 |
| **并发调度** | **Taskflow** | 基于 DAG 的任务并行化，榨干多核 CPU 性能。 |
| **渲染后端** | **Raylib** | 轻量级 OpenGL 抽象，支持 Instancing 批量渲染。 |
| **内存管理** | **mimalloc** | 微软高性能内存分配器，消除多线程锁竞争。 |
| **SIMD 加速** | **xsimd** | 向量化数学运算，用于物理碰撞检测。 |

### ✅ 已实现系统 (Implemented Systems)

- **核心交互**: `InputSystem`, `PhysicsSystem` (Grid-based), `RenderSystem`, `UISystem` (背包/小地图).
- **RPG 数值**: `StatsSystem` (属性/词缀), `ProgressionSystem` (升级), `InventorySystem`, `CraftingSystem`.
- **战斗与生存**: `CombatSystem` (伤害/抗性), `AISystem` (状态机/流场), `EffectSystem`.
- **世界与生态**: `MapSystem` (元胞自动机), `FogOfWarSystem`, `EnemySpawnSystem`, `DropSystem`.

---

## 📂 目录结构 (Project Structure)

```text
NoMoreDay/
├── assets/                 # 游戏资源 (纹理、音频、配置)
│   └── textures/           # 自动管线生成的透明背景素材
├── scripts/                # 工具脚本 (Python AI 资产生成管线)
├── 设计文档/               # 详细的游戏设计与架构文档
│   ├── 开发计划与任务追踪.md
│   ├── 战斗系统与属性设计.md
│   ├── 怪物和AI设计.md
│   └── ...
├── src/                    # C++ 源代码
│   ├── core/               # 基础设施 (Application, ResourceManager)
│   ├── components/         # ECS 组件定义 (POD Types)
│   ├── systems/            # ECS 系统逻辑 (无状态)
│   └── utils/              # 通用工具库
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

## 许可证

请参阅项目中的许可证文件。