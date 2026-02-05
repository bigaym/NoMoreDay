# 天赋系统星盘逻辑审计报告 (Astrolabe Logic Audit Report)

## 1. 审计概览 (Audit Overview)
当前 `assets/data/profession_talents.json` 定义了完整的剑修（Blade Ascendant）天赋树，但在系统实现层面，逻辑层（Logic）与配置层（Data）存在显著断层。

| 系统维度 | 实现状态 | 兼容性评估 |
| :--- | :--- | :--- |
| **天赋星盘 (Astrolabe)** | **部分实现** | 基础解锁逻辑就绪，但点数缩放（Multi-point nodes）未在属性系统同步。 |
| **属性系统 (Attribute)** | **基本兼容** | `StatType` 覆盖面广，但缺乏对 `effects` 和 `conversions` 的解析逻辑。 |
| **战斗系统 (Combat)** | **高风险** | 核心组件（剑意、剑心）挂载逻辑完全缺失，核心天赋点亮后无实际效果。 |
| **技能系统 (Skill)** | **高风险** | 机制数值（如剑意上限）硬编码量大，不响应天赋数据修改。 |

---

## 2. 详细缺陷分析 (Defect Analysis)

### A. 数值缩放失效 (Stat Scaling Failure)
*   **现象**：`StatsSystem::GetStatWithTags` 依然依赖旧的 `activated_nodes` (Set)，而忽略了 JSON 中的 `max_points: 5` 属性。
*   **影响**：玩家投入 5 点天赋，实际战斗中仅获得 1 层的数值支撑，导致数值强度大幅低于预期。

### B. 效果授予断路 (GrantComponent Failure)
*   **现象**：节点 1010 (`SwordIntentUnlock`) 等使用 `type: 0` (GrantComponent) 定义核心机制。C++ 层 `AttributePipeline` 目前没有解析逻辑。
*   **后果**：激活“起源”节点后，系统不会自动为玩家挂载 `SwordIntentComponent`，导致整个剑修机制无法启动。

### C. 转换逻辑缺失 (Conversion Logic Missing)
*   **现象**：节点 1031 (`IntToCritMult`) 定义了属性转化，但属性管道（Pipeline）仅处理了基础 `modifiers`。
*   **后果**：该类定义性天赋在当前版本 100% 失效。

---

## 3. 核心节点实现风险清单

| 节点 ID | 名称 | 风险详细说明 |
| :--- | :--- | :--- |
| **1010** | 剑意觉醒 | 系统无法识别并动态添加 `SwordIntentComponent`。 |
| **1030** | 剑意浩荡 | 剑意上限 `max_stacks` 在 C++ 中为硬编码常量，不接受天赋加成。 |
| **1031** | 心剑合一 | 属性转换公式未注册，战斗逻辑不感知该转换。 |
| **1001-1009** | 属性小节点 | `nodePoints` 逻辑未集成进 `GetStatWithTags`，多级属性加成缩水。 |

---

## 4. 修复建议 (Recommendations)
1.  **升级 `StatsSystem`**：全面迁移至 `nodePoints` 数据源。
2.  **实现 `EffectProcessor`**：在 `AttributePipeline` 中补全对 `effects` 数组的遍历与解析。
3.  **支持动态上限**：引入属性修改机制，使剑意等系统的 `max_charge` 能响应天赋修正。
