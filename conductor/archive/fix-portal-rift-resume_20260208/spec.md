# 维度裂隙进度覆盖修复 Track - 技术规格书

## 1. 目标与范围

Track ID: `fix-portal-rift-resume_20260208`  
类型: Fix  
优先级: P0

目标:
1. 修复“存在进行中的裂隙时，玩家通过城镇 Dimensional Gate 误开新局并覆盖进度”的数据丢失问题。
2. 当玩家从城镇恢复裂隙时，回到“原回城点”（进入城镇前在裂隙内的离开位置）。
3. 增加“继续当前裂隙 / 开始新裂隙”的二次确认流程，防误触覆盖。
4. 修复 `PortalSystem::SpawnTownPortal` 中的 EnTT 迭代期销毁实体风险（UB/崩溃隐患）。
5. 修改 3 层完成后的流程：不强制退出到城镇，允许在维度地图重新拼接；旧裂隙实例销毁。

非目标:
1. 不重构 Portal 渲染架构（`PortalSystem::Render` 仍保持现状）。
2. 不持久化怪物逐个运行时状态（允许怪物按同层 seed 重刷）。
3. 不引入重量级快照系统（仅保存最小必要恢复信息）。

## 2. 代码语义基线（调研结论）

1. Dimensional Gate 触发后在 `GameplayState` 中统一处理:
- `src/game/systems/world/PortalSystem.cpp:111`
- `src/game/states/GameplayState.cpp:453`

2. 当前处理逻辑总是进入 `DimensionalLevelSelectState`，随后进入 `MosaicEditorState`:
- `src/game/states/GameplayState.cpp:492`
- `src/game/states/DimensionalLevelSelectState.cpp:200`

3. `MosaicEditorState::ConfirmAndGenerate` 会无条件重写 `ActiveDimensionalState` 的关键字段:
- `src/game/states/MosaicEditorState.cpp:602`
- `src/game/states/MosaicEditorState.cpp:622`

4. `ActiveDimensionalState` 已包含继续裂隙所需状态（`isActive/currentDepth/biome/gridSnapshots`）:
- `src/game/components/WorldState.hpp:22`

5. `SpawnTownPortal` 当前在遍历 view 时直接 destroy，同类视图存在迭代失效风险:
- `src/game/systems/world/PortalSystem.cpp:260`

6. 当前 3 层完成后会直接回城:
- `src/game/systems/world/PortalSystem.cpp:194`

## 3. 修改边界

允许修改:
1. `src/game/states/GameplayState.cpp`
2. `src/game/states/MosaicEditorState.cpp`
3. `src/game/systems/world/PortalSystem.cpp`
4. `src/game/components/WorldState.hpp`（最小恢复字段）
5. `src/engine/scene/SceneManager.cpp`（恢复落点）
6. `tests/**` 下新增或调整与本 Track 直接相关的测试
7. `conductor/tracks/fix-portal-rift-resume_20260208/{spec.md,plan.md}`
8. `conductor/tracks.md`

禁止修改:
1. `conductor/archive/**`
2. 与本问题无关的战斗/渲染/AI核心逻辑
3. 存档目录结构及 `ActiveDimensionalState` JSON 顶层字段集合

## 4. 技术方案

### 4.1 Dimensional Gate 增加“继续/新开”二次确认

在 `GameplayState` 处理 `PendingDimensionalGateTag` 时：
1. 若无进行中裂隙：保持原流程进入 `DimensionalLevelSelectState`。
2. 若有进行中裂隙：弹出确认对话框（`Resume / Start New`）。
3. 选择 `Start New` 时再弹一次确认（明确“会销毁当前进度”）。

```cpp
enum class DimensionalGateAction : uint8_t {
  OpenNewRunDirectly = 0,    // 无活动裂隙时
  OpenResumeOrNewDialog = 1, // 有活动裂隙时
};

enum class ActiveRiftDecision : uint8_t {
  Resume = 0,
  StartNewRun = 1,
  Cancel = 2,
};

static DimensionalGateAction ResolveGateAction(entt::registry& registry) {
  if (!registry.ctx().contains<ActiveDimensionalState>()) {
    return DimensionalGateAction::OpenNewRunDirectly;
  }
  const auto& state = registry.ctx().get<ActiveDimensionalState>();
  if (state.isActive && !state.isCompleted) {
    return DimensionalGateAction::OpenResumeOrNewDialog;
  }
  return DimensionalGateAction::OpenNewRunDirectly;
}
```

行为契约:
1. `Resume`：恢复到 `state.biome + state.currentDepth`，并在进入地图后回到 `lastExitPosition`。
2. `Start New`：清理旧裂隙状态，再进入选层 + 拼图流程。
3. `Cancel`：不做状态变更。
4. 全路径都写日志；所有阻断/危险动作都给 UI 弹窗提示。

### 4.2 回城点恢复与轻量重建策略

为避免保存过多信息，采用“位置恢复 + seed 重建”：

```cpp
struct ActiveDimensionalState {
  bool isActive = false;
  BiomeID biome = BiomeID::None;
  int currentDepth = 1;
  uint32_t seed = 0;
  bool isCompleted = false;
  Vector2 lastExitPosition{0.0f, 0.0f}; // 新增: 记录回城前在裂隙内的位置
  int selectedBaseLevel = 1;            // 保持当前地图选择
  // ...其余字段保持
};
```

