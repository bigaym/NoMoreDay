# NoMoreDay 游戏项目架构与系统结构文档

## 1. 项目概述

NoMoreDay 是一个基于 ECS (Entity-Component-System) 架构的 2D 动作角色扮演游戏，使用 C++ 和 EnTT 实体组件系统开发。游戏采用 raylib 图形库进行渲染，并集成了 GPU 加速的粒子系统、实体系统和流场寻路系统。

## 2. 项目目录结构

```
NoMoreDay/
├── assets/                 # 游戏资源文件
│   ├── data/              # JSON 配置数据
│   ├── icons/             # UI 图标
│   ├── sprites/           # 精灵图像
│   └── textures/          # 纹理资源
├── scripts/               # Python 脚本工具
├── src/                   # 源代码目录
│   ├── components/        # ECS 组件定义
│   ├── core/              # 核心系统与上下文
│   ├── states/            # 游戏状态管理
│   ├── systems/           # ECS 系统实现
│   └── tools/             # 工具类
├── tests/                 # 单元测试
├── CMakeLists.txt         # 构建配置
├── design_docs/           # 设计文档
└── README.md             # 项目说明
```

## 3. 核心架构分析

### 3.1 主游戏循环 (Game.cpp)

`Game` 类是整个游戏的入口点和核心控制器，负责：

- **窗口管理**：初始化 raylib 窗口和 OpenGL 上下文
- **系统初始化**：按依赖顺序初始化所有子系统
- **游戏循环**：固定时间步长的更新循环
- **状态管理**：通过 StateManager 管理游戏状态栈

**初始化顺序**：
1. 全局静态资源 (星盘、技能、Buff 等注册表)
2. UI 系统 (加载字体)
3. GPU 系统 (粒子、实体、流场系统)
4. 推送初始状态 (MainMenuState)

**游戏循环流程**：
- 固定时间步长更新 (1/60 秒)
- 状态管理器更新当前状态
- GPU 系统同步与计算
- 渲染当前状态

### 3.2 状态管理 (StateManager)

基于栈的状态管理系统，支持透明状态渲染：

- **状态栈**：维护当前激活的状态栈
- **透明渲染**：支持状态分层渲染，上层状态可遮挡下层
- **状态转换**：提供 Push、Pop、Change 等状态操作

**状态类型**：
- `MainMenuState`：主菜单
- `GameplayState`：游戏玩法
- `InventoryState`：背包界面
- `SettingsState`：设置界面

## 4. ECS 架构分析

### 4.1 组件系统 (Components)

组件是纯数据结构，定义实体的属性：

**基础组件**：
- `Position`：位置坐标
- `Velocity`：速度向量
- `ColorComponent`：颜色信息
- `SpriteComponent`：精灵纹理

**战斗相关组件**：
- `HealthComponent`：生命值
- `CombatStats`：战斗属性
- `WeaponComponent`：武器信息
- `AttackState`：攻击状态

**角色相关组件**：
- `PlayerTag`：玩家标识
- `EnemyTag`：敌人标识
- `PrimaryStats`：基础属性点
- `ActiveEffectsComponent`：活跃效果

**技能系统组件**：
- `ActiveSkillsComponent`：激活技能
- `SkillComponent`：技能信息
- `SkillExecution`：技能执行状态
- `GlobalModifierComponent`：全局修饰符

### 4.2 系统实现 (Systems)

系统处理组件数据，实现游戏逻辑：

**核心系统**：
- `CombatSystem`：战斗处理
- `SkillSystem`：技能系统
- `StatsSystem`：属性计算
- `DamagePipeline`：伤害计算管道
- `PhysicsSystem`：物理模拟
- `AISystem`：AI 行为
- `RenderSystem`：渲染系统
- `UISystem`：UI 系统

**系统交互关系**：
```
InputSystem → AISystem → Player Movement
CombatSystem ← SpatialGrid (碰撞检测)
SkillSystem → DamagePipeline → CombatSystem
StatsSystem → CombatSystem (属性应用)
PhysicsSystem → RenderSystem (位置更新)
```

## 5. 战斗系统架构

### 5.1 属性系统

**PrimaryStats (基础属性)**：
- 力量 (Strength)：影响护甲和物理伤害
- 敏捷 (Dexterity)：影响闪避和暴击
- 智力 (Intelligence)：影响抗性和法力
- 体质 (Vitality)：影响生命值

**CombatStats (战斗属性)**：
- 生存：生命值、护甲、抗性
- 进攻：武器伤害、暴击、攻速
- 防御：闪避、格挡、减伤
- 特殊：移动速度、吸血、冷却缩减

**属性计算流程**：
1. 从 PrimaryStats 计算基础值
2. 应用装备、Buff、技能修饰符
3. 计算最终 CombatStats
4. 处理属性间相互影响

