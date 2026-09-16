# 增量编译性能优化设计规范（非源文件聚集方案）

- 日期：2026-09-16
- 状态：草案（方案设计阶段）
- 依据标准：`docs/workflows/design.md`
- 关联痛点：模块拆分后修改局部代码仍触发大范围全量编译，单次耗时达数分钟
- 涉及范围：
  - `src/pch.hpp` 与 `src/game/pch.hpp`（预编译头体系治理）
  - `src/game/foundation/components/Common.hpp` 与 `src/engine/render/GPUData.hpp`（基石组件依赖解耦）
  - `CMakeLists.txt`（C++20 模块扫描门禁、生成器与编译器参数配置）
  - `build.bat`（并发线程数探测、构建目标分流、Ninja 支持）
  - 核心业务头文件的包含关系与显式引用（IWYU 补齐）

---

## 1. 背景与核心痛点（Why & What）

### 1.1 现状与量化数据分析

项目在前期完成了架构模块化拆分（MS-7、M4 等），将单体工程拆分为 `Core`、`Engine`（5 个静态库）、`Game`（21 个静态库）、`App` 以及 `tests`（单体测试目标）。然而在实际开发中，开发者修改任意业务模块（如修改 `Combat.hpp` 或 `Stats.hpp` 中的一个结构体或常量），依然触发大范围增量编译，耗时常达 3~5 分钟。

经过系统级依赖闭包计算与构建过程监测，定位到以下 **四大核心病灶**：

