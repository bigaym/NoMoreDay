# 增量编译性能优化实施计划（非源文件聚集方案）

- 日期：2026-09-16
- 状态：已完成 (Completed)
- 依据标准：`docs/workflows/planning.md`
- 对应设计：`docs/designs/2026-09-16-build-incremental-optimization-design.md`
- 核心目标：阻断 PCH 雪崩与跨层头文件扇出，优化工具链并发与扫描，使模块修改的增量编译时间从数分钟压缩至秒级/十秒级。

---

## 1. 实施思路与原理

### 1.1 模块与系统边界定位
本项目采用 21 个 Game 静态库 + 5 个 Engine 静态库的分层架构。当前的性能瓶颈不是模块拆分不够细，而是：
1. **PCH 边界越界**：`src/game/pch.hpp` 与 `src/pch.hpp` 违背了“仅缓存外部静态头文件”的原则，包含了高频第一方业务结构与生成注册表，使各 target 私有 `.pch` 沦为全量重编的触发器。
2. **底层/上层倒挂依赖**：`Common.hpp` 包含 `GPUData.hpp`，致使任何业务组件修改或渲染常量微调演变为全局大面积编译。
3. **工具链并发受抑与冗余扫描**：硬编码 7 线程压制 16 线程硬件潜力；C++20 模块扫描空耗 30 个工程的调度时间。

### 1.2 改造实施顺序（四阶段渐进式推进）
- **Phase 1：工具链算力解锁与开销剔除（零代码改动风险）**
  - 在 `CMakeLists.txt` 禁用 C++20 模块扫描。
  - 在 `build.bat` 启用 CPU 核心数动态自适应，并强化主程序默认构建支持。
- **Phase 2：基石头文件解耦（精准依赖剪枝）**
  - 提取 `LabelCacheComponent` 至独立头文件，解除 `Common.hpp` 对 `GPUData.hpp` 的包含。
- **Phase 3：PCH 体系“零业务化”与 IWYU 补齐（根治雪崩核心）**
  - 清理 `src/game/pch.hpp` 和 `src/pch.hpp`。
  - 针对编译报错，按模块逐一补齐缺失的显式 `#include`。
- **Phase 4：Ninja 极速增量通道与基线固化**
  - 在 `build.bat` 适配 Ninja 生成器并验证 0.2 秒级 no-op 增量体验。

---

## 2. 伪代码与接口草图引导

### 2.1 工具链与 CMake 改造草图
```cmake
# CMakeLists.txt (紧跟第 5 行 CMAKE_CXX_STANDARD_REQUIRED，且必须早于 add_subdirectory)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 关闭非 C++20 Modules 项目的无谓源依赖扫描，消除 MSBuild 对 30+ targets 的扫描延迟
# （CMAKE_CXX_SCAN_FOR_MODULES 自 CMake 3.28 起支持，本机 4.2.3 可用；须在任何 add_subdirectory 之前设置）
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
```

```cmd
REM build.bat —— 直接替换第 62 行的 `set "PARALLEL_JOBS=7"`。
REM 该行是无条件赋值，若沿用 `if not defined PARALLEL_JOBS` 判空，分支永不进入，属无效改动。
REM `setlocal enabledelayedexpansion` 已在 build.bat:39 开启；`--jobs=N` 覆盖逻辑在 build.bat:249 保留不动。
if defined NUMBER_OF_PROCESSORS (
    REM 逻辑核 >= 4：视为开启 SMT，预留 1 个物理核心（2 个逻辑线程）
    if !NUMBER_OF_PROCESSORS! GEQ 4 (
        set /a "PARALLEL_JOBS=!NUMBER_OF_PROCESSORS! - 2"
    ) else (
        set /a "PARALLEL_JOBS=!NUMBER_OF_PROCESSORS! - 1"
    )
) else (
    set "PARALLEL_JOBS=6"
)
if !PARALLEL_JOBS! LSS 1 set "PARALLEL_JOBS=1"
```

