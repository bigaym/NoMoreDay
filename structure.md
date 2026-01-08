# NoMoreDay 项目结构文档

## 项目概览

NoMoreDay 是一个基于 ECS (Entity-Component-System) 架构的 2D 游戏，采用 C++ 和 raylib 图形库开发。项目使用 CMake 构建系统，集成了 EnTT 实体组件系统、Taskflow 并行处理库等现代 C++ 技术。

## 项目结构

```
NoMoreDay/
├── .gitignore              # Git 忽略文件配置
├── AGENTS.md              # 代理开发指南
├── build.bat              # Windows 构建脚本
├── CMakeLists.txt         # CMake 构建配置文件
├── GEMINI.md              # Gemini 模型相关文档
├── LICENSE                # 项目许可证
├── raylib使用opengl4.3.md # Raylib OpenGL 4.3 使用说明
├── README.md              # 项目介绍文档
├── test_output.txt        # 测试输出文件
├── conductor/            # Conductor 工具配置
├── 设计文档/              # 游戏设计文档
├── assets/               # 游戏资源文件
├── scripts/              # Python 脚本工具
├── src/                  # 源代码目录
└── tests/                # 测试代码目录
```

## 源代码结构 (src/)

### 核心系统 (src/core/)

- **Game.cpp/Game.hpp** - 游戏主循环和核心管理
  - 功能：游戏主类，管理整个游戏循环、状态管理和系统初始化
  - 主要类：Game

- **State.hpp/State.cpp** - 游戏状态基类
  - 功能：定义游戏状态的接口，如菜单、游戏、暂停等状态
  - 主要类：State

- **StateManager.hpp/StateManager.cpp** - 游戏状态管理器
  - 功能：管理游戏状态的切换和堆栈
  - 主要类：StateManager

- **SharedContext.hpp** - 共享上下文
  - 功能：提供系统间共享的数据和资源访问
  - 主要类：SharedContext

- **ItemFactory.hpp** - 物品工厂
  - 功能：负责创建和初始化游戏中的物品
  - 主要类：ItemFactory

### 组件系统 (src/components/)

- **EffectComponent.hpp** - 效果组件
  - 功能：存储实体的效果相关数据，如增益、减益等
  - 主要类：EffectComponent

### 系统 (src/systems/)

- **AISystem.cpp/AISystem.hpp** - AI 系统
  - 功能：处理非玩家实体的智能行为和决策
  - 主要类：AISystem

- **AstrolabeSystem.cpp/AstrolabeSystem.hpp** - 星盘系统
  - 功能：处理星盘相关的游戏机制和界面
  - 主要类：AstrolabeSystem

- **CombatSystem.cpp/CombatSystem.hpp** - 战斗系统
  - 功能：处理游戏中的战斗逻辑，包括伤害计算、技能效果等
  - 主要类：CombatSystem

- **DropSystem.hpp** - 掉落系统
  - 功能：处理敌人死亡后的物品掉落逻辑
  - 主要类：DropSystem

- **EffectSystem.hpp** - 效果系统
  - 功能：处理各种效果的更新和应用
  - 主要类：EffectSystem

- **EnemyBehavior.cpp** - 敌人行为系统
  - 功能：处理特定敌人行为模式
  - 主要类：EnemyBehavior

- **GPUFlowFieldSystem.hpp** - GPU 流场系统
  - 功能：使用 GPU 计算流场，用于路径规划和移动
  - 主要类：GPUFlowFieldSystem

- **GPUParticleSystem.cpp/GPUParticleSystem.hpp** - GPU 粒子系统
  - 功能：使用 GPU 计算粒子效果
  - 主要类：GPUParticleSystem

- **InputSystem.cpp/InputSystem.hpp** - 输入系统
  - 功能：处理玩家输入事件
  - 主要类：InputSystem

- **InventorySystem.cpp/InventorySystem.hpp** - 背包系统
  - 功能：管理玩家的物品背包和装备系统
  - 主要类：InventorySystem

- **MapSystem.cpp** - 地图系统
  - 功能：处理游戏地图的生成、加载和管理
  - 主要类：MapSystem

- **PhysicsSystem.hpp** - 物理系统
  - 功能：处理游戏中的物理模拟和碰撞检测
  - 主要类：PhysicsSystem

- **PlayerHUD.cpp/PlayerHUD.hpp** - 玩家 HUD 系统
  - 功能：显示玩家界面信息，如生命值、魔法值等
  - 主要类：PlayerHUD

- **PortalSystem.cpp/PortalSystem.hpp** - 传送门系统
  - 功能：处理传送门的创建、激活和传送逻辑
  - 主要类：PortalSystem

- **RenderSystem.cpp/RenderSystem.hpp** - 渲染系统
  - 功能：处理游戏实体的渲染逻辑
  - 主要类：RenderSystem

- **SerializationSystem.hpp** - 序列化系统
  - 功能：处理游戏数据的保存和加载
  - 主要类：SerializationSystem

- **SkillSystem.cpp/SkillSystem.hpp** - 技能系统
  - 功能：处理技能的释放、冷却和效果
  - 主要类：SkillSystem

- **StatsSystem.cpp/StatsSystem.hpp** - 属性系统
  - 功能：管理实体的属性和状态计算
  - 主要类：StatsSystem

- **UIMinimap.cpp** - 小地图 UI 系统
  - 功能：显示游戏小地图界面
  - 主要类：UIMinimap

