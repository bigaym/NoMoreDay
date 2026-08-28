# 物品与存储数据生命周期重构设计（Item Storage & Persistence Refactor）

- 日期：2026-08-26（v2 综合修订）
- 状态：已完成实施（Completed & Verified）
- 范围：物品/背包/仓库数据生命周期、存储与持久化组件化
- 前置文档：`docs/workflows/design.md`；关联审计问题：Item/SharedStash 实体生命周期不匹配（High，2026-08-25 记忆审计）
- 输入方案：本文件 v1 + 两份候选提案——方案A「Universal Storage & Item Engine」（值语义、定长 POD 实例直存槽位）与方案B「Storage Core」（句柄、中央存储池、现状丢数据热修清单）。提案原文已删除，其要点、对比与裁决完整固化于本文 §1A，本文自包含可独立评审

---

## 1. 问题陈述（现状证据）

当前物品与存储体系在**数据模型、生命周期、持久化、UI 读取**四个维度均存在结构性问题。以下每条均有代码证据：

### 1.1 胖组件 + 实体句柄数据模型

- `src/game/foundation/components/ItemComponent.hpp:152`：每个物品一个 ECS 实体 + 一份完整 `ItemComponent`（约 30 字段 + 9 个堆分配容器：`name`、`description`、`setName`、`setBonuses`、`affixes`、`implicits`、`conversions`、`damage_modifiers`、`sockets`）。
- 静态数据（名称、描述、套装、基底词缀模板、贴图 id）在每个实例上重复存储。1000 件同基底武器 = 1000 份重复的字符串与隐匿词缀拷贝。
- 槽位存裸 `entt::entity`（`InventoryComponent.items`、`EquipmentComponent.slots`、`StashTab.items[144]`、`ItemComponent.sockets`），跨帧/跨场景引用无代际保护，悬空句柄靠各处手写 `registry.valid()` 检查（如 `ItemFactory.cpp:277`）。

### 1.2 容器物品实体与 registry 生命周期冲突（审计 High 问题实锤）

- `SaveManager::restoreFromSnapshot`（`src/game/application/persistence/SaveManager.cpp:176-307`）流程：`SharedStash::Get().suspend(registry)` → `registry.clear()`（SaveManager.cpp:181）→ `resume(registry)`。
- `SharedStash::suspend/resume`（`src/game/systems/item/SharedStash.cpp:129-144`）：**纯内存数据用 nlohmann::json 做中转**——suspend 时逐物品 `ItemFactory::serializeItem` 转 DTO 再转 JSON，resume 时反向重建实体。每次读档/过图一次完整的「实体→DTO→JSON 文本→DTO→实体」往返，全部在主线程，伴随海量字符串分配与解析。
- 根因：容器内物品是 registry 实体，场景重载必须销毁 registry，于是被迫序列化逃生。**正确的修法不是优化 JSON 往返，而是让容器物品不依赖 registry 生命周期。**

### 1.3 双轨持久化体系并存

- 轨道 A：`SerializationSystem`（`src/game/systems/SerializationSystem.hpp`，435 行）——F5/F8 触发（`GameplayState.cpp:703`），全量世界序列化到 `saves/quicksave.json`，17 种组件手写 TrySerialize 分支 + 两阶段实体/引用修复，`dump(4)` 美化输出。
- 轨道 B：`SaveManager`（单例）——主线程 createSnapshot（SaveManager.cpp:44-174，逐物品 `ItemFactory::serializeItem` 拷贝成 DTO）→ taskflow 异步 JSON 写 temp + 原子 rename（`saves/slot_N.json`，版本 3）。
- 两套格式、两套实体引用修复逻辑、两套版本管理，维护成本双倍。轨道 A 的全量世界快照与轨道 B 的玩家快照语义重叠。
- 触发点分散：`PortalSystem.cpp:143` 过图自动存（**且 world 层直接调 persistence 单例，破坏分层**）、`MainMenuState.cpp:109` 读档、`Game.cpp:547` 退出存全局。

### 1.4 每帧 UI 快照全量重建

- `GameplayState.cpp:675-677`：每帧 `m_snapshotBuilder.Build(registry, options)` → `m_uiHost->Update(...)`。
- `GameUiSnapshotBuilder.cpp`：每帧执行——
  - `ToItemView`（:113-157）对背包/装备每件物品做值快照，`affixes`/`implicits` 逐个拷贝进 `std::vector`（每物品每帧堆分配）；
  - `inventoryById`/`equipmentById`/`groundById` 三张 `unordered_map`（:413-414、:892）每帧重建；
  - 仓库视图（:488-556）遍历最多 10 页 × 144 槽；
  - 材料银行、地面掉落物、tooltip 字符串填充（R8 路径）同样每帧执行。
- 面板关闭时仍在构建对应视图（SnapshotOptions 只部分门控）。UI 快照架构本身（值快照 + 命令重校验）是正确的边界，问题在于**重建频率与分配成本**，不在架构。

### 1.5 序列化代码重复

- 仓库序列化逻辑存在两份手写实现：`SaveManager::createSnapshot` 的 personalStash 分支（SaveManager.cpp:123-146）与 `SharedStash::toJson`（SharedStash.cpp:75-99），同样的 tab/slot 遍历与 DTO 组装。
- `ItemFactory::serializeItem`（ItemFactory.cpp:227-283）与 `restoreItem`（ItemFactory.cpp:937-1010）逐字段、逐词缀手写转换；restoreItem 还存在「DTO→栈上 ItemComponent→emplace 再拷一次入池」的双重拷贝。