规则:
1. 玩家通过 Town Portal 离开裂隙时，记录 `lastExitPosition`。
2. 恢复裂隙时仍使用原 `seed/currentDepth` 生成该层，怪物允许重刷（同 seed、无需保存运行时怪物快照）。
3. 关卡载入完成后，将玩家位置设为 `lastExitPosition`（若不可走则回退到最近可走点）。

### 4.3 MosaicEditor 覆盖防护（防御式兜底）

`MosaicEditorState::ConfirmAndGenerate` 增加前置校验和提示：
1. `isActive && !isCompleted` 时拒绝覆盖。
2. 记录警告日志。
3. 显示弹窗提示“已有进行中的维度裂隙，请先继续或放弃后再新建”。

### 4.4 3 层完成后改为“重拼接续跑”

当前行为是 `currentDepth > maxDepth` 后强制回城；改为：
1. 先弹窗提示“本轮维度挑战已完成”，提供两个动作：`继续重拼接` / `回城`。
2. 选择 `继续重拼接`：
3. 标记本轮完成并销毁旧裂隙实例数据（旧 gridSnapshots/sourceGrid 等清理）。
4. 保留 `selectedBaseLevel`（当前地图选择）。
5. 进入 `MosaicEditorState`，允许重新拼接下一轮。
6. 新一轮确认生成后覆盖为新的裂隙状态（此处允许覆盖，因为旧轮已完成并显式销毁）。
7. 选择 `回城`：
8. 标记本轮完成并销毁旧裂隙实例数据，然后执行 `RequestTransition(Town)`。

### 4.5 Town Portal 生成的 EnTT 安全修复

`PortalSystem::SpawnTownPortal` 改为两阶段删除：

```cpp
std::vector<entt::entity> toDestroy;
for (auto entity : view) {
  if (view.get<PortalComponent>(entity).type == PortalType::Town) {
    toDestroy.push_back(entity);
  }
}
for (auto entity : toDestroy) {
  if (registry.valid(entity)) registry.destroy(entity);
}
```

约束:
1. 不在遍历 `view<PortalComponent>()` 时直接 `destroy`。
2. 保持“同一时间仅一个 Town Portal”的既有语义不变。

## 5. 数据与持久化契约

`ActiveDimensionalState` 仅新增最小恢复字段，不保存怪物细节：

```cpp
struct ActiveDimensionalState {
  bool isActive;
  BiomeID biome;
  int currentDepth;
  int maxDepth;
  uint32_t seed;
  Vector2 lastExitPosition; // 新增
  int selectedBaseLevel;
  bool isCompleted;
  std::array<FragmentSnapshot, 9> gridSnapshots;
  // ... 其余既有字段不变
};
```

不变量:
1. 当 `isActive == true && isCompleted == false` 时，Dimensional Gate 必须先经过“继续/新开”确认。
2. `Resume` 必须使用同层 seed 重建并尝试恢复到 `lastExitPosition`。
3. `ConfirmAndGenerate` 在进行中状态下不得覆盖；仅当“已完成并销毁旧轮”时允许新一轮生成覆盖。

JSON 示例（轻量恢复）:

```json
{
  "isActive": true,
  "biome": 2,
  "currentDepth": 3,
  "maxDepth": 3,
  "seed": 12345678,
  "lastExitPosition": { "x": 1420.5, "y": 880.0 },
  "selectedBaseLevel": 47,
  "isCompleted": false,
  "gridSnapshots": [
    { "hasFragment": true, "remainingLayers": 1 },
    { "hasFragment": false, "remainingLayers": 0 }
  ]
}
```

## 6. 验收标准（AC）

1. AC-RIFT-001  
存在进行中的裂隙（`isActive=true,isCompleted=false`）时，触发 Dimensional Gate 必弹出“继续/新开”确认。

2. AC-RIFT-002  
在确认中选择 `继续` 后，玩家回到 `currentDepth` 且落点恢复到原回城点（不可走时回退最近可走点）。

3. AC-RIFT-003  
在确认中选择 `新开` 时，必须二次确认；取消后二次确认不应产生任何状态变更。

4. AC-RIFT-005  
即使异常进入 `MosaicEditorState`，在进行中裂隙状态下执行 Confirm 也不会覆盖当前 `ActiveDimensionalState`，并会出现日志 + 弹窗提示。

5. AC-RIFT-004  
3 层完成后不强制回城，必须弹窗提供“继续重拼接/回城”选项；选择继续时进入重拼接并保留 `selectedBaseLevel`，新一轮开始前旧轮数据已销毁。

6. AC-RIFT-006  
3 层完成弹窗中选择“回城”时，应正确回城并销毁旧轮裂隙数据（不得保留可恢复的旧轮活动态）。

7. AC-SAFE-001  
`SpawnTownPortal` 中不再出现“遍历 view 时销毁同类实体”的写法。

8. AC-TEST-001  
优先补齐单测/集成测，覆盖 Gate 二次确认、回城点恢复、Mosaic 防覆盖、3 层后完成弹窗（继续/回城）、Town Portal 两阶段删除。

## 7. 风险与缓解

1. 风险: 回城点坐标在地图重建后可能落入不可走区域。  
缓解: 加入“最近可走点回退”逻辑，且记录日志便于排查。

2. 风险: 二次确认流程引入新的 UI 状态分支，可能与现有 StateManager 流冲突。  
缓解: 采用最小状态机（仅 Gate 对话框内消化），不跨系统共享临时状态。

3. 风险: 完成 3 层后“重拼接续跑”可能导致旧裂隙残留数据泄漏。  
缓解: 在进入重拼接前显式销毁旧轮数据并加测试断言“旧轮不可访问”。
