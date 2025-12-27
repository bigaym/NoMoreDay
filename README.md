# NoMoreDay

NoMoreDay是一款使用C++开发的游戏项目，采用ECS架构设计，专注于核心战斗系统和丰富的游戏体验。

## 项目特点

- **ECS架构**: 采用Entity-Component-System架构模式，提供灵活的游戏对象管理系统
- **核心战斗系统**: 包含完整的战斗机制和角色设计
- **资源管理系统**: 集成的资源管理和加载系统
- **现代化C++**: 使用现代C++特性进行开发

## 技术架构

项目包含以下核心系统：
- 输入系统 (InputSystem)
- 渲染系统 (RenderSystem) 
- 物理系统 (PhysicsSystem)
- 战斗系统 (CombatSystem)
- 资源管理系统 (ResourceManager)
- 空间网格系统 (SpatialGrid)

## 文件结构

```
NoMoreDay/
├── src/                    # 源代码
│   ├── core/              # 核心系统
│   ├── systems/           # ECS系统
│   ├── components/        # 组件定义
│   └── tools/             # 工具类
├── assets/                # 游戏资源
│   ├── textures/          # 纹理资源
│   └── ...
├── scripts/               # 脚本工具
├── 设计文档/              # 项目设计文档
└── CMakeLists.txt         # 构建配置
```

## 构建要求

- C++17或更高版本
- CMake 3.10或更高版本
- 支持的平台: Windows, Linux, macOS

## 构建说明

```bash
mkdir build
cd build
cmake ..
make
```

## 项目状态

这是一个活跃开发中的项目，包含完整的游戏设计文档和开发计划。

## 许可证

请参阅项目中的许可证文件。