### 1.6 存储场景碎片化（七套并存的存储形态）

| 场景 | 数据形态 | 所有权 | 持久化 |
|---|---|---|---|
| 背包 | `InventoryComponent.items: vector<entity>` | 玩家实体 | slot_N.json（DTO） |
| 装备 | `EquipmentComponent.slots` | 玩家实体 | slot_N.json（DTO） |
| 个人仓库 | `PersonalStashComponent.tabs`（10×144） | 玩家实体 | slot_N.json（DTO） |
| 共享仓库 | `SharedStash` 单例（非 ECS 但持有实体） | 全局单例 | global.json + suspend/resume JSON 中转 |
| 材料银行 | `MaterialBankComponent`（有序 id/count 数组，账户模型） | 玩家实体 | slot_N.json |
| 传家宝宝库 | `HeirloomVault` 单例（`HeirloomData{ItemComponent 完整拷贝}`，上限 20） | 全局单例 | saves/heirloom_vault.json（同步、dump(2)） |
| 地面掉落 | 实体 + Position/Sprite + LootGridSystem | registry（场景级） | 不持久化 |

- 三个单例（`SharedStash::Get()`、`HeirloomVault::Get()`、`SaveManager::Get()`）构成隐藏全局状态：测试难以隔离、关闭顺序风险（`Game::cleanup` 中 saveGlobalAsync → wait_for_all → clear 的顺序依赖）、分层破坏的温床。
- 金币是 `InventoryComponent.gold` 标量而非物品；材料银行刻意绕开物品实体体系（账户模型，`MaterialBankComponent.hpp:39-95` 的 lower_bound 有序数组）——**这证明团队已经在局部用「账户型存储」解决了实体物品的问题，本设计将其推广为统一方案**。

### 1.7 主动丢数据缺陷（方案B 发现，本次已逐条代码验证，比性能问题更紧急）

**serializeItem/restoreItem 丢实例字段**（ItemFactory.cpp:227-283 / :937-1010 已核对）：
- 不保存：`isLocked`、`activeRunewordId`、`conversions`、`damage_modifiers`、`forgingPotential` 之外的动态状态。其中 **`isLocked` 是玩家手动设置且 `SalvageSystem::excludeLocked`（SalvageSystem.cpp:15,163）依赖它防误拆——读档后锁定状态丢失，分解保护失效**；`activeRunewordId` 丢失导致符文之语装备读档后失去词缀激活状态。
- `maxStack`/`bagCapacity`/`isTwoHanded`/`setName`/`setBonuses` 也不落盘，但可由 `baseId` 从模板派生（restore 路径确实这样隐式依赖）——派生依赖目前是隐式的，无校验。

**createSnapshot 丢容器状态**（SaveManager.cpp:44-174 已核对，grep 全文无对应序列化）：
- 不保存 `InventoryComponent::capacity`（背包扩展格数读档回退）、**不保存 `bag_slots`（已装备的 4 个扩展背包实体未序列化，registry.clear 后彻底消失）**、**不保存 `MaterialBankComponent`（材料银行完全不落盘——而拾取路径 InventorySystem.cpp:94 已把材料转入银行，等于玩家攒的材料在存档往返后全丢）**。
- 背包槽位只保存非空物品、恢复时顺序 push + pad null（SaveManager.cpp:222-223）——**玩家整理好的槽位布局读档后丢失（压缩到前部）**。

**双轨不一致的实证**：轨道 A（SerializationSystem，quicksave）反而保存了 MaterialBank（:163/:252）和 bag_slots（:354/:389），轨道 B（SaveManager，主档 slot_N.json）没有——同一份数据两条存档路径语义不同，玩家用 F8 与用主档得到不同的结果。这直接支持 D8 的双轨归一决策。

**结论：存档往返正在丢玩家数据（锁、符文之语、扩展背包、材料银行、槽位布局），这是 P0 级缺陷，必须先于任何架构重构修复（见 §7 Phase 0）。**

---

## 1A. 三方案对比与综合取舍（v2）

对比对象：本设计 v1、方案A（值语义 + 定长 POD 实例直存槽位，原「Universal Storage & Item Engine」提案）、方案B（句柄 + 中央 ItemStore + Phase 0 热修，原「Storage Core」提案）。两份提案原文已删除，本节即其有效内容的完整记录。

### 1A.1 关键维度对比

