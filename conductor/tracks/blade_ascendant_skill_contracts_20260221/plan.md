# Blade Ascendant Skill Contracts Refactor - 执行计划

> **Track ID**: `blade_ascendant_skill_contracts_20260221`  
> **依赖**: 无（foundation track）

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|---|---|---|---|
| Phase 1 | 合同类型与注册器 | 新增合同数据结构与注册器 | [ ] |
| Phase 2 | 配置迁移 | skills.json 增量迁移与校验脚本 | [ ] |
| Phase 3 | 运行时挂接 | SkillSystem 读取合同并执行业务约束 | [ ] |
| Phase 4 | UI/存档对齐 | 可视化与持久化最小闭环 | [ ] |

---

## Phase 1: 合同类型与注册器

- [ ] Task 1.1: 新增 `SkillContract`/`NodeContractData` 数据结构
- [ ] Task 1.2: 新增 `SkillContractRegistry`，支持按 `skill_id/node_id` 查询
- [ ] Task 1.3: 新增基础单测（反序列化/默认值/枚举映射）
- [ ] Task 1.4: 添加静态断言与版本号常量

## Phase 2: 配置迁移

- [ ] Task 2.1: 为技能 1-9 增加合同字段（角色约束、节点角色、范围声明）
- [ ] Task 2.2: 编写合同合法性验证（节点数量、互斥、触发上限）
- [ ] Task 2.3: 校验抗性五型分配与文档一致
- [ ] Task 2.4: 添加配置回归测试

## Phase 3: 运行时挂接

- [ ] Task 3.1: SkillSystem cast 前校验 Trigger/Transmuter 约束
- [ ] Task 3.2: 增加 `ScopePolicy` 在伤害流程中的作用域过滤
- [ ] Task 3.3: 增加 Trigger 内置 CD 运行态存储
- [ ] Task 3.4: 剑意/御剑步节点标记在运行时可查询
- [ ] Task 3.5: 关键路径零堆分配回归检查

## Phase 4: UI/存档对齐

- [ ] Task 4.1: UI 节点徽标和范围声明显示
- [ ] Task 4.2: SaveManager 序列化 `skill_contract_runtime`
- [ ] Task 4.3: 加载恢复与重建回归测试
- [ ] Task 4.4: 文档同步（合同字段说明）
- [ ] Task 4.5: 运行 `build.bat` 验证

