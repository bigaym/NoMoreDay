# 伤害管线现代化重构设计方案架构审计报告（Design Review）

- 审查对象：`docs/designs/2026-09-11-damage-pipeline-modernization-design.md`
- 日期：2026-09-11
- 审计人：Architecture Auditor & Code Risk Analyzer (NoMoreDay Engine & Systems)
- 审计结论：**通过（已按审计发现与用户裁决完成设计修订）**
- 对应实施计划：`docs/plans/2026-09-11-damage-pipeline-modernization-plan.md`

---

## 1. 审计概述与设计目标对齐审视

本次审计围绕用户提出的四大重构目标：**提升性能**、**增加稳定性**、**抽象能力与模块化**、**减少代码量**，对设计方案进行了深度静态审查与底层技术风险推演。

### 1.1 用户裁决项（User Decisions）
在初审沟通中，用户针对关键设计点做出了明确裁决：
1. **转换效率字段移除**：确认不设计独立的转换效率字段（$E[src]$），仅保留纯百分比（Convert 封顶 100% 比例缩放，GainExtra 纯百分比不封顶），从根本上避免了比例归一化 $scaleC$ 抵消转换效率的数学悖论。
2. **代码量增加预期**：明确由于旧管线残缺、新架构需要承载完整的模块化与防御抽象，整体工程代码量的增加在预料之内；重构核心在于单体解耦、模块化与健壮性，而非盲目削减全仓代码行数。
3. **数值平衡后置**：候选桩移除与双算修复带来的数值跳变属缺陷修复，平衡调优在重构落地后统一处理。

---

## 2. 关键设计漏洞与解决方案处置映射

| # | 审计发现问题 | 潜在技术风险 | 方案修订与落地机制 |
|---|--------------|--------------|-------------------|
| **B1** | **守方动态条件与静态物化冲突** | 冻结增伤（172）、命印叠层（512）、血量阈值暴击（150）等强依赖守方状态，若在攻方施法时物化会导致跨目标污染或快照失效。 | 采用**攻方轻量快照（`AttackerSnapshot`）+ 守方条件位掩码（`TargetConditionMask`）**。攻方在 AoE 场景单次构建，守方通过位运算快速求值合并动态乘区。 |
| **B2** | **转换后源元素标签残留导致多重收益** | 物理转火焰后若保留 `Tag::Physical`，后续独立乘区（Physical More / Increased）将错误作用于转换后火焰伤害。 | 规范 `TransformTagsOnConversion`：转换或 GainExtra 产物**强制剥离所有旧伤害类型标签**（`kAllDamageTypeTags`），仅注入目标类型标签，保留动作/机制标签。 |
| **B3** | **反击与受击触发重入导致 EnTT 指针失效 (UAF)** | Blade Ward 反击（1324/1795）或 Apply 死亡触发直接同步调用 `ResolveDamage`，内层修改组件池导致外层持有的裸指针变野指针。 | ① 计算核心只接收栈上 POD 快照值拷贝；<br>② 引入线程局部重入守卫（深度≤2）；<br>③ 引入**延迟动作队列（`DeferredActionQueue`）**，反击请求在结算收尾后统一安全派发。 |
| **B4** | **实体 ID 复用与全局 LRU 缓存并发竞争** | `source_entity` 作为缓存键在投射物销毁复用后引发幽灵命中；多线程 Taskflow 下全局 LRU 缓存存在并发写竞争与锁瓶颈。 | **彻底废弃全局动态 LRU 缓存**。<br>① 投射物/DoT 实体在生成瞬间挂载 `DamageSnapshotComponent` 直存快照；<br>② 瞬发近战在栈上即时提取（数十纳秒），实现天生无锁并发安全。 |
| **B5** | **武器基础点伤与次级伤害双算** | 去桩后，次级打击（分裂、DoT、环境、荆棘）若带 `skill_id`，会被错误累加武器均伤与技能配置点伤（Double Dipping）。 | 在 `DamageRequest` 中引入 **`DamageOrigin` 强类型枚举**。唯有 `DirectSkillCast` 注入武器点伤与技能基础点伤，其余类型严格仅以 `base_pool` 为基础点伤。 |
| **B6** | **业务副作用强行数值化** | Blade Ward 拦截需消耗飞剑、清空伤害、生成反击，强塞入数值 Op 违背单一职责；无敌检查过晚浪费算力。 | 建立**三层流水线架构**：<br>① Layer 1 (Pre-Mitigation)：无敌与高闪避直接 Early-Out 返回；<br>② Layer 2 (DamageKernel)：纯数学函数（noexcept）；<br>③ Layer 3 (Post-Mitigation)：拦截与 Apply 处理。 |
| **B7** | **单批并轨引发的 SIMD 向量化退化** | 若将批量路径完全退化为逐个标量循环，200 目标 AoE 性能严重下降；若强制单目标走向量化则开销倒挂。 | 采用**两段式自适应编排**：攻方阶段统一计算 1 次；守方减伤单目标（$N<4$）走标量内联，批量（$N\ge 4$）保留基于 `xsimd` 的向量化并行减伤。公式统一引用 `CombatFormula`。 |

---

## 3. 架构规范合规性核查（Architecture Auditor Checklist）

### 3.1 DOD 数据布局与内存对齐
- [x] **POD 结构体验证**：`AttackerSnapshot`（约 160 字节）与 `DefenseSnapshot`（约 64 字节）均为纯 Plain Old Data，无 `std::string` 或动态分配容器；
- [x] **内存对齐保证**：核心快照显式标注 `alignas(32)`，匹配 AVX/SIMD 向量加载规范；
- [x] **紧凑栈开销**：消除原设计中高达 3KB+ 的 `FixedVector<DamageOp, 64>` 栈膨胀，快照在栈上传递与拷贝极度轻量，Cache Line 友好。

### 3.2 EnTT 安全与生命周期
- [x] **指针持久性断绝**：计算内核不持有 `registry.try_get` 返回的组件内部指针，全部通过值拷贝快照隔离；
- [x] **迭代与重入安全**：延迟动作队列彻底阻断了“伤害结算中途重入导致组件池重分配”的崩溃路径；
- [x] **幽灵缓存根除**：快照随实体组件生命周期自动创建与销毁，消除实体 ID 重用导致的幽灵增伤。

### 3.3 并发与 Taskflow 线程安全
- [x] **无共享可变状态（No Shared Mutable State）**：移除全局 LRU 缓存，计算段内无全局静态变量写入，无跨线程互斥锁争用；
- [x] **Taskflow 任务独立性**：`CalculateBatch` 的并行减伤段只读攻方快照，各守方结果独立写入 `results[i]` 槽位，避免 False Sharing。

---

## 4. 审计结论与实施同步

设计方案 `docs/designs/2026-09-11-damage-pipeline-modernization-design.md` 已全面吸纳本次架构审计的全部 7 项核心改进：
1. **三层流水线架构（Layer 1/2/3）** 已明确入档；
2. **攻守状态解耦（快照 + 状态位掩码）** 已取代旧静态物化模型；
3. **实体组件化直存（`DamageSnapshotComponent`）** 已取代全局 LRU 缓存；
4. **纯百分比转换与源标签剥离（`TransformTagsOnConversion`）** 已固化；
5. **`DamageOrigin` 强类型来源与武器点伤唯一注入规则** 已建立；
6. **两段式自适应 SIMD 批量减伤** 已规范化。

**后续推进动作**：
推进实施计划 `docs/plans/2026-09-11-damage-pipeline-modernization-plan.md` 同步更新，按 P0~P4 阶段稳步进入代码编写与自动化测试。