| 维度 | 方案A（值槽 POD） | 方案B（句柄 Storage Core） | 本设计 v1（SoA+句柄） | 综合采纳 |
|---|---|---|---|---|
| 槽位存储 | `ItemInstance` 值（~192B/槽，空槽也占） | `ItemHandle` 8B | `ItemHandle` 8B | **句柄**：空槽 8B（10 页仓库大多空置）、移动=8B 拷贝、物品有稳定身份（拖拽/打造目标/UI 引用） |
| 实例内存布局 | 定长内联 POD，零堆分配，可 memcpy ✓ | `vector<Affix>` 仍堆分配 ✗ | SoA 列（读改写分离最优但字段演进成本高） | **句柄池 + 定长内联 POD 载荷**（AoS）：方案A 的零分配优点 + 方案B 的单一所有权；序列化仍可整块 memcpy（等价方案A 的 blit 收益） |
| sockets 表示 | `uint32` 宝石 definitionId ✗ | `ItemHandle` ✓ | `ItemHandle` ✓ | **句柄**：已验证符文是完整物品实体（CraftingSystem.cpp:258-299 镶 runeEntity + RunewordSystem 检查），只存 defId 会丢符文自身词缀——方案A 此项错误，否决 |
| 词缀上限 | 硬编码 8（注释「2 固有+4 显性+2 传奇」）✗ 与实测规则不符 | vector 无上限 | SmallVec 待定 | 定长内联，**上限按代码库实测**：rollAffixes 最高 3 前缀+3 后缀（Ancient/Mythic，ItemFactory.cpp:773-777），加隐匿（模板来源）与传奇位 → 内联 12 槽（Phase 0 加创建期断言防溢出） |
| quantity 宽度 | uint16 | int | — | uint16 实例（实测 maxStack ≤ 9999，MaterialRegistry.cpp:48）；**材料银行条目保持 int32**（账户无堆叠上限） |
| 静态/实例分离 | ItemCatalog ✓ | ItemTemplate ✓ | ItemTemplateRegistry ✓ | 三案一致，无争议 |
| 脏标记 | per-Tab dirty mask | per-container revision | 全局 version | **全局 version（UI 门控）+ per-container 脏位（持久化增量）** 两级 |
| UI 读取 | 未深入 | **按需详情**：仅 hover/tooltip/打造目标取 affix ✓ | version 门控复用整快照 | **两者叠加**：门控跳过无变化帧 + 变化帧内也只为 displayedItems 填详情（与既有 R8 注释方向一致） |
| 持久化 | blit memcpy + 脏页 ✓ | **槽位索引保留** ✓ + 全字段 + 脏容器 | section+CRC+模板指纹+temp/rename+bak | **全部合并**：section 化二进制 + CRC + 模板指纹 + 槽位索引 + 脏容器增量 + 原子写 + .bak |
| 加载方式 | `reinterpret_cast` 直读 ✗ | 流式解码 | 版本化 section 解码 | **版本化解码**：裸 reinterpret 在格式演化/对齐/填充上是隐患，否决方案A 此项 |
| 事务 API | Move/Swap/Split/Merge/Transfer/AutoDeposit/原地 Sort + SlotFilterPredicate ✓ 完整 | Put/Swap/Remove 基础 | service 命令（未列全） | **采纳方案A 的完整事务集与槽位过滤器**（过滤器对应现有 StashSystem::canStoreItem 的 Tab 类型约束） |
| 立刻修数据丢失 | ✗ | **Phase 0 热修** ✓（本次已验证属实） | ✗ | **采纳方案B Phase 0**，作为整个路线图的第一步 |
| 单例治理/分层 | 未涉及 | 未涉及 | SharedContext 注入 + PortalSystem 分层修复 ✓ | 保留本设计 |
| 双轨归一 | 未涉及 | 提到「移除或统一」 | SerializationSystem 处置方案 ✓ | 保留本设计（§1.7 双轨不一致实证强化了必要性） |
| 组件物理形态 | 未明确 | **`src/storage/` 独立静态库** ✓ 更彻底 | `src/game/systems/item/storage/` 目录 | 折中为 Q8：先按 game 内目录落地（零依赖约束先行），是否升格独立库对齐既有 `modular-split-exe-lib-dll-design` 另议 |
| 阶段划分 | 3 阶段（大步） | 6 阶段（含热修） | P0-P6（护栏+flag） | 合并：护栏 → **热修** → 模板化 → 池化 → …（见 §7） |

### 1A.2 综合结论

三案在「模板/实例分离、存储态与 ECS 解耦、世界态才用实体、二进制+异步原子写、脏标记」上完全收敛——这些是无需再论证的共识基线。分歧点与最终裁决：

1. **值直存槽位（方案A） vs 句柄间接（方案B/v1）→ 句柄胜**：符文之语要求 socket 内是完整实例（defId 方案丢数据，已验证）；空槽内存 8B vs 192B；身份稳定性服务 UI/打造/审计。方案A 的真正优点（POD 零分配、blit 序列化）通过「池内 POD 载荷 + 整块列写盘」同等获得。
2. **SoA（v1） vs AoS-POD（方案A 布局）→ AoS-POD 胜（v1 自我修正）**：SoA 的读改写分离收益在本访问模式（整物品读、逐字段零星写）下不显著，却让「整池 memcpy 快照、与旧 DTO 对拍、字段演进」都更贵。定长 POD 池同样零堆分配，且编码就是一个 `static_assert(sizeof)+memcpy`。
3. **方案B 的 Phase 0 是全场最高性价比步骤且被验证为真**——玩家数据正在丢失，任何架构工作不得排在它前面。
4. 方案A 的 `reinterpret_cast` 直读与其 sockets 设计被否决；其事务 API 面、脏页思想、状态机文档（世界态⇋存储态）被吸收。
5. 本设计 v1 独有且保留：单例治理、双轨归一、性能预算与验收标准、迁移令牌、feature flag、.bak 回退、模板指纹校验。

---

## 2. 目标与非目标

### 目标

