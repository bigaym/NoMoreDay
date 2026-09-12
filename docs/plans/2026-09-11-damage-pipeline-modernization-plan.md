# 伤害管线现代化重构实施计划（R4）

- 依据设计：`docs/designs/2026-09-11-damage-pipeline-modernization-design.md`（含 2026-09-11 架构审计修订与决策记录）
- 依据审计：`docs/reviews/2026-09-11-damage-pipeline-modernization-design-review.md`（设计架构审计通过结论）
- 计划前序：`docs/reviews/2026-09-11-damage-pipeline-modernization-plan-review.md`（R1 审查与处置）
- 基线 commit：`a803623f`
- 用户裁决汇总：
  1. 仅 Convert 单源封顶 100% 比例缩放，GainExtra 纯百分比不封顶；
  2. **不设计转换效率字段**，仅保留纯百分比；
  3. 攻守状态正交解耦（攻方快照 + 守方条件掩码快速求值）；
  4. 转换强制剥离源元素标签（`TransformTagsOnConversion`）；
  5. 引入 `DamageOrigin` 强类型枚举，彻底解决武器基础点伤与次级伤害双算；
  6. 三层流水线解耦业务与数学，延迟动作队列解决反击重入 UAF；
  7. 废弃全局动态 LRU 缓存，投射物/DoT 挂载组件直存快照；
  8. 守方减伤两段式自适应（单目标标量内联，N≥4 保留 `xsimd` 批量向量化）；
  9. 代码量增加在预料之内，优先保证完善性与稳定性；
  10. 桩移除带来的数值变化在实现后统一进行数值平衡。

---

## 1. 实施思路

### 1.1 分层推进与阶段门禁

按 P0 → P4 推进；每阶段独立提交、可单独 revert，仓库在任何时刻保持可构建、可测试。

| 阶段 | 目标 | 核心动作 | 退出标准 |
|------|------|----------|----------|
| **P0 止血与分类** | 根除武器双算与候选桩旁路 | 引入 `DamageOrigin`；审计全仓调用点并分类；移除 CombatV2 候选桩与 `×1.05`；更新桩期测试断言 | 全测试通过；前后对照与锚点清单归档；彻底清零候选桩 |
| **P1 结构解耦与重入安全** | 抽离拦截器与延迟动作队列 | 建立三层流水线；抽离 Pre/Post 拦截器；反击压入延迟队列；单/批防御共享；修复 `calculateBatch` 钩子 | 反击重入安全测试通过；无敌/闪避提前退出生效；基准不劣化 |
| **P2 转换与标签规范** | 落地恐怖黎明转换与标签剥离 | 实现纯百分比快照转换与 `TransformTagsOnConversion` 标签剥离；统一单位协议；删除旧级联链 | 转换 C1–C9 及标签剥离测试全绿；多元素独立转换通过；双重收益消除 |
| **P3 状态正交与组件化** | 攻守解耦与无锁直存 | 落地 `AttackerSnapshot` + `TargetConditionMask`；投射物/DoT 挂载 `DamageSnapshotComponent` 直存；废弃全局 LRU | 冻结/命印等动态条件测试全绿；Taskflow 并行压测无死锁/数据竞争 |
| **P4 清理收敛与终验** | 收敛废弃代码与全量性能终验 | 收敛 `foundation/combat_v2`；删除兼容层旧代码；运行全量基准测试与一致性对账 | 删除清单为空；单目标延迟不劣于基线，AoE 吞吐优于基线；事件一致率 ≥99.9% |

### 1.2 目标数据流（三层流水线架构）

```text
[DamageRequest (含 DamageOrigin 强类型)]
   │
   ▼
【Layer 1: Pre-Mitigation Interceptors】
   ├─ 守方无敌判定 ──▶ 立即返回 0 伤害 (Early-Out)
   ├─ 闪避掷骰 ──────▶ 判定闪避立即返回 (Early-Out)
   └─ 提取/复用 AttackerSnapshot，提取 DefenseSnapshot 与 TargetConditionMask
   │
   ▼
【Layer 2: DamageKernel (纯数学内核，noexcept，0 ECS 依赖，0 堆分配)】
   ├─ Build (按 DamageOrigin 规范注入点伤)
   ├─ Convert/GainExtra (快照比例缩放，纯百分比，源标签剥离)
   ├─ Increased/More (基础乘区 + 守方条件掩码快速加权)
   ├─ Crit Resolution (基础暴击 + 动态条件暴击)
   └─ Mitigation (单目标标量内联，N≥4 xsimd 向量化减伤)
   │
   ▼
【Layer 3: Post-Mitigation Interceptors & Application】
   ├─ 拦截判定 (Blade Ward Interception: 消耗飞剑与伤害清零)
   ├─ 反击判定 (Blade Ward Counter: 生成反击请求压入延迟队列)
   ├─ CombatSystem::ApplyDamage (屏障、生命扣除与死亡判定)
   ├─ 一致性事件派发 (CombatEventDispatcher)
   └─ 执行延迟动作队列 (安全触发反击，杜绝重入 UAF)
```