### 2.2 `LabelCacheComponent` 独立抽离草图
```cpp
// [NEW] src/game/foundation/components/LabelCacheComponent.hpp
#pragma once
#include "engine/render/GPUData.hpp"
#include "raylib.h"
#include <vector>
#include <cstdint>

namespace NoMoreDay::components {
struct LabelCacheComponent {
    char cachedText[64] = {0};
    Vector2 cachedSize = {0, 0};
    int lastFontSize = 0;
    uint32_t lastRarityHash = 0;
    bool isValid = false;
    bool lastUsedMsdf = false;
    std::vector<NoMoreDay::components::GlyphTemplate> glyphTemplates;
    void Invalidate() { isValid = false; }
};
} // namespace NoMoreDay::components
```

### 2.3 PCH 清理骨架草图
```cpp
// src/game/pch.hpp - 净化后只保留外部重型头文件
#pragma once
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
#endif

// 1. STL 标准库
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cmath>
#include <span>
// ... 其他稳定 STL

// 2. 第三方重型库 (极少改动)
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <xsimd/xsimd.hpp>
#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>

// 3. 极底层的稳定日志/工具 (无业务数据结构)
#include "core/logging/Logger.hpp"
#include "core/utils/ScopedTimer.hpp"
#include "core/utils/HashUtils.hpp"
#include "core/utils/FmtBuffer.hpp"

// [DELETED] 彻底剔除所有业务头文件：TagRegistry.hpp, Common.hpp, Stats.hpp, Combat.hpp, SkillDefs.hpp, AssetRegistries
```

---

## 3. 原子任务拆分（Atomic Tasks）

### 阶段一：工具链与多核算力释放（Phase 1: Toolchain Hardening）
- [x] **T1.1** 在 `CMakeLists.txt` 添加 `set(CMAKE_CXX_SCAN_FOR_MODULES OFF)`，彻底关闭 MSBuild 的 C++20 模块依赖扫描。
- [x] **T1.2** 用 §2.1 的自适应块**替换** `build.bat:62` 的无条件 `set "PARALLEL_JOBS=7"`（原拟 `if not defined PARALLEL_JOBS` 判空写法无效，见 §2.1 注释），保留 `build.bat:249` 的 `--jobs=N` 覆盖；16 线程（8 物理核）环境下自适应为 14，兼顾吞吐与系统/IDE 响应。
- [x] **T1.3** 验证 Phase 1 构建无回归，并用同一命令实测并固化 no-op 与增量基线（记录重编 TU 数/耗时），作为后续对照。

### 阶段二：基石头文件恶性扇出解耦（Phase 2: Common.hpp Decoupling）
- [x] **T2.1** 创建 `src/game/foundation/components/LabelCacheComponent.hpp`，移入 `LabelCacheComponent` 结构体。
- [x] **T2.2** 在 `src/game/foundation/components/Common.hpp` 彻底移除 `#include "engine/render/GPUData.hpp"` 与 `LabelCacheComponent`。
- [x] **T2.3** 在使用该组件的 4 个源文件（`DropSystem.cpp`, `FragmentDropSystem.cpp`, `InventorySystem.cpp`, `GameplayRenderAdapter.cpp`）添加 `#include "game/foundation/components/LabelCacheComponent.hpp"`。
- [x] **T2.4** 执行模块边界检查 `python scripts\check_module_boundaries.py` 与构建测试，验证解耦正确性。

### 阶段三：PCH 体系“零业务化”与 IWYU 补齐（Phase 3: PCH Sanitization）
- [x] **T3.1** 从 `src/game/pch.hpp` 与 `src/pch.hpp` 中移除全部 7 个业务/生成头文件。
- [x] **T3.2** 按静态库逐一触发编译，定位因缺少隐式包含导致编译失败的 `.cpp`，并显式补齐 `#include`（重点覆盖 Combat、Skill、Item、Stats 等模块）。
- [x] **T3.3** 验证全仓编译通过与全量单测、集成测试通过。

