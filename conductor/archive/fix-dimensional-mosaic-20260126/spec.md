# Technical Specification: Fix Dimensional Mosaic System

## 1. Overview
This track addresses several critical bugs in the Dimensional Mosaic (map splicing) system, focusing on UI clarity, progression flow, and state persistence.

## 2. Technical Requirements

### 2.1 词缀聚合与描述引擎 (Affix Aggregation & Description)
- **Component**: `MapAffixRegistry`, `MapAffixCalculator`
- **Requirement**: 将多个来源的同类词缀合并为单条显示。
- **Implementation**:
    - **聚合逻辑**: 在 `MapAffixCalculator` 中新增 `AggregateAffixes(const std::vector<MapAffix>&)`。
        - 输入：原始词缀列表（包含重复类型）。
        - 输出：`std::vector<AggregatedAffix>`。
        - 结构定义：
          ```cpp
          struct AggregatedAffix {
              MapAffixType type;
              float totalValue;
              int maxTier;
              std::vector<std::string> sources; // 记录来源以便在 Tooltip 中显示
          };
          ```
    - **描述模板**: 为 `MapAffixDefinition` 增加描述模板，支持占位符（如 `{value}`）。
    - **格式化**: `MapAffixRegistry::FormatDescription(type, value)` 返回最终渲染文本。

### 2.2 字体与 UI 润色 (Font & UI Polish)
- **Component**: `UISystem`, `MosaicEditorState`
- **Requirement**: Fix missing characters (`•`) and improve layout.
- **Implementation**:
    - Add `0x2022` to `UISystem` font codepoints.
    - Update `MosaicEditorState::RenderTooltip` to:
        - Use `GetDescriptionZh`.
        - Calculate height dynamically based on the number of affixes.
    - Update `MosaicEditorState::HandleInput` to ensure ESC always pops the state, even if the player is still inside a portal hitbox.

### 2.3 维度状态演化 (Dimensional State Evolution)
- **触发条件**: 玩家在异界内进入 `PortalType::NextLevel` 传送门。
- **演化算法**:
    1. **碎片衰减**: 遍历 `ActiveDimensionalState.mosaicGrid` 中的所有碎片。
        - 将所有碎片的 `remainingLayers` 减 1。
        - **移除逻辑**: 如果碎片 `remainingLayers <= 0`，将其从网格中永久移除。
    2. **属性重算**: 调用 `MapAffixCalculator::GenerateAffixesFromGrid`。
        - 根据残余的碎片重新生成聚合词缀列表。
    3. **难度递增**: 增加 `currentDepth`。
        - 按照 `最终加成 = 基础值 * (1.0 + Depth * 0.1)` 更新生效数值。
    4. **条件检测**:
        - 如果网格内已无任何碎片，或 `currentDepth >= maxDepth`，标记维度任务为 `Completed`。
        - 传送门目标改为 `Scene::Town`。

### 2.4 状态持久化与恢复 (Persistence & Restoration)
- **核心载体**: `ActiveDimensionalState` (位于 `registry.ctx()`)。
- **持久化契约**: 必须确保 `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` 宏包含以下字段：
    - `isActive` (bool): 维度任务是否正在进行。
    - `currentDepth`, `maxDepth` (int): 当前/最大深度。
    - `difficultyScore` (int): 总难度。
    - `explicitAffixes` (std::vector): **当前生效的聚合词缀列表**。
    - `mosaicGrid` (MosaicGrid): 记录残余碎片的布局，用于进阶时的损耗计算。
    - `killCounter` (int): **新增**：当前层级的击杀进度（若有任务需求）。
- **场景切换协议**:
    - **离开 (To Town)**: `SaveManager` 自动序列化 `ctx()` 中的状态到 `global.json`。
    - **返回 (To Rift)**: `SceneManager::ApplyLoadedLevel` 检测到场景标签为 `Rift` 时，**禁止重新生成 `ActiveDimensionalState`**，必须从 `ctx()` 恢复现有数据。

### 2.5 聚合词缀的动态加成公式
- Follows DOD (Data-Oriented Design) via EnTT.
- UI remains distinct from logic but reads from `ActiveDimensionalState`.
- Persistence uses `nlohmann/json`.

## 4. Risks
- **Persistence Bloat**: Serializing a 3x3 grid of entities is fine as long as we only store the *data* of the fragments if needed, but currently `ActiveDimensionalState` stores the `MosaicGrid`. We need to ensure `MosaicGrid` describes what was there (e.g., item types/rarities) rather than raw pointers/runtime IDs.