- **G1 统一存储组件**：一个独立、可测试的 `ItemStorage` 组件，统一承载全部七类存储场景的运行时数据（背包/装备/仓库/共享仓库/材料/宝库/地面待拾取）。
- **G2 解耦 registry 生命周期**：容器物品不再依赖 `entt::registry` 存活，从根上消灭 suspend/resume JSON 中转与悬空实体句柄。
- **G3 持久化单轨化**：归一到 SaveManager 轨道，物品编码升级为版本化二进制（JSON 保留为调试/导出/迁移格式），消除双轨。
- **G4 减少拷贝**：静态/实例数据分离（模板化）、SOA 紧凑存储、稳定代际句柄、UI 快照脏驱动增量重建。
- **G5 高性能可测量**：全部路径有 benchmark 与预算（沿用 `tests/performance` 的 `LOG_BENCHMARK` 模式 + Tracy 区段）。
- **G6 明确合同**：数据所有权、生命周期、修改通道（写路径收口）、跨系统访问规则写成可执行约定。

### 非目标

- 不改变任何玩法规则：掉落、词缀滚动、装备校验、仓库解锁经济、UI 呈现与交互流程全部保持现状语义。
- 不引入新第三方依赖（二进制编解码手写；序列化库评估仅在未决问题 Q6 讨论）。
- 不做云端/网络存档、跨平台字节序（当前仅 win64，固定 little-endian）。
- 不重构技能/星盘/战斗历史等非物品持久化字段（但新编码容器必须可扩展承载它们）。
- 不在 v1 提供物品回收站/误删恢复（Q4）。

---

## 3. 架构总览

```
┌─────────────────────────── 应用层（组合根）───────────────────────────┐
│ Game / SharedContext（注入，替代全部单例）                             │
│   ├─ ItemStorageService   （本设计核心，拥有全部物品运行时数据）        │
│   ├─ SaveManager          （消费 ItemStorageService，只做编码与 IO）   │
│   └─ entt::registry       （场景实体：玩家、敌人、地面物品实体、投射物）│
└──────────────────────────────────────────────────────────────────────┘
         │ 读(视图)                          │ 写(命令)
┌────────▼───────────────────────────┐  ┌───▼──────────────────────────┐
│ UI: GameUiSnapshotBuilder          │  │ Gameplay: InventorySystem /   │
│     （脏驱动增量，读 SOA 视图）       │  │ StashSystem / CraftingSystem  │
│     Controller/Renderer 不变        │  │ （改调 ItemStorageService）     │
└────────────────────────────────────┘  └──────────────────────────────┘
```

### 3.1 核心组件（新增目录 `src/game/systems/item/storage/`）

| 文件 | 职责 |
|---|---|
| `ItemStorageTypes.hpp` | `ItemHandle`、`SlotRef`、`ContainerId`、`StorageError`、各容量常量 |
| `ItemTemplateRegistry.hpp/.cpp` | baseId → 不可变 `ItemTemplate`（静态数据单一来源） |
| `ItemStore.hpp/.cpp` | 句柄池 + 定长 POD 实例载荷 + 代际管理 + 容器槽位表（核心，无 IO、无 raylib/json 依赖） |
| `ItemMigration.hpp/.cpp` | 场景迁移令牌（结构化所有权转移，无序列化） |
| `ItemPersistenceCodec.hpp/.cpp` | 版本化二进制编码器/解码器（+ JsonCodec 调试/导出适配） |
| `ItemStorageService.hpp/.cpp` | 门面：组合上述能力，暴露读视图与写命令；由 Game 持有 |

依赖方向：`ItemStore` 零外部依赖（纯数据结构，可独立单测）；`ItemStorageService` 依赖 `ItemStore` + `ItemTemplateRegistry` + codec；`SaveManager` 依赖 service 的只读视图。**物品存储位于 foundation/systems 交界，不依赖 application 层，满足现有分层。**

---

## 4. 详细设计（决策 D1–D8）

### D1. 模板/实例分离（消灭静态数据重复）

- `ItemTemplateRegistry`：启动时从现有 `ItemFactory` 内部基底表（`BaseItemDef`，ItemFactory.cpp:288 起）与词缀定义表构建，产出不可变模板：

```cpp
struct ItemTemplate {            // 不可变，共享所有权
  uint32_t baseId;               // 全局唯一基底 ID（现 ItemComponent.baseId）
  ItemKind  kind;                // 静态分类（type/slot/isTwoHanded/weaponSubtype/catalystKind 折叠）
  uint32_t textureId;
  uint32_t maxStack;
  int      minLevel;
  StatRanges baseRanges;         // attack/defense 基础范围
  SmallVec<AffixDefId, 3> implicits;   // 隐匿词缀引用（定义在词缀表）
  SetId    setId;               // 套装引用（名称/加成查套装表）
  NameId   nameId;              // 本地化键，运行期解析为字符串视图
};
```

- `ItemInstance` 只存滚动结果：模板引用 + 稀有度、数量、词缀数组、孔位、潜能、锁、时间戳等动态字段。
- 名称/描述/套装名不再逐实例存储。UI 侧 `GameUiSnapshotBuilder` 的 R8 注释已表明「名称经模板表解析」正是既定方向（GameUiSnapshot.hpp:59-63），本设计将其落到数据层。

### D2. ItemStore：句柄池 + 定长 POD 载荷（v2：吸收方案A 内联布局，放弃 v1 的 SoA）