1. **预编译头（PCH）的“业务毒化”与雪崩放大（最首要元凶）**：
   - 现状：Game 层有 20 个静态库 + 1 个 INTERFACE 契约库（`NoMoreDayGameContractsCore`，无源码不产 PCH）；其中 20 个子模块与 `SkillBehaviors`（OBJECT 库）共 **21 处**引用 `src/game/pch.hpp`，另有 **7 处**引用 `src/pch.hpp`（Engine/App 等）。MSVC/CMake 为每个引用目标独立生成一份私有的 `.pch` 二进制——**实测全工程共 28 份 `cmake_pch.pch`，合计 11.99 GB**。
   - 毒化：[`src/game/pch.hpp`](file:///d:/PRJ/NoMoreDay/src/game/pch.hpp#L68-L78) 包含了大量高频变动的第一方业务头文件：
     - `TagRegistry.hpp`（代码生成文件）
     - `Common.hpp`（全工程基石组件）
     - `Stats.hpp`（属性定义）
     - `Combat.hpp`（战斗事件与状态）
     - `SkillDefs.hpp`（技能核心结构）
     - `EquipmentAssetRegistry.hpp` / `RuneAssetRegistry.hpp`（生成文件）
   - 雪崩：修改上述任一业务头文件，**21 份 game PCH 二进制全部失效**。MSVC 必须先重新编译这 21 份 PCH，随后其旗下所有 `.cpp`（实测 `src/game` 共 182 个）全部被迫重新编译。模块拆分带来的物理隔离被 PCH 完全击穿。

2. **基石头文件的恶性跨层级扇出（Fan-out 爆炸）**：
   - 全工程传递依赖闭包统计（全工程 572 个 `.cpp`，含 262 个 `src` 与 310 个 `tests`）：
     - [`src/engine/render/GPUData.hpp`](file:///d:/PRJ/NoMoreDay/src/engine/render/GPUData.hpp)：影响 **441 个 .cpp（77.1%）**。
     - [`src/game/foundation/components/Common.hpp`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/Common.hpp)：影响 **364 个 .cpp（63.6%）**。
   - 跨层污染根因：底层渲染核心头文件 `GPUData.hpp`（1195 行）被上层游戏基础组件 `Common.hpp` 包含（`Common.hpp:4`），其原因仅是末尾一个用于战利品文字渲染缓存的冷门组件 [`LabelCacheComponent`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/Common.hpp#L201-L217) 使用了结构体 `NoMoreDay::components::GlyphTemplate`。导致全工程所有引用 `Position`/`Velocity` 的模块和测试全部与底层渲染结构体物理绑定。
   - **实测补充（本仓直接 include 扇出，非传递闭包）**：`Common.hpp` 被 206 个 `.cpp` 显式包含、`Stats.hpp` 136、`SkillDefs.hpp` 106、`Combat.hpp` 32、`TagRegistry.hpp` 12，`EquipmentAssetRegistry.hpp`/`RuneAssetRegistry.hpp` 各 2。这说明**仅做 PCH 纯净化后，改 `Common.hpp`/`Stats.hpp` 仍会分别波及 200+/130+ 个 TU**——这两个头的问题不是 PCH，而是自身粒度过粗，需在 Phase 5 按组件拆分（见 §4 风险 3）。

3. **测试工程体量倒挂与日常构建绑定**：
   - 规模倒挂：全工程 `src` 下有 262 个 `.cpp`，而 `tests` 下有 **310 个 `.cpp`**。全部通过 `file(GLOB_RECURSE)` 塞入单一的 `NoMoreDayTests.exe`。
   - 默认全量链接：[`build.bat`](file:///d:/PRJ/NoMoreDay/build.bat#L524) 默认目标为 `ALL_BUILD`。修改任意核心头文件，不仅业务代码重编，测试工程中上百个测试 `.cpp` 也跟着重编，随后 `link.exe` 耗费漫长时间链接已逼近 PDB 上限的测试巨怪。

4. **工具链与 MSBuild 增量冗余开销**：
   - C++20 模块依赖无谓扫描：项目使用 C++20 但未采用 C++20 Modules，MSVC 默认开启 `/scanDependencies`。30 多个工程每次构建前逐一输出“正在扫描源以查找模块依赖项...”，带来巨大调度时延。
   - 并发度限制：AMD Ryzen 7 7700（8核16线程）开发机环境下，[`build.bat:62`](file:///d:/PRJ/NoMoreDay/build.bat#L62) 硬编码了 `PARALLEL_JOBS=7`，算力闲置过半。
   - 编译器缓存基本失效：实测 `ccache -s`（PATH 上解析到的是 4.10.2，与配置指向的 4.12.2 不一致）显示 15489 次调用中**仅 13.0% 可缓存**；不可缓存的 86.97% 里，**84.92% 为 `Could not use precompiled header`**（ccache 无法复用 MSVC 外部 PCH）、14.09% 为 `Unsupported compiler option`。即当前 PCH 全覆盖下 ccache 近乎零收益，只增加每次调用开销。

### 1.2 目标与明确非目标

- **核心目标**：
  1. **斩断 PCH 雪崩链条**：修改局部业务头文件时，仅重编直接引用的编译单元，其他模块 PCH 保持有效，增量编译时间收敛至秒级/十秒级。**注意范围界定**：PCH 纯净化只消除「PCH 失效引发的全量重编」；`Common.hpp`/`Stats.hpp` 自身仍是高扇出头（206/136 个显式 includer），其增量规模须靠 Phase 5 组件头拆分才能继续下降。
  2. **剪除跨层级超级扇出**：彻底解除 `Common.hpp` 对 `GPUData.hpp` 的包含依赖，将 `GPUData.hpp` 的影响范围从 441 个 `.cpp` 压缩至 ~90 个（仅限渲染管线内部）。
  3. **释放多核算力与清除扫描冗余**：关闭 C++20 模块依赖扫描，并发度根据实际 CPU 线程数自动伸缩。
  4. **构建目标按需解耦**：日常开发构建默认收敛至主程序 `NoMoreDay`，测试目标按需触发。
- **明确非目标（Strict Non-Goals）**：
  1. **不使用任何形式的源文件聚集（Unity Build / Jumbo Build）**：不引入 `CMAKE_UNITY_BUILD` 或合并 `.cpp`，杜绝全局命名空间污染、静态符号冲突和单文件调试困境。
  2. **不改变现有的静态库模块层级架构**：保持已有的 21 个 Game 子模块与 5 个 Engine 子模块物理结构不变。
  3. **不改变产品游戏逻辑与 ABI 契约**：所有优化均为物理包含与构建配置维度的优化，不改变任何游戏组件数据内存对齐与业务语义。

---

## 2. 详细设计与解耦方案

### 2.1 方案一：PCH 体系“零业务化”改造（Zero Business Logic in PCH）

#### 原则
PCH 只能包含“外部引入的、极重度模板/头文件、全生命周期极少变更”的基础库。严禁包含任何第一方业务组件及自动生成的注册表头文件。

#### 设计改动
1. **重构 [`src/game/pch.hpp`](file:///d:/PRJ/NoMoreDay/src/game/pch.hpp)**：
   - 彻底移除以下 7 个头文件：
     - `game/foundation/data/TagRegistry.hpp`
     - `game/foundation/components/Common.hpp`
     - `game/foundation/components/Stats.hpp`
     - `game/foundation/components/Combat.hpp`
     - `game/foundation/components/SkillDefs.hpp`
     - `engine/resource/EquipmentAssetRegistry.hpp`
     - `engine/resource/RuneAssetRegistry.hpp`
   - 保留内容：
     - Windows 宏安全防御 (`WIN32_LEAN_AND_MEAN`, `NOMINMAX`)
     - STL 标准库头文件 (`<vector>`, `<string>`, `<algorithm>`, `<memory>`, `<span>`, `<cmath>` 等)
     - 核心第三方依赖库 (`<raylib.h>`, `<raymath.h>`, `<rlgl.h>`, `<nlohmann/json.hpp>`, `<spdlog/spdlog.h>`, `<xsimd/xsimd.hpp>`, `<entt/entt.hpp>`, `<taskflow/taskflow.hpp>`)
     - 极稳定的核心轻量工具 (`core/logging/Logger.hpp`, `core/utils/ScopedTimer.hpp`, `core/utils/HashUtils.hpp`, `core/utils/FmtBuffer.hpp`)
2. **重构 [`src/pch.hpp`](file:///d:/PRJ/NoMoreDay/src/pch.hpp)**：
   - 彻底移除 `engine/resource/EquipmentAssetRegistry.hpp` 与 `engine/resource/RuneAssetRegistry.hpp`。
3. **IWYU（Include What You Use）显式补齐**：
   - 移除 PCH 隐式包含后，在真正需要上述头文件的各个 `.cpp` 文件顶部显式补齐 `#include`。
     - 由于依赖关系变为文件级显式依赖，修改 `Combat.hpp` 仅会触发真正包含它的 `.cpp` 重新编译（实测当前显式 includer 为 **32 个**；IWYU 补齐后以实际引用者为准，必须在 T4.2 量化），21 份 game PCH 二进制毫发无损。

### 2.2 方案二：斩断 `Common.hpp` -> `GPUData.hpp` 恶性扇出

#### 现状解剖
`Common.hpp` 仅为了声明一个轻量组件 `LabelCacheComponent`，就引入了 1195 行的 `engine/render/GPUData.hpp`（只为使用其中的 `NoMoreDay::components::GlyphTemplate`）。而全仓仅有 4 个源文件使用 `LabelCacheComponent`：
- `src/game/systems/item/DropSystem.cpp`
- `src/game/systems/item/FragmentDropSystem.cpp`
- `src/game/systems/item/InventorySystem.cpp`
- `src/game/application/render/GameplayRenderAdapter.cpp`

#### 设计改动
1. **独立组件定义**：
   - 新建独立头文件：[`src/game/foundation/components/LabelCacheComponent.hpp`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/LabelCacheComponent.hpp)。
   - 将 `LabelCacheComponent` 迁移至该文件，并在该文件中包含 `<vector>`, `<raylib.h>` 以及 `engine/render/GPUData.hpp`。
2. **净化 `Common.hpp`**：
   - 从 `Common.hpp` 中彻底删除 `#include "engine/render/GPUData.hpp"` 与 `struct LabelCacheComponent`。
   - 在上述 4 个使用该组件的 `.cpp` 文件中显式包含 `LabelCacheComponent.hpp`。
3. **隔离效果**：
   - `Common.hpp` 彻底脱离对渲染管线的依赖。
   - `GPUData.hpp` 的影响范围从 **441 个 .cpp 骤降至 ~90 个**（仅限渲染系统与这 4 个掉落渲染适配文件），降幅达 **79.6%**！

### 2.3 方案三：构建生成器与工具链多核算力释放

#### 1. 全局关闭 C++20 模块依赖扫描
- 在根目录 [`CMakeLists.txt`](file:///d:/PRJ/NoMoreDay/CMakeLists.txt) 中紧跟 `set(CMAKE_CXX_STANDARD 20)` 添加：
  ```cmake
  set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
  ```
- 彻底消除 MSBuild 对 30 多个工程无端执行的 `/scanDependencies` 模块扫描开销。

#### 2. 动态并发度与系统调度保留策略（保留 1 个物理核心）
- 修改 [`build.bat`](file:///d:/PRJ/NoMoreDay/build.bat)：
  - 核心准则：**为系统与 IDE 始终保留 1 个完整物理核心做调度与交互响应**，杜绝 100% 占满导致系统卡死或调度抖动。
   - 动态计算机制：
     - 在开启 SMT / 超线程的环境下（逻辑处理器数为物理核心数的 2 倍），保留 1 个物理核心对应保留 2 个逻辑线程，计算公式为：`PARALLEL_JOBS = %NUMBER_OF_PROCESSORS% - 2`（至少为 1）。
     - 在未开启超线程的环境下，计算公式为：`PARALLEL_JOBS = %NUMBER_OF_PROCESSORS% - 1`（至少为 1）。
   - 实现要点：`build.bat:62` 是无条件 `set "PARALLEL_JOBS=7"`，必须**直接替换该赋值**；不能用 `if not defined PARALLEL_JOBS` 判空（永不成立，属无效改动）。`setlocal enabledelayedexpansion` 已在 `build.bat:39` 开启，`--jobs=N` 覆盖逻辑在 `build.bat:249` 保留。精确批处理写法见计划 §2.1。
  - 在当前 AMD Ryzen 7 7700（8 物理核心 / 16 逻辑处理器）环境下，并发数自适应设定为 **14**（16 - 2）。相较于原先硬编码的 7 线程，可用算力翻倍，同时确保系统始终保留 1 个物理核心（2 个逻辑线程）进行流畅的 OS 调度。

#### 3. 日常构建目标收敛
- 调整 `build.bat` 的构建策略：
   - 具体做法（保持默认兼容、不破坏 CI）：默认目标**仍是** `ALL_BUILD`；把内循环入口明确化——`build.bat notest` 已可只构建 `NoMoreDay`（`build.bat:181-182` 置 `BUILD_TEST_TARGET=OFF`，`build.bat:523-524` 据此把目标切回 `NoMoreDay`），但默认值不变，CI 的完整构建不受影响。建议另加 `build.bat fast` 别名 = `notest + novalidate`，并在文档/提示中引导日常使用。
  - 测试套件在执行 `build.bat test` 或运行 `ctest` 前按需增量构建。

#### 4. 拥抱 Ninja 原生构建模式（极速增量路径）
- `build.bat` 增加 `ninja` 构建选项支持（实测 `D:\mingw64\bin\ninja.exe` 存在；`build.bat:560-570` 已有 Ninja 仅用于 `compile_commands.json` 回退的先例）。
- **必须使用独立构建目录**：Ninja 是单配置生成器，无法复用 VS 生成的 `build/`，应使用 `build-ninja/`，以 `-G Ninja -DCMAKE_BUILD_TYPE=!BUILD_TYPE!` 配置，并**不传** `--config`/`/m:`/`/p:UseMultiToolTask`（VS/MultiToolTask 专属；`CMAKE_VS_GLOBALS` 也被 Ninja 忽略），同时**去掉 `/MP`**（Ninja 自带任务级并行，叠加 `/MP` 会过度订阅）。配置参数需与 `build.bat:499` 附近保持一致（含 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`）。
- 在多达 30+ 个子库的架构下，Ninja 使用全局扁平的 `.ninja_deps`，增量检查耗时仅需 **0.1 秒**，彻底消除 MSBuild 递归遍历 30 个 `.vcxproj` 的调度停顿。

### 2.4 可选方案：共享 PCH 编译产物（消除 28 份重复 PCH）

- 现状：28 个子目标各自编译一份内容几乎相同的 453 MB PCH（实测 11.99 GB），纯属重复浪费。
- CMake 4.2 支持 `target_precompile_headers(<target> REUSE_FROM <base>)`（`PRECOMPILE_HEADERS_REUSE_FROM`，3.16+；已确认本机 CMake 支持）：让各子目标复用同一 base 目标构建出的 PCH 产物。
- 前提已满足：实测 20 个 game 子模块与 Engine 各子目标的 `COMPILE_DEFINITIONS`/`COMPILE_OPTIONS` **完全一致**（REUSE_FROM 要求二者匹配）。
- 收益：PCH 编译实例 **28 → 2**（分别对应 `src/pch.hpp` 与 `src/game/pch.hpp` 两套内容）；PCH 失效时只重编 1 份而非 N 份；构建目录可回收约 10 GB。
- 定位：**Phase 3 之后收益下降**——PCH 只剩第三方头，正常开发几乎不再失效；故列为低优先级可选项（T4.4），主要价值在压缩冷构建与磁盘占用。
- 约束核对：属构建产物共享，非源文件聚集，不违反 §1.2 非目标 1。

---

## 3. 验收标准与测试验证

### 3.1 可观察的性能验收指标（Metrics）
1. **无修改增量构建耗时（No-op Build Time）**：
   - 现状：MSBuild 遍历检查耗时约 12~18 秒（**经验估值，基线须由 T1.3 实测固化**）。
   - 目标：Ninja 模式下小于 1.0 秒；MSBuild 模式下关闭模块扫描后小于 6 秒。
2. **单模块业务头文件修改增量耗时（Incremental Blast Radius）**：
   - 场景：在 `src/game/foundation/components/Combat.hpp` 中新增或修改一个注释/字段。
   - 现状：触发 21 个模块的 PCH 重编，编译 200+ 个 `.cpp`，耗时 150~240 秒。
   - 目标：仅重编直接引用 `Combat.hpp` 的 17 个 `.cpp`，无 PCH 重编，耗时降至 **10~20 秒以内**（提速 90% 以上）。
3. **渲染核心修改增量耗时**：
   - 场景：修改 `src/engine/render/GPUData.hpp`。
   - 现状：影响 441 个 `.cpp`。
   - 目标：影响范围收敛至小于 95 个 `.cpp`，Game 层 18 个系统完全不触发重编。

### 3.2 正确性与回归验证方式
1. **全仓编译通过性**：
   - 执行 `cmd.exe /c build.bat clean && build.bat`，确保在 RelWithDebInfo 下零编译错误通过。
2. **测试套件 100% 通过**：
   - 运行全量 CTest：`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`（标签为**正则匹配**；仓库中 `unit` 与 `integration` 是彼此独立的标签，**不存在** `unit/integration` 这一标签，若需两者应写作 `-L "unit|integration"`；AGENTS.md 约定用 `-L ci`），确保所有单元测试与集成测试无回归。
3. **架构边界门禁检查**：
   - 执行 `python scripts\check_module_boundaries.py`，确保头文件拆分不破坏模块间单向依赖。

---

## 4. 风险与缓解策略

1. **PCH 清理引发的隐式包含丢失**：
   - *风险*：某些 `.cpp` 原本未显式 `#include "Common.hpp"`，完全依赖 PCH 隐式注入，清理 PCH 会暴露大量缺少 include 的编译错误。
   - *缓解*：在实施阶段，按模块逐一编译并补齐显式 include。由于是“缺什么补什么”的机械式补充，且符合 IWYU 工业标准，风险极低且收益恒久。
2. **测试目标解耦对 CI/本地测试流程的影响**：
   - *风险*：开发者只编了主程序而遗漏测试构建。
   - *缓解*：保持 `build.bat` 在未加特殊参数时的兼容性（**默认目标仍为 `ALL_BUILD`**），日常提速走显式 `notest`/`fast` 别名并提供提示；CI 环境强制构建 `ALL_BUILD`，不依赖默认值。
3. **`Common.hpp`/`Stats.hpp` 残余热点未消除**：
   - *风险*：PCH 纯净化只消除 PCH 失效导致的全量重编；改这两个头仍会波及实测 206/136 个显式 includer，用户原始痛点只解决一半。
   - *缓解*：Phase 3 完成后立即用 T4.2 的方法实测两个头的增量规模；若仍不达标，追加 **Phase 5：基石头文件拆分**——按领域把 `Common.hpp`/`Stats.hpp` 拆为细粒度头（如 `TransformComponents.hpp`、`CombatStats.hpp`），消费者改为最小 include。此项涉及面广，须单独立项并按 design→planning→implementation 流程走。
