# 技能7专精节点整改复核报告（第二轮）

- 日期：2026-09-10
- 审查对象：`docs/reviews/2026-09-10-skill7-specialization-nodes-review.md` 所列问题的修复波
- 复核方式：独立 cpp-reviewer 子代理只读审查 + 主线程构建/测试验证
- 结论：**可提交**（0 Blocker / 0 Major；登记项见 §5）

## 1. 第一轮复核（修复波前）

结论「需修改」，5 个 Major：

| 编号 | 问题 | 处置 |
| --- | --- | --- |
| M1 | 703 射程点满负收益（缺 `del.range` 基准） | `SkillSpecializationBaker.cpp` case7 设基准射程；0 点 350、4 点 490 |
| M2 | 775 压制无中心门控，AoE 全命中均挂 | `applyHit` 按中心距离判定；711/754/772 传 `false`→`cutPos` |
| M3 | 772 感电区域重复全额落雷且可叠加 | 区域改为仅施加 Shock，伤害由落雷本体承担 |
| M4 | 714 寂灭 OnKill 无击杀来源过滤 | `CreateOnKill` 增 `skill_id`、`TriggerRule.required_skill_id=7`、ProcEngine 过滤、DamagePipeline 两处透传 |
| M5 | 735 孤影被硬编码 `nearbyCount=1` 绕过 | 按命中位置真实计数（`isIsolatedAt`） |

另处理 Minor：732/734 优先级、GetFloat 临时串、聚合函数声明归位、元素映射判定、772 死数据、ShockField owner 守卫。

## 2. 第二轮复核发现与处置

新 Major：

- **730 次级撕裂中心参考点错误**：`applyHit` 对全部路径使用主 `cutPos` 计算距中心距离，730 次级命中实际中心为 `secPos`，导致 715 中心暴击 / 775 压制参考错中心。
  修复：`applyHit` 形参由 `bool inCenter` 改为 `const Vector2 &hitCenter`，主 tick/711/754/772 传 `cutPos`，730 传 `secPos`（`BeamChannelDeliverySystem.cpp:372`）。
  一致性追加：`onHitNodesFn` 内 752 流血中心加速原硬编码主 `cutPos`，改为复用 `applyHit` 计算出的 `centerHit`（`BeamChannelDeliverySystem.cpp:335-345`）。

Minor 处置：

| 项 | 处置 |
| --- | --- |
| `GetFloat(const char*)` 注释与实现矛盾 | 改为 `string_view` 异质查找（透明哈希），重载不再构造临时 `std::string` |
| InputSystem 732/734 注释不准确 | 注释改为真实语义：734 激活时新按下左键优先登记打断，持续按住才微步 |
| 350 射程双硬编码 | 外置机制表键 `(7, 0, "base_range")`，Baker 与交付层共用同键同默认值 |
| 714 粗粒度仅代码注释 | 登记至本报告 §5 |
| 测试 Minor：用例 13 重复上界断言、M2 场景二依赖精确半径 84 | 合并断言；改为按 `Bake` 结果推导区间中点并加保护 |

## 3. 回归测试

`tests/functional/MindBladeNodes.cpp` 新增「730 次级撕裂以自身中心判定 775」：不点 711，唯一敌人距主中心 > centerRadius，断言其获得 `Skill7MentalSuppression`（旧实现不会挂）。原 5 个 Major 回归用例保留，测试 Minor 已清理。

## 4. 验证证据（RelWithDebInfo）

- `cmake --build build --config RelWithDebInfo --target NoMoreDayTests`：EXIT=0
- MindBlade 专项：16 用例 / 148 断言，全部通过
- `ctest -L ci`：100% 通过（1380 用例 / 107192 断言，0 失败）

## 5. 登记取舍与残留风险

1. **714 技能级粗粒度**：按 `skill_id==7` 过滤而非「711 碎空爆」精确判定；取 711 时其余命中路径已禁用，实际近似仅引爆，影响可忽略。
2. **711 外圈失去 715 中心暴击**：M2 中心门控的连带语义变更，属预期收敛。
3. **772 区域总伤害下降**：区域不再直接结算伤害，改由异常系统承担，待数值验证。
4. **DoT 击杀不触发 714**：DoT tick `skill_id=0`，与设计「直接击杀」一致。
5. **775 Lightning×730 组合、752 流血中心（次级撕裂）**：逻辑已一致，但测试未直接覆盖；752 仅有机制复用保证。
6. **M4 projectile 链路**：代码审查确认透传，测试仅走直接伤害链。
7. 首 tick 立即结算与技能5 一致，未变更。

## 6. 变更规模

工作区 31 文件，+1591/-446（含 `settings.json` 既有运行时改动；`docs/reviews` 审查文档与 `tests/functional/MindBladeNodes.cpp` 未跟踪）。未提交。