```cpp
struct ItemHandle { uint32_t index; uint32_t gen; };   // 8B，null = {0,0}

// 实例载荷：紧凑 POD，零堆分配，可整块 memcpy（吸收方案A，AoS 而非 SoA）
struct alignas(8) ItemInstance {
  uint64_t instanceId;          // 稳定实例标识（打造/审计/UI 引用）
  uint32_t baseId;              // 模板引用（静态数据唯一来源）
  uint32_t quantity;            // uint32 宽度（实测 maxStack≤9999，余量充足）
  uint16_t itemLevel; uint8_t rarity; uint8_t flags;   // isLocked/isTwoHanded 等位标志
  int16_t forgingPotential; uint8_t legendaryPotential; uint8_t socketCount;
  float attack, defense, value;
  uint8_t affixCount;
  std::array<CompactAffix, 12> affixes;   // 显性 3+3 + 隐匿 + 传奇，上限按实测规则（§1A）
  std::array<ItemHandle, 6> sockets;      // 句柄，保符文实例完整（否决方案A 的 defId）
  uint32_t activeRunewordId;
  // 溢出罕见路径：conversions/damage_modifiers 等不定长字段入 side-table（稀疏 map），
  // 常规物品零分配；side-table 条目计入 dirty 与序列化。
};
static_assert(std::is_trivially_copyable_v<ItemInstance>);
```

- 池：`std::vector<ItemInstance>` 连续数组 + 空闲链表 + `gen` 代际回收（方案B 模型）。
- v1 的 SoA 列布局撤销，理由见 §1A.2-2：AoS-POD 保留全部零分配收益，同时快照=整块 memcpy、与旧 DTO 对拍、字段演进都更便宜。
- `ItemStore` 提供：`create(roll) / get(Handle) -> const ItemInstance* / mutate(Handle, fn) / destroy(Handle) / visit(range)`；`get` 代际失配返回 nullptr 并计数 telemetry。
- **收益**：单实例 ≤ ~200B 定长、零堆分配（对比现状 ≥300B + 最多 9 次分配）；快照冻结 = 池引用计数（或双缓冲，Q5），10k 实例 memcpy ≈ 2MB < 1ms。

### D3. 统一容器模型（七场景一套寻址）

```cpp
enum class ContainerKind : uint8_t {
  Inventory, Equipment, BagSlots, PersonalStash, SharedStash,
  MaterialBank, HeirloomVault, GroundPending
};
struct SlotRef { ContainerKind kind; uint8_t container; uint16_t page; uint16_t index; };
```

- 背包/装备/仓库/共享仓库/宝库槽位统一为 `ItemHandle` 数组，挂在 `ItemStorageService` 拥有的容器表上（玩家私有容器以玩家存档 id 关联，跨存档容器以全局作用域关联）。
- `MaterialBank` 与 `HeirloomVault` 并入统一模型：材料条目本就是 (id, count) 账户（MaterialBankComponent 现状保留语义）；传家宝是带元数据的 ItemHandle 槽（上限 20 保持）。
- **物品不再是容器内 ECS 实体**。移动物品 = 两个 SlotRef 的句柄交换 + 一次 dirty 标记，零拷贝零分配。
- 地面掉落保留实体形态（需要 Position/Sprite/物理/渲染），但实体只挂轻量 `GroundItemComponent{ ItemHandle handle; }`；拾取 = 句柄从 `GroundPending` 移入 `Inventory`，实体销毁。`LootGridSystem`/`GPULootSystem`/`DropSystem` 改读该组件。
- **命令面（v2，采方案A 事务集）**：service 写命令 = `Move / Swap / SplitStack / MergeStack / Transfer(跨容器) / AutoDeposit(自动合并+寻空槽) / Sort(原地零分配) / Destroy`，全部事务语义（校验失败零副作用或完全回滚）；槽位准入用 `SlotFilterPredicate`（无虚函数，对应现有 `StashSystem::canStoreItem` 的 Tab 类型约束，如材料页拒装备）。

### D4. 场景迁移令牌（消灭 suspend/resume JSON）

- `registry.clear()` 不再影响任何容器数据（容器物品无实体）。
- 唯一需要跨场景处理的是 `GroundPending`：过图时执行显式策略（默认：未拾取地面物品随场景丢弃并回收句柄，与现行为一致；如需保留则移入「场景暂存容器」）。策略经 `ItemMigration::beginSceneSwitch(policy) -> MigrationToken` 显式声明，结束后 `endSceneSwitch(token)`。
- 存档读入：解码器直接重建 ItemStore 内容与槽位表（二进制顺序读 → memcpy 级），**不存在「先清空再逐实体重建」阶段**。旧档（JSON v3）走一次性导入路径（D8）。
- 验收钩子：场景切换路径上不再出现任何 nlohmann::json 构造（可加编译期/运行时断言测试）。

### D5. 持久化编码：版本化二进制 + 异步 + 原子（保留既有可靠性机制）

- 格式：`[magic][version][section table][payload...]`，little-endian，每 section 附 CRC32。首版 `ITEM_STORE_VERSION = 1`。
- Section 划分：模板表指纹（baseId 集合哈希，用于校验游戏版本兼容）、实例池（SOA 原始列块）、容器槽位表、元数据（金币、解锁页、页签名）。
- 写盘：沿用现有可靠性机制——主线程拿一致快照（池冻结/双缓冲）→ taskflow 异步编码写 temp → 原子 rename（SaveManager 现有模式，SaveManager.cpp:309/411 不变）。加载失败回退 `.bak`（新增：rename 前备份旧档一版）。
- **槽位索引保留（v2，采方案B）**：容器 section 按 `slotIndex + handle` 落盘（修复 §1.7 布局压缩缺陷的格式层保证），空槽显式占位。
- **脏容器增量（v2，采方案A 脏页 + 方案B 脏容器）**：全局 version 之外，每容器脏位；存档时未脏容器的 section 引用上次编码块（copy-on-write 块表），仅重编码脏容器。城镇整理 1 页仓库 → 只写 1/10 的仓库 section。
- **主线程快照成本**：因 SOA 列块本身就是紧凑连续内存，「拿快照」= 引用计数冻结或列块双缓冲交换，目标 10k 实例 < 1ms（对比现状 createSnapshot 1000 件 ≈ 10ms 量级，tests/performance/SaveManagerBenchmark.cpp:145 预算 < 10ms）。
- JSON 保留为 `JsonCodec`（调试导出、玩家可读档案、迁移源），同一 `ItemPersistenceCodec` 接口。
- 技能/星盘等其余 CharacterSaveData 字段暂留 JSON 编码，容器格式为后续统一预留 section 位（非目标，但不断路）。