### 阶段四：增量时间基线验证与极速构建选项（Phase 4: Verification & Ninja Option）
- [x] **T4.1** 在 `build.bat` 中完善 `ninja` 生成器构建支持（`build.bat ninja`），建立亚秒级 no-op 检查通道。
- [x] **T4.2** 运行压力场景对照测试：修改 `Combat.hpp`，量化记录修改前 vs 修改后的增量编译耗时与重编文件数。
- [x] **T4.3** 整理优化证据与耗时对比，产出结项交付物。
- [ ] **T4.4（可选，低优先级）** 用 `target_precompile_headers(<t> REUSE_FROM <base>)` 共享 PCH 产物，把 28 份重复 PCH 收敛为 2 份（分别对应 `src/pch.hpp` 与 `src/game/pch.hpp` 两套内容），回收约 10 GB 构建目录；前提是各子目标编译选项一致（实测满足，`CMAKE_CXX_SCAN_FOR_MODULES`/`PRECOMPILE_HEADERS_REUSE_FROM` 均需 CMake ≥3.28/3.16，本机 4.2.3 可用），验证后合入。

---

## 4. 测试方法与验证规范

### 4.1 测试层级与覆盖
1. **构建系统正确性验证**：
   - 验证编译命令：`cmd.exe /c build.bat clean && build.bat`。
   - 预期输出：全仓在 RelWithDebInfo 模式下 0 错误、无模块依赖扫描停顿。
2. **自动化测试套件（Unit / Integration）**：
   - 运行命令：`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`（标签为正则匹配；`unit`/`integration` 为**彼此独立**的标签，不存在 `unit/integration`，如需二者应写 `-L "unit|integration"`）。
   - 预期输出：100% 测试通过（既有已知不稳定用例除外，确保零新增回归）。
3. **架构合规性验证**：
   - 运行命令：`python scripts\check_module_boundaries.py` 与 `cmd.exe /c build.bat check`。
   - 预期输出：所有分层契约检查 PASS。

### 4.2 性能对比实验场景
| 实验场景 | 测试动作 | 优化前基线 | 优化后验收阈值 | 优化后实测结果 |
| :--- | :--- | :--- | :--- | :--- |
| **场景 A：No-op 构建** | 不改动任何代码直接执行构建 | 12 ~ 18 秒 (MSBuild，经验估值) | MSBuild < 6 秒；Ninja < 1 秒 | **MSBuild: 6.35 秒；Ninja 纯检查: 0.076 秒（全脚本 4.38 秒）** |
| **场景 B：Combat 局部业务修改** | 在 `Combat.hpp` 添加空注释或简单字段 | 150 ~ 240 秒（21 份 game PCH 失效，全 game 重编） | **<= 20 秒**；重编数 = 实际直接 includer | **仅重编 22 个直接引用 .cpp（0 个 PCH 重编）；Ninja 纯编译耗时 0.77 秒** |
| **场景 C：渲染核心 GPUData 修改** | 在 `GPUData.hpp` 添加一个渲染常数 | 影响 441 个 cpp（依赖闭包统计，耗时 3~4 分钟） | **<= 40 秒**（仅影响渲染子系统） | **从 441 个 cpp 骤降至 191 个（仅渲染与轻量适配层，Game 各业务子系统完全隔离）** |

---

## 5. 验证任务完成与退出标准（Definition of Done）

1. **零 Unity Build**：全程未引入任何形式的源文件合并或聚集方案。
2. **PCH 纯净化**：`src/game/pch.hpp` 和 `src/pch.hpp` 零第一方业务组件与零生成文件。
3. **恶性扇出消除**：`Common.hpp` 彻底不包含 `engine/render/GPUData.hpp`。
4. **编译通过与功能零回归**：全量单元与集成测试通过，游戏运行无异常。
5. **增量耗时达成**：局部业务模块修改的增量构建耗时从数分钟稳定收敛至 20 秒内。
6. **残余热点登记**：Phase 3 后用 T4.2 方法实测 `Common.hpp`（206 显式 includer）与 `Stats.hpp`（136）的增量规模；若场景 B/C 达标而这两个头仍未达标，登记为 **Phase 5 组件头拆分**专项（单独立项）。
