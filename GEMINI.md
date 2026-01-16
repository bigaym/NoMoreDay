# NoMoreDay - 智能体规则与上下文

## 1. Project Identity
*   **游戏类型**: 高性能 2D 类暗黑 (Diablo-like) Roguelite ARPG。
*   **核心技术**: C++20, ECS (EnTT), Raylib。
*   **目标**: 支持 10,000+ 实体同屏，GPU 加速，数据导向设计 (DOD)。

## 2. Tech Stack Rules
*   **语言**: 必须使用 **C++20** (Modules, Concepts, Coroutines)。
*   **ECS**: 仅限使用 **EnTT**。组件 (Components) 必须是 POD 类型。
*   **渲染**: **Raylib** + 自定义 OpenGL 计算着色器 (Compute Shaders)。
*   **异步**: 使用 **Taskflow** 进行基于 DAG 的并行调度。
*   **数学**: 使用 **xsimd** 进行向量化加速。
*   **内存**: 使用 **mimalloc** 进行内存分配。
*   **数据**: 使用 **nlohmann/json** 进行序列化。
*   **日志**: **spdlog**。

## 3. Development Directives
### 安全与稳定性
*   **规则**: 对未定义行为 (UB)、内存泄漏或 Use-After-Free 零容忍。
*   **规则**: 严格遵守 **RAII**。禁止使用原生 `new`/`delete`。使用智能指针或 EnTT 句柄。
*   **规则**: 必须保证线程安全。禁止使用全局可变状态。

### 性能与数据导向设计 (DOD)
*   **规则**: **数据导向设计**为首要原则。最大化缓存局部性 (Cache Locality)。
*   **规则**: 避免在热点路径 (Hot Paths) 中使用虚函数和深层继承。
*   **规则**: 主游戏循环中禁止进行堆内存分配。使用对象池或预分配缓冲区。
*   **规则**: 使用 `std::string_view` 和 `std::span` 避免不必要的拷贝。

### 配置与常量
*   **规则**: 逻辑常量 $\to$ `src/game/components/Common.hpp`。
*   **规则**: GPU/渲染相关常量 $\to$ `src/engine/render/GPUData.hpp`。
*   **规则**: 不要锁死帧率。尊重 `settings.json` 中的 `target_fps`（默认为 180）。

## 4. Build & Environment
*   **构建**: 运行 `.\build.bat` (Windows)。输出位于 `build/bin/`。
*   **测试**: 位于 `build/bin/tests/`。
*   **脚本**: `scripts/` 目录下的 Python 3.10+ 脚本（遵循 Google 风格）。

## 5. 目录结构
*   `src/app`: 入口点，状态机。
*   `src/core`: 基础设施（日志、数学、线程）。
*   **`src/engine`**: 引擎系统（渲染、物理、输入、资源）。
*   **`src/game`**: 游戏业务逻辑（组件、状态、数据）。
*   `src/systems`: ECS 系统实现。
*   `assets/`: 纹理、着色器、JSON 配置。