### D6. UI 快照脏驱动 + 按需详情（v2：方案B 的懒填充与本设计门控叠加）

- `ItemStore` 维护全局 `version`（任何写命令递增）+ 按 `ContainerKind` 的脏位。
- `GameUiSnapshotBuilder::Build` 增加前置门：面板未开启的容器直接跳过（现状已部分具备 options 门控，补全 stash/materials/ground 的门控）；`version` 未变且 options 未变时复用上一次视图（含 `displayedItems` tooltip 字符串）。
- **按需详情（v2）**：列表/网格视图只填标量字段 + `ItemHandle`，**不为每个物品拷贝 affixes/implicits 向量**；仅 hover/拖拽/tooltip/打造目标等「当前展示详情」的物品（即 `displayedItems`）才从 ItemStore 取词缀数据填充——与 GameUiSnapshot.hpp:59-63 既有的 R8 设计注释方向一致。
- 视图向量复用：snapshot 对象由 host 持有并复用，向量 capacity 跨帧保留（可选 `std::pmr::monotonic_buffer_resource`，若评审认为引入 pmr 复杂度过高则退化为普通复用）。
- 收益：无物品变化帧物品视图构建成本 0；有变化帧也只为少数展示中物品付详情成本（对比现状每帧全量 affix 拷贝 + 三张 hash map 重建）。
- `GameUiItemView` 结构与 Controller/Renderer 合同**不变**（UIInventoryController.cpp 等零改动或极小改动），符合「UI 只读快照 + 命令重校验」的既有 remediation 边界。

### D7. 所有权与生命周期合同（替代单例）

- `Game` 组合根构造 `ItemStorageService`，经 `SharedContext`（src/game/foundation/SharedContext.hpp:27）注入：新增 `ItemStorageService* itemStorage` 字段。`SharedStash`/`HeirloomVault` 单例降级为 service 内部数据 + 兼容门面（过渡期），最终删除。
- 写路径收口：物品数据的结构修改只允许经 service 命令接口（create/move/split/merge/destroy/mutate）；游戏性系统（InventorySystem/StashSystem/CraftingSystem/SalvageSystem/RunewordSystem）从直改组件改为调用命令。读路径开放：`ItemView` 只读访问随处可用。
- 生命周期时序（替代 Game::cleanup 的隐式顺序）：
  1. 启动：构造 service → 载模板表 → 读 global 档（共享仓库/宝库）；
  2. 对局：service 常驻，场景切换仅触发 D4 迁移令牌；
  3. 过图/退出：SaveManager 从 service 只读视图编码落盘（PortalSystem 不再直呼 SaveManager 单例，改为经 SharedContext 发存档请求，修复分层破坏）；
  4. 关闭：显式 `service.shutdown()`（flush + 释放），顺序由组合根保证。
- 测试性：`ItemStore`/codec 可在无 raylib、无 registry 的单测环境中全量测试（对比现状：物品逻辑测试必须先搭 entt registry 与纹理桩）。

### D8. 兼容与迁移（旧档无损）

- 保留现有 JSON 读取器为迁移源：slot_N.json v3 / global.json / heirloom_vault.json → 首次加载时导入 ItemStore → 下次保存写二进制 v1。旧读取器保留至少一个发布版本。
- 迁移链沿用既有模式（参考 `MigrateLegacySpecializedSlots`，SaveManager.cpp 内 v<3 处理）。
- `SerializationSystem`（轨道 A）处置：F5/F8 快捷键重定向到 SaveManager 轨道（快捷全量存档）；全量世界序列化代码移除（其 debug 价值由 JsonCodec 导出替代）。详见 Q3。

> **D8 修订（2026-08-28）**：
> 正式废止「旧 JSON 读取器保留至少一个发布版本」条款，改为「旧版存档不兼容，按无存档处理」。
> 触发原因：写入方已全量消亡（所有写盘路径已统一为 `.nmd` 二进制），且处于开发期无存量发布用户档需求，彻底移除 JSON 读取兼容与 DTO 冗余字段，实现单轨持久化。


---

## 5. 数据所有权与跨系统合同（行为规则）

| 数据 | 所有者 | 写通道 | 读通道 |
|---|---|---|---|
| 物品静态模板 | ItemTemplateRegistry（不可变） | 仅启动加载/热重载工具 | 任意只读 |
| 物品实例 | ItemStore | service 命令 | ItemView 任意只读 |
| 容器槽位 | ItemStorageService | service 命令（move/swap） | 视图迭代 |
| 金币 | Inventory 容器元数据 | service 命令 | 视图 |
| 存档字节流 | SaveManager | 异步编码任务 | 加载路径 |
| UI 快照 | GameUiHost | SnapshotBuilder（唯一写者） | Controller/Renderer 只读 |
| 地面物品表现 | registry 实体（GroundItemComponent 持句柄） | DropSystem/拾取命令 | 渲染/AI |

