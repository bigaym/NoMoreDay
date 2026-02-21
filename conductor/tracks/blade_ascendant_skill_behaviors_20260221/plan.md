# Blade Ascendant Skill Behaviors Refactor - 执行计划

> **Track ID**: `blade_ascendant_skill_behaviors_20260221`

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|---|---|---|---|
| Phase 1 | 行为引擎护栏 | Trigger/Transmuter/Scope guard | [ ] |
| Phase 2 | 技能分批迁移 | 9 技能行为迁移与对齐 | [ ] |
| Phase 3 | 剑意与御剑步统一 | 资源交互和移动状态统一 | [ ] |
| Phase 4 | 回归与稳定性 | 测试补齐与回归清理 | [ ] |

---

## Phase 1: 行为引擎护栏

- [ ] Task 1.1: 增加 Trigger depth/cooldown guard
- [ ] Task 1.2: 增加 Transmuter 互斥检查
- [ ] Task 1.3: 增加 ScopePolicy 判定入口
- [ ] Task 1.4: 增加通用日志与诊断码（用于 validation）

## Phase 2: 技能分批迁移

- [ ] Task 2.1: 迁移 3.1 流云刺 / 3.2 裂空斩
- [ ] Task 2.2: 迁移 3.3 灵剑决 / 3.4 剑气护体
- [ ] Task 2.3: 迁移 3.5 万剑归宗 / 3.6 剑阵
- [ ] Task 2.4: 迁移 3.7 心剑无影 / 3.8 御剑回旋 / 3.9 绝影绝剑
- [ ] Task 2.5: 迁移后行为快照回归（旧档案）

## Phase 3: 剑意与御剑步统一

- [ ] Task 3.1: 剑意获取、消耗、超时清空统一
- [ ] Task 3.2: 御剑步 buff 生命周期统一
- [ ] Task 3.3: 御剑步联动节点（命中回蓝/延时/暴击）统一接口
- [ ] Task 3.4: 绝影期间特殊逻辑与 Trigger 互不冲突

## Phase 4: 回归与稳定性

- [ ] Task 4.1: 新增 unit tests（Trigger 防环、互斥、范围声明）
- [ ] Task 4.2: 新增 integration tests（SkillBehaviorRegistry 主流程）
- [ ] Task 4.3: 修复回归并更新文档
- [ ] Task 4.4: 运行 `build.bat`

