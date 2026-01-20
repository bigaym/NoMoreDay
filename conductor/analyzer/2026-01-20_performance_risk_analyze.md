# NoMoreDay - 性能风险与极致"魔法"优化方案报告 (V2.0 深度更新)

## 1. 现状评估 (Current State)
目标：**180 FPS** (帧预算 **5.5ms**)。
瓶颈：CPU 与 GPU 同步开销（Draw Calls）、大批量实体属性计算带来的 Cache Miss、以及单线程逻辑。

---

## 2. 深度"魔法"优化建议 (The "Magic" Optimizations)

### 2.1 GPU-Driven Pipeline: MDI 与 GPU 剔除 [魔法等级: 超位]
*   **痛点**: 目前 CPU 仍在遍历实体决定谁该画，这在 10k 实体下太慢。
*   **魔法方案**: **Multi-Draw Indirect (MDI)**。
*   **实现细节**:
    1.  **全量上传**: CPU 将所有实体的变换数据一次性存入 SSBO。
    2.  **GPU 剔除 (Compute Shader)**: 使用一个专用的 Compute Shader 进行视锥剔除和遮挡剔除。
    3.  **Indirect Command**: Compute Shader 直接写入一个 `DrawArraysIndirectCommand` 缓冲区。
    4.  **一次性绘制**: CPU 仅调用一次 `glMultiDrawArraysIndirect`。
*   **魔法效果**: **CPU 开销降为 O(1)**，无论屏幕上有 1,000 还是 10,000 个怪，CPU 只发一个指令。

### 2.2 EnTT 深度分组 (Internal Grouping) [魔法等级: 炼金术]
*   **痛点**: 属性系统在 Recalculate 时需要跨组件访问（Stats + Equipment + Buffs），导致随机内存访问。
*   **魔法方案**: **entt::group**。
*   **实现细节**:
    *   强制将 `CombatStats`, `Position`, `Velocity` 划分为一个 **Owning Group**。
    *   EnTT 会在物理内存上将这些组件重新排列，确保它们不仅在各自的池中连续，而且 **跨组件也是完全连续的**。
*   **魔法效果**: 属性系统更新时，CPU 预取器能以最高效率工作，**Cache Miss 接近 0**。

### 2.3 Branchless 逻辑与位运算 [魔法等级: 幻术]
*   **痛点**: 损伤计算和属性更新中有大量的 `if (hasBuff)` 或 `if (isEnemy)` 分支，导致 CPU 分支预测失败。
*   **魔法方案**: **无分支计算 (Branchless Logic)**。
*   **实现细节**:
    *   使用 `mask = -(int)condition;` 将布尔值转为全 0 或全 1 的掩码。
    *   计算公式改为：`final_damage = (base * mult) & mask;` 或 `value += bonus * (int)has_bonus;`。
*   **魔法效果**: 消除流水线停顿，大幅提升大规模实体循环的速度。

### 2.4 零拷贝缓冲区 (Zero-Copy Buffer Strategy) [魔法等级: 禁咒]
*   **痛点**: 每帧更新投射物位置需要从 CPU 传给 GPU，带宽和同步是瓶颈。
*   **魔法方案**: **Triple-Buffered Persistent Mapping**。
*   **实现细节**:
    *   分配 3 倍大小的 Persistent Mapped Buffer。
    *   使用 `glFenceSync` 确保 CPU 写入第 3 块内存时，GPU 正在读取第 1 块。
*   **魔法效果**: **完全消除 CPU 与 GPU 之间的 Wait-for-Idle 同步停顿**，渲染和逻辑可以真正的 100% 并行。

### 2.5 空间网格 SIMD 化 (SIMD Hash Grid) [魔法等级: 召唤术]
*   **痛点**: 空间网格查询中的距离平方计算和哈希计算在高烈度战斗下非常密集。
*   **魔法方案**: **xsimd 向量化查询**。
*   **实现细节**:
    *   一次处理 8 个实体的坐标数据。
    *   使用 SIMD 指令同时计算 8 个实体到玩家的距离。
*   **魔法效果**: 网格查询性能提升 4-6 倍，为 AI 和投射物碰撞留出充足时间。

---

## 3. 针对 Raylib 的特定优化 (Raylib Hacks)

1.  **绕过 rlgl 状态机**: 对于 `GPUEntitySystem`，直接调用 `glBindBufferRange` 和 `glDraw*`，避免 `rlgl` 内部的状态同步和冗余检查。
2.  **Texture Batching**: 强制所有怪物和掉落物使用同一张 4k 大贴图（Atlas），消除 Texture Bind 切换。
3.  **字库渲染优化**: 伤害飘字（Damage Popups）全部转为 GPU 粒子实现，而不是调用 `DrawText`。

---

## 4. 优先级建议 (Implementation Roadmap)

1.  **P0 (极致性能)**: 实现 **GPU-Driven MDI 渲染**，解决 Draw Call 瓶颈。
2.  **P1 (内存效率)**: 使用 **entt::group** 重构 Combat 相关组件。
3.  **P2 (逻辑提速)**: 对 Projectile 碰撞和 Stats 更新进行 **SIMD 化**。

---
*分析师: Gemini (Skill: code-risk-analyzer)*