- **UICharacter.cpp** - 角色 UI 系统
  - 功能：显示角色属性界面
  - 主要类：UICharacter

### 游戏状态 (src/states/)

- **GameplayState.cpp** - 游戏状态
  - 功能：处理游戏进行中的逻辑和渲染
  - 主要类：GameplayState

- **InventoryState.hpp** - 背包状态
  - 功能：处理背包界面的显示和交互
  - 主要类：InventoryState

- **LoadingState.cpp/LoadingState.hpp** - 加载状态
  - 功能：处理游戏资源加载过程
  - 主要类：LoadingState

- **MainMenuState.cpp/MainMenuState.hpp** - 主菜单状态
  - 功能：处理主菜单界面和选项
  - 主要类：MainMenuState

- **PauseState.cpp/PauseState.hpp** - 暂停状态
  - 功能：处理游戏暂停时的界面和逻辑
  - 主要类：PauseState

### 工具 (src/tools/)

- **CrashHandler.cpp/CrashHandler.hpp** - 崩溃处理器
  - 功能：处理程序崩溃并生成错误报告
  - 主要类：CrashHandler

- **Logger.cpp/Logger.hpp** - 日志系统
  - 功能：提供日志记录功能
  - 主要类：Logger

### 工具函数 (src/utils/)

- **GPUUtils.hpp** - GPU 工具函数
  - 功能：提供 GPU 相关的辅助函数
  - 主要函数：GPU 相关工具函数

## 资源文件 (assets/)

- **assets/data/** - 游戏数据文件
  - `astrolabe.json` - 星盘系统配置数据
  - `biomes.json` - 生物群系配置数据
  - `tags.json` - 实体标签配置数据

- **assets/shaders/** - 着色器文件
  - `entity.frag/vert` - 实体渲染着色器
  - `flow_integration.compute` - 流场积分计算着色器
  - `flow_reset.compute` - 流场重置计算着色器
  - `flow_vector.compute` - 流场向量计算着色器
  - `grid_clear.compute` - 网格清理计算着色器
  - `grid_count.compute` - 网格计数计算着色器
  - `grid_sort.compute` - 网格排序计算着色器
  - `particle.compute` - 粒子计算着色器
  - `particle.frag` - 粒子渲染片段着色器

- **assets/textures/** - 纹理资源
  - `equipment/` - 装备纹理
  - `ui/` - UI 界面纹理
  - `ui/icons/` - 技能图标纹理

## 脚本文件 (scripts/)

- **gen_affix_data.py** - 生成词缀数据
- **gen_armor_jewelry_batch.py** - 批量生成护甲和珠宝数据
- **gen_equipment_registry.py** - 生成装备注册表
- **gen_pdb.bat** - 生成 PDB 文件的批处理脚本
- **gen_tags.py** - 生成标签数据
- **gen_weapons_misc_batch.py** - 批量生成武器和杂项数据
- **generate_blade_icons.py** - 生成刀剑图标
- **generate_icons.py** - 生成图标
- **get_skill_hashes.py** - 获取技能哈希值
- **resize_icons.py** - 调整图标大小
- **save_load_memory.py** - 内存保存加载脚本
- **spplit.py** - 分割脚本

## 测试文件 (tests/)

- 包含各种系统和功能的单元测试和集成测试
- **CMakeLists.txt** - 测试项目的 CMake 配置
- **main.cpp** - 测试主函数
- 各种具体测试文件，如 `AffixSystemTest.hpp`, `CombatSystemTest.hpp` 等

## 设计文档 (设计文档/)

- **地图和敌人刷新机制.md** - 地图和敌人刷新机制设计
- **怪物和AI设计.md** - 怪物和 AI 系统设计
- **核心战斗与角色设计.md** - 核心战斗和角色设计
- **技术架构与实现路线.md** - 技术架构和实现路线
- **局外成长与终局玩法.md** - 局外成长和终局玩法设计
- **开发计划与任务追踪.md** - 开发计划和任务追踪
- **游戏流程与状态管理.md** - 游戏流程和状态管理
- **战斗系统与属性设计.md** - 战斗系统和属性设计
- **职业被动和技能设置.md** - 职业被动和技能设置
- **职业设计草案_剑修.md** - 剑修职业设计草案
- **装备和存储设计.md** - 装备和存储系统设计
- **UI系统重构方案.md** - UI 系统重构方案

## 配置文件

- **CMakeLists.txt** - 项目构建配置
- **.gitignore** - Git 忽略配置
- **build.bat** - Windows 构建脚本
- **AGENTS.md** - 代理开发指南
- **GEMINI.md** - Gemini 模型相关说明
- **LICENSE** - 项目许可证
- **raylib使用opengl4.3.md** - Raylib OpenGL 4.3 使用说明
- **README.md** - 项目说明文档

## 项目特点

1. **ECS 架构**：使用 EnTT 库实现实体-组件-系统架构，便于模块化开发和性能优化
2. **GPU 加速**：大量使用 GPU 计算来处理流场、粒子等复杂效果
3. **并行处理**：使用 Taskflow 库进行并行处理，提高性能
4. **模块化设计**：系统之间通过共享上下文进行通信，降低耦合度
5. **完整的测试覆盖**：包含大量单元测试和集成测试
6. **丰富的游戏内容**：包含战斗、装备、技能、AI、地图等多种系统