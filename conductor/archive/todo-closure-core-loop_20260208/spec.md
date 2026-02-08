# TODO 闭环修复 Track - 技术规格书

## 1. 目标与范围

Track ID: `todo-closure-core-loop_20260208`
类型: Fix
优先级: P0

目标:
1. 完成 `SkillSystem` 中反击伤害占位逻辑，确保行为可验证。
2. 完成 `SaveManager` 存档头部中的角色名与游玩时长采集，去除硬编码占位。
3. 仅处理 `src/` 现存 TODO，不纳入 `conductor/archive/` 的历史任务状态。

非目标:
1. 不重构完整战斗公式体系。
2. 不改动归档 Track 文档、归档计划状态。
3. 不做旧存档兼容改造（允许本 Track 后新逻辑直接生效）。

## 2. 代码语义基线（调研结论）

1. `SkillSystem::Update` 在 `GameplayState` 每帧调用:
- `src/game/states/GameplayState.cpp:510`
- `src/game/systems/skill/SkillSystem.hpp:41`

2. TODO 位于技能事件回调（Phantom Flash 反击）内:
- `src/game/systems/skill/SkillSystem.cpp:223`

3. `SaveManager::createSnapshot` 是角色存档快照入口:
- `src/engine/persistence/SaveManager.cpp:16`
- 异步保存调用它: `src/engine/persistence/SaveManager.cpp:186`

4. 存档触发路径存在于传送系统:
- `src/game/systems/world/PortalSystem.cpp:125`

## 3. 修改边界

允许修改:
1. `src/game/systems/skill/SkillSystem.cpp`
2. `src/engine/persistence/SaveManager.cpp`
3. `src/game/components/` 下新增角色名组件及其必要接入
4. 测试文件（如新增/调整单元或集成测试）

禁止修改:
1. `conductor/archive/**`
2. 渲染、AI、地图系统无关逻辑
3. Save 文件路径、存档目录结构、JSON 顶层 schema

## 4. 设计方案

### 4.1 反击伤害闭环（SkillSystem）

现状问题:
- `OnTakeDamage` 回调里创建了反击执行上下文，但实际伤害计算留空。

方案:
1. 反击伤害必须统一走 `DamagePipeline`，禁止在 `SkillSystem` 中新增独立伤害公式。
2. 仅组织流水线所需输入（攻击者、防御者、技能上下文），不复制战斗计算逻辑。
3. 严禁直接改血（例如直接写 `current_health -= x`）。
4. 增加可测试的事件或日志锚点，便于自动化用例断言反击触发与结算结果。

接口契约:
```cpp
// 不新增外部接口，仅补全 SkillSystem 内部 TODO。
// 伤害结算唯一入口为 DamagePipeline。
// 保证在 attacker 缺失 CombatStats 时安全返回。
```

### 4.2 存档头部信息闭环（SaveManager）

现状问题:
- `data.header.name = "Hero"`
- `data.header.playtime = 0`

方案:
1. 新增 `PlayerName` 组件并挂载到玩家实体。
2. 默认名称固定为 `"玩家0"`；快照写入时优先读取 `PlayerName`。
3. `playtime` 语义明确为“角色生涯累计秒数”（跨会话累积）。
4. 本 Track 不要求兼容旧存档：按新语义直接写入/读取并保持 `header.playtime >= 0`。

接口契约:
```cpp
struct PlayerName {
  std::string value = "玩家0";
};

// CharacterSaveData.header
struct SaveHeader {
  std::string name;
  std::string characterClass;
  int64_t playtime;   // role_lifetime_seconds, >= 0
  std::time_t timestamp;
  int version;
};
```

## 5. 验收标准（AC）

1. AC-SKILL-001:
- 触发 Phantom Flash 反击时，目标受到非 0 伤害；无崩溃、无空指针访问。

2. AC-SKILL-002:
- 对无 `CombatStats` 的 attacker，逻辑安全短路，程序行为稳定。

3. AC-SAVE-001:
- 新存档文件中 `header.name` 来源于 `PlayerName`，默认值为 `"玩家0"`。

4. AC-SAVE-002:
- 新存档文件中 `header.playtime` 为角色生涯累计秒数，非负且可跨会话持续增长。

5. AC-SCOPE-001:
- 本 Track 不修改 `conductor/archive/**`。

6. AC-TEST-001:
- 验收以自动化测试为准，需新增或更新测试覆盖 Skill 反击流水线路径与 Save Header 写入语义。

## 6. 风险与缓解

1. 风险: 反击伤害路径接错，造成重复结算。
- 缓解: 强制统一走 `DamagePipeline`；以自动化测试锁定唯一入口。

2. 风险: 游玩时长来源不稳定。
- 缓解: 明确单一来源并设置回退；写入前 clamp 到 `>=0`。

3. 风险: 角色名组件在不同场景不存在。
- 缓解: 玩家初始化流程强制挂载 `PlayerName{"玩家0"}`。
