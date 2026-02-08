# TODO 闭环修复 Track - 实施计划

| 字段 | 值 |
|---|---|
| Track ID | `todo-closure-core-loop_20260208` |
| 类型 | Fix |
| 优先级 | P0 |
| 状态 | Completed |

## Phase 0: 约束确认

- [x] 0.1 锁定范围为 `src/` 与必要测试；显式忽略 `conductor/archive/**` TODO 状态
- [x] 0.2 记录基线（当前 TODO 命中与相关函数位置）

## Phase 1: SkillSystem 反击伤害补全

- [x] 1.1 定位 `SkillSystem.cpp` 中 Phantom Flash 反击 TODO
- [x] 1.2 梳理并确认 `DamagePipeline` 所需输入并构造调用参数
- [x] 1.3 反击结算强制接入 `DamagePipeline`，移除/禁止局部伤害公式
- [x] 1.4 增加日志/事件标记便于断言
- [x] 1.5 编写/更新自动化测试：触发反击时经流水线产生伤害

## Phase 2: SaveManager 存档头信息补全

- [x] 2.1 新增 `PlayerName` 组件并在玩家初始化流程挂载默认值 `玩家0`
- [x] 2.2 将 `playtime` 明确为角色生涯累计秒数，并落地累计来源
- [x] 2.3 在 `createSnapshot` 写入 `PlayerName` 与累计 `playtime`，移除固定占位
- [x] 2.4 编写/更新自动化测试：保存名 name=`玩家0`(默认) 与 `playtime` 符合累计语义

## Phase 3: 回归与验收

- [x] 3.1 编译与自动化测试回归
- [x] 3.2 校验“无旧存档兼容要求”下的新语义写入/读取行为
- [x] 3.3 输出变更说明与风险复核

## 交付物

1. `conductor/tracks/todo-closure-core-loop_20260208/spec.md`
2. `conductor/tracks/todo-closure-core-loop_20260208/plan.md`
3. 代码与测试改动

## 退出标准

1. 满足 `spec.md` 中 AC-SKILL-001/002。
2. 满足 `spec.md` 中 AC-SAVE-001/002。
3. 满足 `spec.md` 中 AC-TEST-001。
4. 未触碰 `conductor/archive/**`。
