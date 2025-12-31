# 项目结构文档

## 项目概述

NoMoreDay 是一个基于 ECS (Entity-Component-System) 架构的 2D 游戏项目，使用 C++ 和 raylib 图形库开发。项目遵循现代游戏开发实践，采用组件化设计和并行处理技术。

## 项目目录结构

### 根目录文件

- **.gitignore**: Git 版本控制忽略文件配置
- **AGENTS.md**: 项目编码规则和代理工作指导
- **build.bat**: Windows 平台构建批处理脚本
- **CMakeLists.txt**: CMake 构建系统配置文件
- **GEMINI.md**: 项目核心背景、技术架构和操作指令文档
- **README.md**: 项目说明文档
- **test_output.txt**: 测试输出日志文件

### assets/ - 游戏资源目录

资源文件目录，包含游戏所需的所有图像、音频和数据文件

#### assets/data/ - 游戏数据文件

- **affixes.json**: 词缀系统数据配置文件，定义各种词缀属性
- **loot_tables.json**: 掉落表配置文件，定义物品掉落规则
- **loot_filters/** - 掉落过滤器配置目录
  - **default.json**: 默认掉落过滤器配置

#### assets/textures/ - 游戏纹理资源

- **characters/** - 角色纹理
  - **corrupted_beast.png**: 堕落野兽角色纹理
  - **cultist.png**: 邪教徒角色纹理
  - **demon.png**: 恶魔角色纹理
  - **player_warrior.png**: 玩家战士角色纹理
  - **skeleton.png**: 骷髅角色纹理
- **environment/** - 环境纹理
- **ui/** - UI 界面纹理
- **weapons/** - 武器纹理
  - **weapon_sword_fantasy_01.png**: 幻想剑武器纹理

### conductor/ - 项目管理和开发工具

- **product-guidelines.md**: 产品开发指南
- **product.md**: 产品规格说明
- **setup_state.json**: 开发环境设置状态
- **tech-stack.md**: 技术栈说明文档
- **tracks.md**: 开发进度追踪文档
- **workflow.md**: 开发工作流程文档
- **archive/** - 历史文档归档
- **code_styleguides/** - 代码风格指南

### plans/ - 项目规划文档

- **scene_generation_architecture.md**: 场景生成架构设计文档

### scripts/ - 项目脚本工具

- **asset_gen.py**: 资源生成脚本
- **gen_affix_data.py**: 词缀数据生成脚本
- **gen_armor_jewelry_batch.py**: 装备珠宝批量生成脚本
- **gen_weapons_misc_batch.py**: 武器杂项批量生成脚本
- **save_load_memory.py**: 记忆保存和加载脚本

### src/ - 源代码目录

主要游戏逻辑和系统实现

#### src/main.cpp - 程序入口点

C++ 程序的主入口函数，初始化游戏引擎并启动主循环。

#### src/pch.hpp - 预编译头文件

预编译头文件，包含常用的系统头文件以提高编译速度。

#### src/README.md - 源码目录说明

源代码目录的说明文档。

#### src/components/ - 组件定义目录

ECS 架构中的组件定义，每个组件代表实体的一个属性或状态

- **AffixComponent.hpp**: 词缀组件定义，包含词缀属性和效果数据
- **AIComponent.hpp**: AI 组件定义，包含敌人的 AI 状态和行为数据
- **Combat.hpp**: 战斗相关组件定义，包含伤害、防御等战斗属性
- **Common.hpp**: 通用组件定义，包含基础实体属性
- **EffectComponent.hpp**: 效果组件定义，处理各种状态效果和 buff
- **EnemyComponent.hpp**: 敌人组件定义，标识敌人实体并包含敌人特定属性
- **EquipmentComponent.hpp**: 装备组件定义，处理装备槽位和装备属性
- **InventoryComponent.hpp**: 背包组件定义，管理物品存储和数量
- **ItemComponent.hpp**: 物品组件定义，标识物品实体和基本属性
- **ItemStats.hpp**: 物品属性组件定义，包含物品的具体属性数值
- **MapComponent.hpp**: 地图组件定义，包含地图数据和结构信息
- **PlayerState.hpp**: 玩家状态组件定义，管理玩家当前状态
- **Projectile.hpp**: 投射物组件定义，处理弹道和投射物行为
- **Stats.hpp**: 基础属性组件定义，包含生命值、攻击力等基础数值
- **UIAnimationComponent.hpp**: UI 动画组件定义，处理界面动画效果

#### src/core/ - 核心系统目录

游戏核心系统和基础架构

- **Application.hpp**: 应用程序类定义，管理整个游戏应用的生命周期
- **AssetLoadingSystem.cpp**: 资源加载系统实现
- **AssetLoadingSystem.hpp**: 资源加载系统接口定义，负责加载和管理游戏资源
- **AssetRegistry.hpp**: 资源注册表定义，管理资源的注册和查找
- **Game.cpp**: 游戏主类实现
- **Game.hpp**: 游戏主类接口定义，协调各子系统运行
- **ItemFactory.cpp**: 物品工厂实现
- **ItemFactory.hpp**: 物品工厂接口定义，负责创建各种类型的物品
- **LevelManager.cpp**: 关卡管理器实现
- **LevelManager.hpp**: 关卡管理器接口定义，管理游戏关卡和场景
- **LootFilter.cpp**: 掉落过滤器实现
- **LootFilter.hpp**: 掉落过滤器接口定义，控制物品掉落规则
- **LootTable.hpp**: 掉落表定义，定义物品掉落概率和规则
- **ResourceManager.cpp**: 资源管理器实现
- **ResourceManager.hpp**: 资源管理器接口定义，管理内存中的资源
- **SharedContext.hpp**: 共享上下文定义，提供系统间共享的数据访问
- **State.hpp**: 游戏状态基类定义，定义状态系统的接口
- **StateManager.cpp**: 状态管理器实现
- **StateManager.hpp**: 状态管理器接口定义，管理游戏状态切换
- **UIAssetRegistry.hpp**: UI 资源注册表定义，管理界面资源
- **UIContext.hpp**: UI 上下文定义，提供 UI 系统的上下文信息
- **UIRenderer.cpp**: UI 渲染器实现
- **UIRenderer.hpp**: UI 渲染器接口定义，负责渲染用户界面

#### src/states/ - 游戏状态目录

不同游戏状态的实现

- **GameplayState.cpp**: 游戏状态实现
- **GameplayState.hpp**: 游戏状态接口定义，处理游戏运行时逻辑
- **InventoryState.cpp**: 背包状态实现
- **InventoryState.hpp**: 背包状态接口定义，处理背包界面和交互
- **LoadingState.cpp**: 加载状态实现
- **LoadingState.hpp**: 加载状态接口定义，处理资源加载过程
- **MainMenuState.cpp**: 主菜单状态实现
- **MainMenuState.hpp**: 主菜单状态接口定义，处理主菜单界面
- **PauseState.cpp**: 暂停状态实现
- **PauseState.hpp**: 暂停状态接口定义，处理游戏暂停逻辑

#### src/systems/ - 游戏系统目录

ECS 架构中的系统实现，处理特定类型的组件

- **AISystem.cpp**: AI 系统实现
- **AISystem.hpp**: AI 系统接口定义，处理敌人的 AI 行为和决策
- **CombatSystem.cpp**: 战斗系统实现
- **CombatSystem.hpp**: 战斗系统接口定义，处理实体间的战斗交互
- **CraftingSystem.cpp**: 制作系统实现
- **CraftingSystem.hpp**: 制作系统接口定义，处理物品制作和合成逻辑
- **DropSystem.cpp**: 掉落系统实现
- **DropSystem.hpp**: 掉落系统接口定义，处理敌人死亡后的物品掉落
- **EffectSystem.cpp**: 效果系统实现
- **EffectSystem.hpp**: 效果系统接口定义，处理状态效果和 buff 的应用
- **EnemyBehavior.cpp**: 敌人行为系统实现，处理敌人的具体行为逻辑
- **EnemySpawnSystem.cpp**: 敌人生成系统实现
- **EnemySpawnSystem.hpp**: 敌人生成系统接口定义，控制敌人的生成位置和时机
- **FogOfWarSystem.cpp**: 战争迷雾系统实现
- **FogOfWarSystem.hpp**: 战争迷雾系统接口定义，实现视野限制效果
- **InputSystem.cpp**: 输入系统实现
- **InputSystem.hpp**: 输入系统接口定义，处理玩家输入事件
- **InventorySystem.cpp**: 背包系统实现
- **InventorySystem.hpp**: 背包系统接口定义，处理背包物品管理
- **MapSystem.cpp**: 地图系统实现
- **MapSystem.hpp**: 地图系统接口定义，处理地图渲染和交互
- **PhysicsSystem.cpp**: 物理系统实现
- **PhysicsSystem.hpp**: 物理系统接口定义，处理碰撞检测和物理模拟
- **ProgressionSystem.cpp**: 进度系统实现
- **ProgressionSystem.hpp**: 进度系统接口定义，处理角色成长和进度保存
- **RenderSystem.cpp**: 渲染系统实现
- **RenderSystem.hpp**: 渲染系统接口定义，处理游戏画面渲染
- **SerializationSystem.hpp**: 序列化系统定义，处理数据的保存和加载
- **SpatialGrid.hpp**: 空间网格定义，用于高效的碰撞检测和邻居查询
- **StatsSystem.cpp**: 属性系统实现
- **StatsSystem.hpp**: 属性系统接口定义，处理实体的属性计算和更新
- **UIAnimationSystem.cpp**: UI 动画系统实现
- **UIAnimationSystem.hpp**: UI 动画系统接口定义，处理界面动画效果
- **UICharacter.cpp**: 角色界面系统实现
- **UICharacter.hpp**: 角色界面系统接口定义，处理角色属性界面
- **UICommon.hpp**: 通用 UI 系统接口定义，提供基础 UI 功能
- **UIInventory.cpp**: 背包界面系统实现
- **UIInventory.hpp**: 背包界面系统接口定义，处理背包界面显示
- **UIMinimap.cpp**: 小地图系统实现
- **UIMinimap.hpp**: 小地图系统接口定义，显示游戏地图的小地图
- **UISystem.cpp**: UI 系统实现
- **UISystem.hpp**: UI 系统接口定义，管理整个 UI 系统
- **XPAwardingSystem.cpp**: 经验奖励系统实现
- **XPAwardingSystem.hpp**: 经验奖励系统接口定义，处理经验获取和等级提升

#### src/tools/ - 工具类目录

- **Logger.cpp**: 日志记录器实现
- **Logger.hpp**: 日志记录器接口定义，提供统一的日志记录功能

#### src/utils/ - 工具函数目录

- **Parallel.hpp**: 并行处理工具定义，提供并行计算支持
- **Tilemask.hpp**: 瓷砖掩码工具定义，用于地图处理
- **UUID.hpp**: 唯一标识符工具定义，生成唯一 ID

### tests/ - 测试目录

- **AffixSystemTest.cpp**: 词缀系统测试
- **AssetLoadingSystemTest.cpp**: 资源加载系统测试
- **CMakeLists.txt**: 测试模块的 CMake 配置
- **CombatSystemTest.cpp**: 战斗系统测试
- **DropSystemBenchmark.cpp**: 掉落系统性能基准测试
- **DropSystemTest.cpp**: 掉落系统测试
- **EquipmentSystemTest.cpp**: 装备系统测试
- **ItemModificationTest.cpp**: 物品修改测试
- **ItemStatsTest.cpp**: 物品属性测试
- **ItemSystemTest.cpp**: 物品系统测试
- **LootFilterTest.cpp**: 掉落过滤器测试
- **ProgressionSystemTest.cpp**: 进度系统测试
- **RenderSystemTest.cpp**: 渲染系统测试
- **StatsBenchmark.cpp**: 属性系统性能基准测试
- **StatsSystemTest.cpp**: 属性系统测试

### 设计文档/ - 设计文档目录

- **地图和敌人刷新机制.md**: 地图生成和敌人刷新机制设计文档
- **怪物和AI设计.md**: 怪物和 AI 行为设计文档
- **核心战斗与角色设计.md**: 核心战斗系统和角色设计文档
- **技术架构与实现路线.md**: 技术架构和实现路线规划
- **局外成长与终局玩法.md**: 局外成长系统和终局玩法设计
- **开发计划与任务追踪.md**: 项目开发计划和任务追踪文档
- **游戏流程与状态管理.md**: 游戏流程和状态管理设计
- **战斗系统与属性设计.md**: 战斗系统和属性设计文档
- **装备和存储设计.md**: 装备系统和存储设计文档
- **UI系统重构方案.md**: UI 系统重构方案文档

## 项目架构特点

### ECS 架构

项目采用 EnTT 库实现的 ECS (Entity-Component-System) 架构，具有以下特点：

- 组件 (Components)：数据容器，存储实体的属性
- 系统 (Systems)：处理特定组件的逻辑
- 实体 (Entities)：唯一标识符，由组件组成

### 并行处理

使用 Taskflow 库实现并行处理，提高性能关键系统(如物理系统)的执行效率。

### 空间网格

使用空间网格(SpatialGrid)进行高效的碰撞检测和邻居查询，优化战斗和物理系统性能。

### 资源管理

统一的资源管理系统，负责游戏资源的加载、缓存和管理，确保资源的有效利用。

### 状态管理

基于状态机的游戏状态管理，支持主菜单、游戏、暂停等不同游戏状态的切换。

## 关键依赖

- raylib: 2D/3D 图形库
- EnTT: ECS 架构库
- Taskflow: 并行计算库
- spdlog: 日志记录库