### 1.3 计划纪律与硬约束

1. **重入安全铁律**：计算核心严禁持有 `registry.try_get` 返回的组件裸指针，所有输入通过栈上 POD 快照传递。反击与受击触发必须压入延迟动作队列，在 `Execute` 收尾阶段统一派发，严禁结算中途重入修改组件池。
2. **天生无锁并发**：废弃全局动态 LRU 缓存。投射物与 DoT 实体在生成时挂载 `DamageSnapshotComponent`；近战在栈上即时提取快照。Taskflow 并行段内 0 锁、0 全局写竞争。
3. **标签剥离铁律**：伤害转换后强制剥离源元素标签，仅保留目标元素标签及非元素动作标签，杜绝多重受益。
4. **性能预算**：单目标单次命中延迟 ≤ 基线 P95；AoE 200 目标批量吞吐优于基线。单目标计算 0 堆分配断言保护。

---

## 2. 接口与类型冻结（WS0）

### 2.1 模块布局

```text
src/game/systems/combat/damage/
  DamageTypes.hpp            // DamageOrigin, TargetConditionMask, 实例定义
  DamageSnapshot.hpp         // AttackerSnapshot, DefenseSnapshot, DamageSnapshotComponent
  DamageConversion.hpp       // 纯百分比快照转换与 TransformTagsOnConversion (头文件内联)
  DamageStages.hpp/.cpp      // Build, Increased, More, Crit 纯函数
  DamageKernel.hpp/.cpp      // 阶段编排纯数学内核 (单目标与 SIMD 批量共用公式)
  DamageInterceptors.hpp/.cpp// Pre/Post 业务拦截器 (无敌, 闪避, 剑阵拦截, 延迟反击)
src/game/systems/combat/
  DamagePipeline.hpp/.cpp    // 门面：快照调度、三层流水线编排、Apply 落地、事件派发
  CombatConstants.hpp        // 常量与容量预算
```

### 2.2 核心数据结构草图

```cpp
// 1. 伤害来源分类
enum class DamageOrigin : uint8_t {
    DirectSkillCast,   // 技能直接施法 (注入武器点伤 + 技能配置点伤)
    SecondaryProc,     // 衍生次级打击 (仅使用 base_pool)
    AilmentTick,       // 异常 DoT (仅使用 base_pool，不暴击)
    HazardEnvironment, // 地面环境 (仅使用 base_pool)
    ThornsReflect,     // 荆棘反伤 (仅使用 base_pool)
    ItemAffixProc      // 装备特效 (仅使用 base_pool)
};

// 2. 守方条件掩码
enum class TargetCondition : uint32_t {
    None         = 0,
    Frozen       = 1 << 0,  // 处于冻结
    Bleeding     = 1 << 1,  // 处于流血
    HpAbove80    = 1 << 2,  // 生命值 > 80%
    Controlled   = 1 << 3,  // 受控 (眩晕/定身/减速)
    HasFateMark  = 1 << 4,  // 拥有命印
    HasQiBrand   = 1 << 5,  // 拥有气印
};

// 3. 动态条件修饰项
struct ConditionalDamageOp {
    TargetCondition required_condition;
    OpStage stage;         // More, CritDamage
    float base_value;      // 基础增量 (如 0.50f)
    float per_stack_value; // 叠层增量 (如 0.03f/层)
    BuffKind stack_source; // 关联 Buff
};

// 4. 转换规则
struct ConversionRule {
    DamageType src_type;
    DamageType dst_type;
    float pct;             // 纯转换比例 (0.0 ~ 1.0)
    bool is_gain_extra;    // true 为 GainExtra, false 为 Convert
};

// 5. 攻方快照 (约 160 字节紧凑 POD)
struct alignas(32) AttackerSnapshot {
    std::array<float, 6> base_flat = {0.0f};
    std::array<float, 6> increased = {0.0f};
    std::array<float, 6> unconditional_more = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    float base_crit_chance = 0.05f;
    float base_crit_multiplier = 1.50f;
    float armor_pen = 0.0f;
    float accuracy = 0.97f;
    Tag combined_tags = Tag::None;
    float added_effectiveness = 1.0f;
    float trigger_effectiveness = 1.0f;

    FixedVector<ConversionRule, 8> conversions;
    FixedVector<ConditionalDamageOp, 4> conditional_ops;
};

// 6. 守方快照
struct alignas(32) DefenseSnapshot {
    std::array<float, 6> resistances = {0.0f};
    float armor = 0.0f;
    float dodge_chance = 0.0f;
    float block_chance = 0.0f;
    float block_multiplier = 1.0f;
    float global_dr = 0.0f;
    int cached_area_level = 1;
    uint32_t condition_mask = 0;
};

// 7. 投射物与 DoT 实体快照组件
struct DamageSnapshotComponent {
    AttackerSnapshot snapshot;
    DamageOrigin origin;
    uint32_t skill_id;
    entt::entity caster;
};
```

---

## 3. 分阶段实施任务清单

