# Track Plan: 符文语系统 (Runewords System)

## 1. 目标 (Goal)
实现类似 Diablo 2 的符文语系统：
1. 玩家可以将符文 (Runes) 镶嵌到有孔装备 (Socketed Items) 中。
2. 符文本身提供基础属性。
3. 当按特定顺序镶嵌符文时，如果底材符合条件，将触发“符文语”，赋予装备强大的额外属性。

## 2. 技术规格 (Tech Specs)
- **符文定义**: 符文作为一种特殊的物品 (`ItemType::Rune` 或保留在 `Material` 但可提取)。
- **插槽系统**: `ItemComponent` 已包含 `sockets` 字段，需完善其序列化 (Entity vs UUID) 和逻辑。
- **注册表**: `RunewordRegistry` 加载 `runewords.json` 定义。
- **系统逻辑**: `RunewordSystem` 监听插槽变化，验证公式并应用/移除特效。

## 3. 任务清单 (Tasks)

### Phase 1: 基础设施 (Infrastructure)
- [x] **Task 1.1**: 验证并修复物品插槽的序列化逻辑 (Entity Persistence)。
- [ ] **Task 1.2**: 完善 `assets/data/runes.json` (或复用 `materials.json`) 和 `runewords.json` 数据结构。
- [ ] **Task 1.3**: 实现 `RunewordRegistry` 加载符文语配方。

### Phase 2: 交互与逻辑 (Interaction & Logic)
- [ ] **Task 2.1**: 实现从材料银行“提取”符文为实体物品的功能 (或者让符文直接作为普通物品掉落)。
- [ ] **Task 2.2**: 实现 `SocketSystem`：处理符文与装备的镶嵌交互 (UI Drag & Drop)。
- [ ] **Task 2.3**: 实现 `RunewordSystem`：检测镶嵌序列，应用符文语属性。
- [ ] **Task 2.4**: 实现“底材隐性属性 (Implicit)”系统支持。

### Phase 3: UI 与测试 (UI & Testing)
- [ ] **Task 3.1**: 更新详细信息 UI，显示已镶嵌的符文和激活的符文语名称/特效。
- [ ] **Task 3.2**: 编写单元测试覆盖符文语激活、移除和属性叠加。

## 4. 定义完成 (Definition of Done)
- [ ] 能成功将符文镶嵌到装备上。
- [ ] 正确的符文顺序能激活符文语，错误顺序仅提供符文基础属性。
- [ ] 存读档后镶嵌状态和符文语效果不丢失。
- [ ] 包含至少 3 个测试符文语 (如 Stealth, Spirit 等)。