### 5.2 技能系统

**技能执行流程**：
1. `TryCast`：技能尝试释放
2. `SkillExecution`：技能执行状态
3. `RegisterEffect`：技能效果回调
4. `OnSkillHit`：技能命中处理

**技能类型**：
- `Flowing Thrust`：流云刺
- `Rending Wave`：裂空斩
- `Blade Formation`：灵剑决
- `Sword Array`：剑阵

**技能分支逻辑**：
- 天赋节点影响技能效果
- 剑意系统提供强化
- 装备特效触发额外效果

### 5.3 伤害计算管道

**五步伤害计算**：
1. 基础伤害池构建
2. 伤害类型转换与获取
3. 增伤乘区计算
4. 更多乘区计算
5. 最终结算 (暴击、防御)

**DamagePipeline 特点**：
- 支持多种伤害类型转换
- 动态属性查询
- 天赋节点修饰符应用
- 暴击与防御计算

## 6. AI 与寻路系统

### 6.1 AI 状态机

**AI 类型**：
- `IDLE`：闲置状态
- `PATROL`：巡逻状态
- `CHASE`：追击状态
- `ATTACK`：攻击状态
- `FLEE`：逃跑状态

**AI 决策逻辑**：
- 脱战与激活范围管理
- 流场寻路与局部回避
- 目标选择与状态转换

### 6.2 GPU 流场寻路

**GPUFlowFieldSystem**：
- 使用 Compute Shader 计算流场
- 支持动态目标更新
- 与 CPU 端 AI 系统集成

## 7. 渲染系统

### 7.1 渲染层次

**渲染顺序**：
1. 精灵渲染 (SpriteComponent)
2. GPU 粒子系统
3. GPU 实体系统
4. 基础形状渲染
5. 特效渲染
6. UI 渲染

### 7.2 视觉效果

**特效系统**：
- 攻击轨迹特效
- 伤害飘字系统
- 粒子效果
- 屏幕震动

## 8. UI 系统

### 8.1 UI 架构

**UIContext (UI 上下文)**：
- 全局 UI 状态管理
- 缩放与主题控制
- 交互状态跟踪

**UIRenderer (UI 渲染器)**：
- 统一渲染接口
- 主题系统
- 工具提示

### 8.2 UI 组件

**主要 UI 模块**：
- `UIInventory`：背包系统
- `UICharacter`：角色面板
- `UIAstrolabe`：星盘系统
- `UISkillHub`：技能树系统
- `UIMinimap`：小地图

## 9. 数据管理系统

### 9.1 注册表系统

**数据注册表**：
- `SkillRegistry`：技能数据
- `BuffRegistry`：Buff 数据
- `AstrolabeRegistry`：星盘节点
- `BiomeRegistry`：生物群系

### 9.2 资源管理

**ResourceManager**：
- 纹理资源缓存
- 字体资源管理
- 资源生命周期管理

## 10. 系统间交互关系

### 10.1 数据流

```
Input → StateManager → GameplayState → Systems
    ↓
InputSystem → Player/AI → PhysicsSystem → SpatialGrid → CombatSystem
    ↓
SkillSystem → DamagePipeline → CombatSystem → StatsSystem
    ↓
RenderSystem → UI System → Display
```

### 10.2 依赖关系

**核心依赖**：
- `SharedContext`：全局共享上下文
- `entt::registry`：实体组件系统
- `SpatialHashGrid`：空间查询网格

**系统依赖链**：
1. `StatsSystem` → `CombatSystem` → `SkillSystem`
2. `SpatialGrid` → `CombatSystem`/`AISystem`
3. `AssetLoadingSystem` → `RenderSystem`/`UISystem`

## 11. 第三方库集成

- **EnTT**：ECS 框架
- **raylib**：图形渲染和输入
- **Taskflow**：并行任务执行
- **nlohmann/json**：JSON 数据处理
- **spdlog**：日志系统

## 12. 扩展与维护指导

### 12.1 添加新系统

1. 创建新的系统类，实现 update 方法
2. 在 GameplayState::update 中调用系统
3. 确保系统依赖关系正确
4. 添加必要的组件定义

### 12.2 添加新组件

1. 在 components 目录下创建组件定义
2. 定义组件数据结构
3. 在系统中使用组件进行逻辑处理

### 12.3 性能优化要点

- 组件数据缓存与预计算
- 空间网格优化碰撞检测
- GPU 加速计算密集型任务
- 系统并行处理

### 12.4 调试与测试

- 使用 SpatialGrid 调试可视化
- StatsSystem 性能监控
- 系统独立单元测试
- 渲染性能分析

这个架构提供了良好的模块化设计，各系统间通过共享的实体组件系统进行通信，保证了高内聚低耦合的特性，便于功能扩展和维护。