### 3.1 Phase 0: 止血与分类（P0）
- **P0-1 基线固化**：运行 `DamagePipelineBenchmark` 与 `CombatReleaseGateBenchmark`，固化单目标与 200 目标性能基线；
- **P0-2 请求分类审计**：全仓扫描伤害调用点，引入 `DamageOrigin` 枚举，为普攻、反伤、DoT、环境、分裂打击显式标记来源；
- **P0-3 武器来源修复与删桩**：
  - 修复 `SkillRegistry` 缺 id 0 回退默认普攻的武器乘区污染；
  - 仅允许 `DirectSkillCast` 注入武器点伤；
  - 彻底删除 `DamagePipeline.cpp:511-538` 中的 CandidateOnly 桩拦截代码；
- **P0-4 测试锚点全量重写**：重写 `CombatV2CutoverTests` 与 `DefenseMitigationChainTests` 中的旧桩期 `105` 期望值；
- **P0-5 行为变化归档**：记录去桩与去双算引起的数值变化至 `phase-P0/balance-impact.md`（平衡调优后置）。

### 3.2 Phase 1: 结构解耦与重入安全（P1）
- **P1-1 钩子修复**：修复 `calculateBatch` 注册钩子，确保真实返回批量计算结果；
- **P1-2 拦截器抽离**：实现 Layer 1 (Pre-Mitigation) 与 Layer 3 (Post-Mitigation) 拦截器，无敌/高闪避提前退出；
- **P1-3 延迟动作队列落地**：引入线程局部 `s_deferred_actions` 与重入守卫，将 Blade Ward 反击与受击施法改为收尾延迟派发，消除重入 UAF；
- **P1-4 公式单源化**：消除 SIMD 路径中的内联 level factor，全量走 `CombatFormula.hpp`；
- **P1-5 死代码清理与 More 数组合并**：删除已废弃的旧计算函数，合并两套 More 数组。

### 3.3 Phase 2: 转换与标签规范（P2）
- **P2-1 纯百分比转换算法落地**：实现 §4.4 快照式转换纯函数，严格保证守恒、Convert 封顶 100% 比例缩放、GainExtra 纯百分比；
- **P2-2 标签剥离实现**：实现 `TransformTagsOnConversion`，转换与 GainExtra 产物强制剥离旧元素标签，补充防双重收益测试；
- **P2-3 多元素分池支持**：Build 阶段按伤害类型分池独立转换，多元素技能互不影响；
- **P2-4 单位协议归一**：全仓暴击率统一为 `0.0 ~ 1.0` 分数制，修复批量路径除以 100 的历史缺陷。

### 3.4 Phase 3: 状态正交与组件化直存（P3）
- **P3-1 攻守状态解耦**：实现 `AttackerSnapshot` 提取与 `TargetConditionMask` 快速求值，流云刺 172、万剑 512、弱点 150 改走动态条件规则；
- **P3-2 实体组件化直存**：在投射物生成与 DoT 施加处挂载 `DamageSnapshotComponent`，彻底废弃全局动态 LRU 缓存；
- **P3-3 SIMD 两段式自适应分流**：攻方阶段统一执行一次，守方减伤单目标标量内联，N≥4 保留 `xsimd` 批量并行处理；
- **P3-4 并发压力验证**：Taskflow 多线程并发触发命中压测，验证无锁且无数据竞争。

### 3.5 Phase 4: 清理收敛与终验（P4）
- **P4-1 废弃代码收敛**：删除 `foundation/combat_v2` 中已无用的 Facade/Kernel 原型与桩，清理 CMake 过滤词；
- **P4-2 兼容开关与残渣删除**：终检并删除全部迁移期兼容代码；
- **P4-3 性能与门禁终验**：对比 P0 基线，单目标延迟不劣于基线，AoE 吞吐显著提升；
- **P4-4 文档与报告结项**：归档全阶段交付物，结项关闭。

---

## 结项状态（2026-09-12）

| 阶段 | 状态 | 证据 |
|------|------|------|
| P0 止血与基线 | 已完成，主代理门禁通过 | `docs/reports/damage-pipeline-modernization/phase-P0/` |
| P1 结构化结算与安全 | 已完成，主代理门禁通过 | `docs/reports/damage-pipeline-modernization/phase-P1/` |
| P2 转换与标签规范 | 已完成，主代理门禁通过 | `docs/reports/damage-pipeline-modernization/phase-P2/` |
| P3a/P3b 状态正交与组件化 | 已完成，主代理门禁通过 | `docs/reports/damage-pipeline-modernization/phase-P3/` |
| P4 清理收敛与终验 | 已完成，本包结项 | `docs/reports/damage-pipeline-modernization/phase-P4/P4-report.md` |

**P4 完成标准核对**：删除清单为空（构建输入范围零引用）；单目标延迟不劣于基线；AoE 200 P99 显著优于基线；`build.bat` / `ctest -L ci` / `ctest -L integration` 全绿；全阶段交付物归档。遗留项为 release-gate 计时噪声（详见 P4 报告 §5），非阻塞。本计划结项关闭。