- 悬空防护：句柄代际校验是唯一合法性判据，`get()` 失配返回空视图并计入 telemetry（对比现状散落的 `registry.valid()`）。
- 线程模型：主线程写；异步编码任务只读冻结快照；UI 快照构建在主线程（现状不变）。
- 命令失败必须显式（`StorageError`：满/不存在/类型不符/锁保护），禁止静默丢物品——现状 `InventorySystem` 各 bool 返回语义保持并映射到此错误码。

---

## 6. 对现有系统的影响与性能预算

### 接口影响（按调用面排序）

| 系统 | 改动 | 规模估计 |
|---|---|---|
| InventorySystem / StashSystem / SharedStash / CraftingSystem / SalvageSystem / RunewordSystem / DropSystem / FragmentDropSystem | 实体操作改句柄命令 | 最大头；逐系统小步替换，行为测试兜底 |
| GameUiSnapshotBuilder | 门控 + 脏驱动 + 视图源切换 | 中 |
| SaveManager | createSnapshot/restore 改读 ItemStore；PortalSystem 解耦 | 中 |
| ItemFactory | 生成走模板表 + ItemStore::create；serializeItem/restoreItem 转为迁移用 | 中 |
| HeirloomVault / SharedStash 单例 | 门面化后删除 | 小 |
| SerializationSystem | 移除（快捷键重定向） | 小（删代码） |
| UIInventory/UIStash/UICrafting Controller | 基本零改动（快照合同不变） | 极小 |

### 存档资产影响

- 新增二进制档（`saves/slot_N.nmd` 等，命名待 Q7）；旧 JSON 档只读导入，不删除；global.json / heirloom_vault.json 同策略。
- 存档体积估算：JSON ~N KB → 二进制 ~N/4（字符串与键名消除）；1000 件物品档预计从数百 KB 降至数十 KB（验收时实测）。

### 性能预算（新增/沿用 benchmark，全部进 `tests/performance`）

| 指标 | 现状预算 | 新预算 |
|---|---|---|
| SaveManager createSnapshot 1000 件 | < 10ms（现有） | < 1ms（冻结快照） |
| restoreFromSnapshot 1000 件 | < 15ms（现有） | < 2ms（二进制直读） |
| 场景切换共享仓库交接 | JSON 往返（无预算、有分配风暴） | ≈ 0（所有权转移，0 分配） |
| 无物品变化帧的物品视图构建 | 每帧全量 | 0 |
| ItemFactory batch create 1000 | < 5ms（现有，保持） | < 2ms（模板+池） |
| 新增 ItemStoreBenchmark（create/destroy/move/visit 10k） | — | move < 10ns/次、visit 10k < 0.1ms |
| 新增 UiSnapshotBenchmark（脏/净帧） | — | 净帧物品部分 = 0 |

Tracy 区段：`ItemStore::*`、`SnapshotBuild.Items`、`Codec.Encode/Decode` 全覆盖（符合 performance 工作流要求）。

---

## 7. 实施阶段（供后续 planning 工作流细化）

- **P0a 护栏**：新增 ItemStore/UiSnapshot benchmark 与 Tracy 区段；记录现状基线（不改行为）。
- **P0b 数据丢失热修（v2 新增，采方案B，最高优先级，不改架构）**：
  1. `serializeItem/restoreItem` 补齐实例字段：`isLocked`、`activeRunewordId`、`conversions`、`damage_modifiers`（派生项 maxStack/setName 等保持模板隐式恢复，但加校验日志）；
  2. `createSnapshot/restoreFromSnapshot` 补齐：`InventoryComponent::capacity`、`bag_slots`（含包内物品递归）、`MaterialBankComponent`；
  3. 背包/仓库存档改带 `slotIndex`（或恢复期保布局），`CURRENT_CHARACTER_SAVE_VERSION` 3→4 + 迁移分支；
  4. 回归测试：满背包+扩展包+材料银行+带孔/符文之语/锁定物品的 save/load round-trip（字段级断言）。
  风险最低、立刻止血玩家数据丢失，且这些修复在新架构落地后依然有效（迁移导入路径复用同一 DTO）。
- **P1 模板化**：ItemTemplateRegistry 落地；ItemComponent 静态字段改为模板解析（保留字段兼容读取，标记 deprecated）；ItemFactory 生成走模板。
- **P2 ItemStore + 句柄**：POD 池落地；Inventory/Equipment/Stash/SharedStash/MaterialBank/Vault 容器切句柄；适配层保持 InventorySystem 签名不变（内部转发）；地面物品挂 GroundItemComponent。
- **P3 迁移令牌**：删除 suspend/resume JSON；场景切换 0 序列化断言测试。
- **P4 编码单轨**：二进制 codec + SaveManager 接入 + 旧档导入 + F5/F8 重定向；删除 SerializationSystem 全量世界轨。
- **P5 UI 增量**：门控 + version 复用 + 按需详情填充。
- **P6 收尾**：单例删除、PortalSystem 分层修复、文档与术语表更新。

每阶段独立可验收、可回退（feature flag：`settings->useItemStore` 期间保留旧路径直至 P4 通过）。

