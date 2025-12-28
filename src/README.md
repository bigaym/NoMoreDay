# NoMoreDay 源代码结构说明

## 📁 目录结构总览

```
src/
├── main.cpp                    # 程序入口点
├── pch.hpp                     # 预编译头文件
├── components/                 # ECS 组件定义
├── core/                       # 核心框架与应用逻辑
├── systems/                    # ECS 系统实现
├── tools/                      # 工具类与辅助功能
└── utils/                      # 通用工具函数
```

---

## 🧩 Components 模块 (src/components/)

ECS 架构中的组件定义，用于描述实体的属性和状态。所有组件都应为 POD（Plain Old Data）类型，以保证数据局部性。

### 组件列表：

- **AIComponent.hpp** - AI 相关属性，包含行为状态、目标实体等
- **Common.hpp** - 通用组件定义，如位置、速度、生命值等基础属性
- **EffectComponent.hpp** - 特效组件，包含粒子效果、视觉反馈等
- **EnemyComponent.hpp** - 敌人特有属性，如敌人类型、AI 等级等
- **InventoryComponent.hpp** - 背包系统组件，管理物品持有
- **ItemComponent.hpp** - 物品属性组件，包含物品类型、品质、属性等
- **MapComponent.hpp** - 地图相关组件，如地图格子、区域信息等
- **PlayerState.hpp** - 玩家状态组件，如当前技能、经验等
- **Stats.hpp** - 角色属性组件，包含攻击力、防御力、生命值等数值

---

## 🔧 Core 模块 (src/core/)

游戏核心框架与应用层逻辑，管理游戏的生命周期和主要状态。

### 模块功能：

- **AssetRegistry.hpp** - 资源注册表，管理所有游戏资源的加载和引用
- **Game.cpp/Game.hpp** - 游戏主类，实现游戏主循环、状态管理
- **LevelManager.cpp/LevelManager.hpp** - 关卡管理器，负责地图生成、切换和持久化
- **ResourceManager.cpp/ResourceManager.hpp** - 资源管理器，处理纹理、音频、配置文件的加载与缓存

---

## ⚙️ Systems 模块 (src/systems/)

ECS 架构中的系统实现，处理具有相同组件类型的实体集合的逻辑。

### 系统列表：

- **AISystem.cpp/AISystem.hpp** - AI 系统，处理敌人 AI 逻辑，如寻路、攻击决策
- **CombatSystem.cpp/CombatSystem.hpp** - 战斗系统，处理攻击、伤害计算、技能效果
- **EffectSystem.cpp/EffectSystem.hpp** - 特效系统，管理粒子效果、视觉反馈
- **EnemyBehavior.cpp** - 敌人行为实现，具体 AI 行为逻辑
- **EnemySpawnSystem.cpp/EnemySpawnSystem.hpp** - 敌人生成系统，管理敌人刷新机制
- **FogOfWarSystem.cpp/FogOfWarSystem.hpp** - 战争迷雾系统，实现地图探索效果
- **InputSystem.cpp/InputSystem.hpp** - 输入系统，处理玩家输入（键盘、鼠标、手柄）
- **InventorySystem.cpp/InventorySystem.hpp** - 背包系统，管理物品交互、装备系统
- **MapSystem.cpp/MapSystem.hpp** - 地图系统，处理地图渲染、碰撞检测
- **PhysicsSystem.cpp/PhysicsSystem.hpp** - 物理系统，处理碰撞、移动、速度计算
- **RenderSystem.cpp/RenderSystem.hpp** - 渲染系统，处理所有视觉元素的绘制
- **SpatialGrid.hpp** - 空间哈希网格，用于高效的空间查询和碰撞检测
- **UISystem.cpp/UISystem.hpp** - UI 系统，处理用户界面显示和交互

---

## 🛠 Tools 模块 (src/tools/)

开发和调试工具类，提供辅助功能。

### 模块功能：

- **Logger.cpp/Logger.hpp** - 日志系统，提供格式化日志输出功能

---

## 🧰 Utils 模块 (src/utils/)

通用工具函数和辅助类，不特定于游戏逻辑。

### 模块功能：

- **Parallel.hpp** - 并行计算工具，封装 Taskflow 相关功能
- **Tilemask.hpp** - 地图瓦片遮罩工具，用于地图生成和处理

---

## 📝 编程规范与注意事项

### ECS 架构原则：

1. **组件（Components）**：必须是 POD 类型，只包含数据，不包含逻辑
2. **系统（Systems）**：必须是无状态的，只处理具有相同组件类型的实体集合
3. **实体（Entities）**：由 EnTT 管理，作为组件的容器

### 性能优化原则：

1. **数据局部性**：组件数据在内存中连续存储，提高缓存命中率
2. **并行处理**：利用 Taskflow 实现系统间的并行执行
3. **避免内存分配**：主循环中避免动态内存分配，使用对象池等技术

### 代码组织：

1. **模块职责单一**：每个系统只处理特定类型的组件
2. **依赖关系清晰**：系统间的依赖通过 Taskflow 明确声明
3. **接口简洁**：组件和系统接口保持简洁，易于维护和扩展

---

## 🔄 游戏循环顺序

根据项目架构要求，游戏循环执行顺序为：

1. **Input → Player Movement → AI → Combat → Spatial Grid Rebuild → Physics**

此顺序对于帧一致性至关重要，特别是 Combat 系统使用前一帧的空间网格数据（可接受的 1 帧延迟）。

---

## 🔗 相关文档

- [战斗系统与属性设计](../../设计文档/战斗系统与属性设计.md)
- [怪物和AI设计](../../设计文档/怪物和AI设计.md)
- [技术架构与实现路线](../../设计文档/技术架构与实现路线.md)