---

## 8. 验收标准（可观察、可验证)

1. **P0b 止血生效（v2 新增）**：锁定物品、符文之语物品、扩展背包（含包中物品）、材料银行、背包槽位布局，save/load round-trip 后字段级/位置级一致（新增回归测试断言）；旧 v3 档读入不丢新字段默认值。
2. **行为不变**：现有测试套全部通过且不修改断言——StashSystemTest、InventoryDragSwapTests、UIInventoryControllerTests、UIStashControllerTests、ItemEquipValidationServiceTest、SerializationSkillSanitizeTests、ItemFeedbackP3Tests、ItemLevelScalingTest。
3. **场景切换零序列化**：新增测试断言过图/读档路径无 nlohmann::json 构造与 ItemFactory::serializeItem 调用（桩/计数器验证）。
4. **性能达标**：§6 预算表全部由 `LOG_BENCHMARK` 输出达标；Tracy 采集净帧物品构建区段为空。
5. **存档往返**：二进制存→读→字段级一致（round-trip 属性测试：随机生成 N 件含孔/套装/锁物品，save/load 深比较）；旧 JSON v3/v4 档导入结果与旧代码加载一致（对拍测试）。
6. **损坏恢复**：截断/翻转字节的档位加载失败且回退 .bak（新增故障注入测试）。
7. **手动冒烟**：城镇→副本→过图→退出重进→共享仓库存取→传家宝存取全流程物品无损（保留证据于 docs/reviews）。

---

## 9. 未决问题

- **Q1 词缀/孔位上限（v2 部分解决）**：显性词缀实测上限 = 3 前缀 + 3 后缀（Ancient/Mythic，ItemFactory.cpp:773-777），传奇词缀叠加路径与隐匿词缀（模板来源）上限待查后固化 `CompactAffix[12]` 与 `sockets[6]` 常量；P0b/P2 加创建期溢出断言。**仍阻塞 P2 的常量固化。**
- **Q1b quantity 宽度（v2 新增，基本解决）**：实测 maxStack ≤ 9999（MaterialRegistry.cpp:48 默认、药水 99、碎片 1），实例 uint32 已留足余量；材料银行 count 保持 int32 不设堆叠上限。
- **Q2 地面物品过图策略**：丢弃（现行为）还是暂存？倾向 v1 丢弃保持语义，`GroundPending` 容器结构已预留扩展。
- **Q3 F5/F8 语义**：重定向为「快捷全量存档」后是否还需要「全世界调试导出」？倾向 JsonCodec `--export-world` 调试开关替代。
- **Q4 物品回收站**：v1 非目标，destroy 命令预留审计钩子（telemetry 记录）以便未来追溯。
- **Q5 双缓冲粒度**：冻结快照用引用计数（读侧克隆池）还是双缓冲整池？P4 前用 benchmark 决定（10k 实例 ≈ 2MB，双缓冲内存代价可接受）。
- **Q6 编码手写 vs 库**：本设计默认手写（零依赖），若评审认为维护成本高可评估轻量 header-only 方案（需过 tech-stack 流程）。
- **Q7 存档命名与目录**：`saves/slot_N.nmd`？`saves/v4/slot_0/store.bin`？与现有 temp 目录约定对齐，planning 时定。
- **Q8（v2 新增）组件物理形态**：方案B 的 `src/storage/` 独立静态库 vs 本设计的 `src/game/systems/item/storage/` 目录。倾向先按 game 内目录落地（零外部依赖约束先行），升格独立库与 `docs/designs/modular-split-exe-lib-dll-design.md` 的拆库计划合并决策，避免两次搬移。
- **Q9（v2 新增）side-table 稀疏字段范围**：`conversions`/`damage_modifiers` 出现频率需统计（grep 调用面），确认放稀疏 map 的命中率与序列化代价；若高频则提为定长内联。

## 10. 风险与取舍

| 风险 | 缓解 |
|---|---|
| 物品调用面广（30+ 文件） | 适配层小步替换；P2 全程保持 InventorySystem 对外签名不变；行为测试兜底 |
| SOA 字段演进（加列=格式变更） | section 化 + version + 迁移链（既有模式）；模板表指纹校验 |
| 双写期不一致（P2–P4 过渡） | feature flag 单路径运行；每阶段 round-trip 对拍 |
| 二进制档不可读 | JsonCodec 调试导出保留；`--export-world` 工具 |
| 物品实体语义丢失影响隐藏依赖（渲染/AI/tooltip 直取 ItemComponent） | P2 前用调用图全量清点 ItemComponent 读取点（codebase-memory graph）；地面物品保留实体覆盖最大隐藏面 |
| 内存驻留上升（ItemStore 常驻） | 上限 + 池预保留可配；对比现状实体+胖组件净占用预计下降 |

## 11. 术语表

| 术语 | 定义 |
|---|---|
| ItemHandle | 代际稳定物品句柄（index+gen），替代裸 entt::entity 的物品引用 |
| ItemTemplate | 基底静态数据（不可变，按 baseId 共享） |
| ItemInstance | 模板 + 滚动动态状态的单件物品记录 |
| ItemStore | SOA 实例池与代际管理核心 |
| SlotRef / ContainerKind | 统一容器寻址（背包/装备/仓库/……） |
| MigrationToken | 场景切换的结构化所有权转移令牌（无序列化） |
| Codec | 版本化持久化编解码器（Binary 为主，JSON 为调试/迁移） |
