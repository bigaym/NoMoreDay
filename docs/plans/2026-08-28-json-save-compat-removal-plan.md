# 计划：JSON 存储兼容移除与二进制单轨收口

- 日期：2026-08-28
- 状态：待评审（Proposed）
- 上游设计：`docs/designs/2026-08-26-item-storage-lifecycle-refactor-design.md`（P0–P6 已完成并通过评审）
- 本计划依据：`docs/workflows/planning.md`
- 前置事实：仓库内已无任何 JSON 存档写入方（`slot_N.json`、`global.json` 均只剩读取兼容；`heirloom_vault.json` 自 P5/P6 起已无写盘路径），本次是纯减法收口。

## 1. 目标与范围裁定

### 1.1 目标

移除全部「旧版 JSON 存档兼容」能力与对应测试，使 `ItemStorageService + ItemPersistenceCodec` 二进制方案成为唯一存档轨道。旧版本存档（含 P0b 之前的 v3 JSON 档）一律不迁移、不导入、不报错崩溃——按「无存档」处理直接进入新游戏。

### 1.2 删除清单（范围内）

| 类别 | 对象 | 位置 |
| --- | --- | --- |
| 旧档导入 | 主档 JSON 分支（v3/v4 `CharacterSaveData` 导入） | `src/game/application/persistence/SaveManager.cpp:825-840` |
| 旧档导入 | `getJsonSavePath` | `SaveManager.cpp:864-869` |
| 旧档导入 | 全局档 JSON 分支（`GlobalSaveData` + `SharedStash::fromJson`） | `SaveManager.cpp:951-986` |
| 版本迁移 | `MigrateSaveDataV3toV4`、`MigrateLegacySpecializedSlots`、版本判断迁移分支 | `SaveManager.cpp:41-`、`:188-196`、`:453-456`、`:627` |
| 无 service 回退 | `createSnapshot` ECS 兼容分支（`ItemFactory::serializeItem` 全套） | `SaveManager.cpp:320-380` |
| 无 service 回退 | `saveCharacterAsync` 从 charData DTO 构建临时 storageSnapshot 的回退 | `SaveManager.cpp:698-732` |
| 无 service 回退 | `saveGlobalAsync` 从 `sharedStash->toJson` 构建快照的回退 | `SaveManager.cpp:1021-1038` |
| 无 service 回退 | `restoreFromSnapshot` 中 gold/capacity 取 `snapshotData` 的回退 | `SaveManager.cpp:603-604` |
| 双轨参数 | `restoreFromSnapshot(..., restoreItems)` 参数及 `restoreItems=true` 全分支 | `SaveManager.hpp:53-57`、`SaveManager.cpp:490-593` |
| DTO 层 | `SaveManager` 三助手：`SerializeItemComponentHelper`、`SerializeHandleToDto`、`RestoreDtoToHandle` | `SaveManager.cpp:54`、`:114`、`:124` |
| DTO 层 | `CharacterSaveData` 物品字段（gold/inventoryCapacity/inventory/equipment/bagSlots/materialBank/personalStash）及 `SerializedInventoryEntry/SerializedBagSlot/SerializedMaterialEntry` | `src/game/foundation/data/SaveData.hpp:76-106` 及同文件结构体 |
| DTO 层 | `SerializedItem.hpp` 整文件（含 legacy 宽松 from_json） | `src/game/foundation/data/SerializedItem.hpp`（225 行） |
| DTO 层 | `ItemFactory::serializeItem / restoreItem` | `src/game/systems/item/ItemFactory.cpp:233`、`:724`；`ItemFactory.hpp:25,29` |
| DTO 层 | `StashData.hpp` 中 `SerializedStashSlot/SerializedStashTab/SerializedStash`（**保留 `StashTabType` 枚举**） | `src/game/foundation/data/StashData.hpp:20-39` |
| JSON 接口 | `SharedStash::toJson/fromJson/suspend/resume` 与 `m_suspendedData`（自述「双轨过渡垫片」） | `src/game/systems/item/SharedStash.hpp:33-40,45`；`SharedStash.cpp:75-144` |
| 门面 | `SerializationSystem.hpp` 整文件（src 无调用方，唯一消费者是测试） | `src/game/systems/SerializationSystem.hpp` |
| 数据结构 | `GlobalSaveData.hpp` 整文件 | `src/game/application/persistence/GlobalSaveData.hpp` |
| 配置残留 | `useItemStore` 字段及 JSON 读写键 | `src/game/foundation/Settings.hpp:32,88,110`；如 `settings.json` 含该键一并清理 |

### 1.3 保留清单（明确出范围，防止误删）

- **progression payload 的 JSON 编码**（`BuildProgressionJson`/`ParseProgressionJson`，`SaveManager.cpp:198-241`）：这是现行 `.nmd` 容器内部的现行格式（设计 D5 明文），不是兼容层。
- `StashTabType` 枚举、`SkillRegistry::SanitizeLoadedSkillSlots`：现行功能。
- `SharedStash` 的实体槽位 API（`putItem/takeItem/getTab`、`m_tabs`）：属 P4 High-06 记录的实体双源长期项，与 JSON 存储兼容无关，本次不动（仅要求验证 `StashSystem.cpp:49-52` 的 `getTab` 分支不依赖被删 JSON 方法）。
- 引擎/渲染/静态资源 JSON（`GPUSkillEffectSystem`、`MaterialManager`、`QualityTierManager`、`LootFilter`、`LootTable`、`MaterialRegistry`、`RunewordSystem` 定义数据等）：非存档持久化。
- `.nmd` 三级结构中的 `.bak` 回退（`Medium 12` 路径前置检查修复保留，不动）。
- 门禁脚本 `scripts/check_legacy_reintroduction.py` 语义为「标记数量只许减少」，删除代码天然 PASS，无需改基线。

## 2. 实施思路与原理

### 2.1 为什么现在能删（决策依据）

1. **写入方已消亡**：`saveCharacterAsync`（`:751-763`）与 `saveGlobalAsync`（`:1048-1052`）只写 `.nmd`；`heirloom_vault.json` 自 P6 起无写盘路径（`HeirloomVault.hpp:155/190` 仅 add/remove）。JSON 档只会越用越旧，导入路径服务的对象不存在增量。
2. **DTO 字段在二进制主路径是死重**：`createSnapshot` 的 storage 分支构建 item DTO（`:273/:281/:288/:313`），但 `BuildProgressionJson` 白名单根本不消费它们——每次存档在做无用功；这些字段唯一运行时消费方是旧 JSON 导入链（`restoreItems=true`）。
3. **用户决策**：本作尚在开发期，明确不做老版本兼容。设计文档 D8 的「保留至少一个发布版本」条款由本计划正式废止（需在设计文档追加修订记录，见 T0）。
4. **无 service 防御回退违反单轨不变式**：P4 High-06 已裁定实体重建路径长期删除；运行时 `ItemStorageService` 由 `Game` 组合根保证注入（`Game.hpp:84-88`、`SharedContext.hpp:39-41`），缺失属编程错误，应**快速失败**而非静默兜底（静默兜底会导致物品无声丢失，比崩溃更糟）。

### 2.2 版本策略（新）

- `CharacterSaveData.header.version`（`CURRENT_CHARACTER_SAVE_VERSION`，现值 4，定义于 `SaveData.hpp`）从「迁移起点」变为「准入门槛」：解析 progression 后 `version != CURRENT` → 该档判死，尝试 `.bak`，仍失败 → 新游戏，LOG_ERROR 说明版本不匹配。
- `ITEM_STORE_BINARY_VERSION=1` 与模板指纹校验维持 codec 现状不动。

### 2.3 存档失败语义（新）

`saveCharacterAsync` / `saveGlobalAsync` 在取不到 `ItemStorageService` 时：`LOG_ERROR` + 返回失败 future，**拒绝存档**。理由：无 service 时存出空物品档 = 静默丢档，宁可失败。

### 2.4 影响面摘要

- 代码净减：预计 ~800+ 行（SerializedItem.hpp 225 + SaveManager 兼容分支 ~180 + ItemFactory serialize/restore ~200 + SerializationSystem 98 + GlobalSaveData 12 + SharedStash JSON ~70 + HeirloomVault JSON ~130 + Settings 3 处 + SaveData 字段/结构 ~40）。
- 测试净减：ItemPersistenceParityTests（JSON 对拍主题消亡）、ItemSaveRoundTripTests 7 个 DTO 往返用例 + 1 个迁移用例（因 ItemFactory serialize/restore 删除，等价语义由 ItemPersistenceCodecTests 覆盖）、ItemTemplateRegistryTests 1 个子用例；净增：4 个负向兼容用例（见 §5）。
- `createSnapshot` 语义变化：仅构建 progression（ECS 组件直取），无物品 DTO；`SaveManagerBenchmark`（tests/performance/SaveManagerBenchmark.cpp，P4 曾对 createSnapshot 1000 物品预算 <1ms）需同步改基准口径为 codec encode/decode + restore。

## 3. 伪代码引导

### 3.1 loadCharacter（删 JSON 层 + 版本准入）

```
loadCharacter(registry, slotIndex):
    storage = GetItemStorageService(registry)
    if !storage:
        LOG_ERROR("ItemStorageService 缺失，拒绝读档")   # 不变式失败，快速失败
        return false

    # 一级：主档 .nmd
    payload = decode(getSavePath(slotIndex))
    if payload ok:
        charData = ParseProgressionJson(payload.progressionJson)
        if charData.header.version != CURRENT_CHARACTER_SAVE_VERSION:
            LOG_ERROR("存档版本 {} 与当前 {} 不匹配，不做旧版兼容", ...)
        else:
            restoreFromSnapshot(registry, charData)      # 单轨，无 restoreItems 参数
            return true

    # 一级半：.bak（逻辑同上，路径换 getBackupPath；成功后写回主档交给下次原子存档，维持 Medium 12 现状）
    ...

    # 兼容提示（可选但建议）：旧 JSON 存在时给出清晰人话
    if fileExists(getJsonSavePath(slotIndex)):
        LOG_WARN("检测到旧版 JSON 存档 {}，不再支持导入，将开始新游戏", ...)

    return false    # 上层 MainMenuState 既有失败→新游戏流程不变（MainMenuState.cpp:110）
```

### 3.2 restoreFromSnapshot（单轨化）

```
restoreFromSnapshot(registry, charData):
    # 删除 MigrateSaveDataV3toV4 / MigrateLegacySpecializedSlots / originalVersion 追踪
    registry.clear(); 重建玩家基础组件           # :459-486 现状保留
    # 无 restoreItems 分支：仅保留 :595-623 单轨镜像挂载
    inv.gold    = storage.getGold()
    inv.capacity= storage.getInventorySlots().size()   # 无 snapshotData 回退
    ...（镜像标量 + stash meta，现状 :595-623）
    挂载 skills/astrolabe/blade_*/combatHistory  # :625-666 现状保留
```

### 3.3 saveCharacterAsync / saveGlobalAsync（收口）

```
saveCharacterAsync(registry, slotIndex):
    m_isSaving 守卫                              # :681 保留
    storage = GetItemStorageService(registry)
    if !storage:
        LOG_ERROR("ItemStorageService 缺失，拒绝存档以防静默丢物品")
        return 失败 future                        # 替换 :698-732 DTO 回退
    storageSnapshot = *storage
    progressionPayload = BuildProgressionJson(createSnapshot(registry)).dump()
    encode + SaveFileAtomic                      # :751-763 现状保留

saveGlobalAsync(...):
    storage 缺失 → 同样拒绝存档                   # 替换 :1021-1038 sharedStash->toJson 回退
    # :1048-1052 掩码 encode 现状保留
```

### 3.4 loadGlobal（删 JSON 层）

```
loadGlobal(ctx):
    decode(global.nmd) → storage 分区灌入 + sharedStash 页数镜像   # :878-911 现状保留
    decode(global.nmd.bak) → 同上                                 # :913-946 现状保留（重复循环是否顺手合并由实现定，非必须）
    if fileExists("saves/global.json"):
        LOG_WARN("检测到旧版 global.json，不再导入")
    sharedStash->initialize()   # 首启正常路径，保留
    return true
```

### 3.5 SerializationSkillSanitizeTests 改写

```
原：SerializationSystem::Load(registry, savePath)   # 走门面 JSON 测试路径
新：j = json::parse(测试内嵌字符串/临时文件)
    SkillRegistry::SanitizeLoadedSkillSlots(j)     # 直测被测函数
    断言未知技能槽被清洗
# 之后删除 src/game/systems/SerializationSystem.hpp，无 src 消费者
```

## 4. 原子任务拆分

每个任务结束必须 `build.bat RelWithDebInfo` 全量构建（**禁止 notest / debug**）+ 跑受影响测试绿，方可勾选。代码注释用中文、面向上下文（不写「本次任务删了什么」的过程性注释）。

- [x] **T0 记忆与设计修订**：查询 memory 相关条目（已完成项标记 superseded）；在 `docs/designs/2026-08-26-item-storage-lifecycle-refactor-design.md` 追加「D8 修订（2026-08-28）」：放弃「旧 JSON 读取器保留至少一个发布版本」条款，改为「旧版存档不兼容，按无存档处理」，注明触发原因（写入方已消亡 + 开发期无存量用户档需求）。记录决策到 memory。
- [x] **T1 SaveManager 旧档导入移除**：删 `loadCharacter` JSON 分支（`:825-840`）、`getJsonSavePath`（`:864-869`）、`loadGlobal` JSON 分支（`:951-986`）、删除 `GlobalSaveData.hpp`；实现 §3.1/§3.4 版本准入（`header.version != CURRENT` 拒载）与旧档检测 LOG_WARN；删 `MigrateSaveDataV3toV4`、`MigrateLegacySpecializedSlots`、`:453-456` 迁移分支、`:627` 调用点。验证：rg 确认 `getJsonSavePath|GlobalSaveData|MigrateSaveDataV3toV4|MigrateLegacySpecializedSlots` 零命中；现有存档相关测试仍绿。
- [x] **T2 无 service 回退收口**：按 §3.2/§3.3 改 `createSnapshot`（删 `:320-380` ECS 分支与 item DTO 构建）、`saveCharacterAsync`（删 `:698-732` 回退，storage 缺失拒绝存档）、`saveGlobalAsync`（删 `:1021-1038`）、`restoreFromSnapshot`（删 `restoreItems` 参数与 `:490-593` 分支、`:603-604` 回退，签名同步改 `SaveManager.hpp:53-57`）。验证：全部 `restoreItems` 引用归零；存档 e2e 测试绿。
- [x] **T3 DTO 层删除**：删 `CharacterSaveData` 物品字段与 `SerializedInventoryEntry/SerializedBagSlot/SerializedMaterialEntry`（`SaveData.hpp`）、`SerializedItem.hpp` 整文件、`SaveManager` 三助手（`:54/:114/:124`）、`ItemFactory::serializeItem/restoreItem`、`StashData.hpp` 的 `SerializedStashSlot/SerializedStashTab/SerializedStash`（保留 `StashTabType`）、`SharedStash::toJson/fromJson/suspend/resume` 与 `m_suspendedData`（同步删 `SharedStash.hpp` 的 `nlohmann/json.hpp` include）。先跑 `rg -n "SerializedItem|SerializeHandleToDto|RestoreDtoToHandle|serializeItem|restoreItem|SerializedStash|toJson|fromJson" src/` 核对无第三方消费者再删。验证：src 全量构建零错误；`StashSystem.cpp` 编译不回归（`:49-52` getTab 分支确认不依赖被删方法）。
- [x] **T4 SerializationSystem 退役**：按 §3.5 改写 `tests/unit/SerializationSkillSanitizeTests.cpp`（测试内直解析 JSON，保留 `SanitizeLoadedSkillSlots` 行为断言）；删除 `src/game/systems/SerializationSystem.hpp`；顺手清理 `InventoryComponent.hpp:41` 过时注释。验证：`rg -n "SerializationSystem" src tests` 仅剩改写后测试文件名变化为零。
- [x] **T5 useItemStore 清理**：删 `Settings.hpp:32` 字段、`:88` 写键、`:110` 读键；检查仓库根 `settings.json` 是否含 `"useItemStore"` 键，有则删。验证：`rg -n "useItemStore" src settings.json` 零命中。
- [x] **T6 测试清理与负向用例**：删 `tests/unit/ItemPersistenceParityTests.cpp`（主题消亡；若其 `$nmd.tests.item` 注册表位置需要占位，并入负向用例文件）；删 `ItemSaveRoundTripTests.cpp` 中 7 个 DTO 往返用例（含 Sparse Inventory 与 Bag Slots/Material Bank DTO 用例）+ 1 个迁移用例；删 `ItemTemplateRegistryTests.cpp:211-222` 子用例；新增 `tests/unit/ItemSaveCompatRemovalTests.cpp`（用例见 §5.2）；全仓 `rg -n "restoreItems|SerializedItem|ItemFactory::serializeItem|SerializationSystem|useItemStore|getJsonSavePath" tests/` 清扫残余引用并修复。验证：`ctest --test-dir build -C RelWithDebInfo -L ci` 全绿。
- [x] **T7 基准复基线与门禁**：更新 `tests/performance/SaveManagerBenchmark.cpp` 对 `createSnapshot` 的口径（progression-only，预算重定；codec encode/decode + restore 预算沿用 P4 结论 9.3/9.4）并跑 perf 套件留证据；`python scripts/check_legacy_reintroduction.py`、`python scripts/check_module_boundaries.py` PASS（预期标记数下降）。
- [x] **T8 实机烟测与评审**：完整构建 `bin/NoMoreDay.exe`（实机二进制，不是只建测试目标）；清理 `HeirloomVault.hpp` 中的 JSON 死代码；烟测剧本见 §5.3；按 `docs/workflows/review.md` 走评审，报告落 `docs/reviews/2026-08-28-json-save-compat-removal-review.md`；memory 记录验证结果与残留风险。

## 5. 测试方法

### 5.1 自动化命令（构建配置 RelWithDebInfo）

```powershell
.\build.bat                                                          # RelWithDebInfo 全量（含 tests，绝不 notest）
ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.item" --output-on-failure
ctest --test-dir build -C RelWithDebInfo -R "SaveManager|Persistence|Serialization|Stash|Settings" --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure    # 收尾全量
ctest --test-dir build -C RelWithDebInfo -L performance -R "SaveManager|Codec" --output-on-failure   # 基准复基线
```

### 5.2 新增负向用例（`tests/unit/ItemSaveCompatRemovalTests.cpp`）

1. `legacy slot JSON is not imported`：向 `getJsonSavePath(0)` 等价路径写入合法 v4 结构 JSON（含物品/gold），`loadCharacter` 返回 false，registry 为初始状态；再次断言**未生成**对应 `.nmd`。
2. `legacy global.json is not imported`：写 `saves/global.json`，`loadGlobal` 后 storage 为初始（空仓、无金）且 `sharedStash->getUnlockedTabCount()` 为初始值。
3. `version-mismatched nmd is rejected with bak fallback`：写一个 progression `header.version = CURRENT - 1` 的 `.nmd`（可由 codec encode 后篡改 progression 字节或构造后重编），主档拒载；同槽放一份合法 `.bak` → 载入成功。无 `.bak` 时 → 新游戏。
4. `save fails fast without ItemStorageService`：无 service 上下文调用 `saveCharacterAsync` → future 为 false 且无新档文件产生（防静默丢档）。

### 5.3 手动烟测剧本

1. 旧档场景：手工放置一份旧 `saves/slot_0.json` 与 `saves/global.json`（无 `.nmd`）→ 启动 → 日志出现「不再支持导入」WARN → 正常进入新游戏无崩溃。
2. 正常闭环：新档游玩 → 获得物品/穿装备/开共享仓页 → 存档退出 → 重进读档 → 物品/金/页解锁逐项比对。
3. `.bak` 恢复：破坏 `slot_0.nmd` 首字节 → 读档走 `.bak` 成功。
4. 快存：副本内 F5 → 读档位界面确认 `quicksave.nmd` 行为与 P4 一致。
5. 观察日志无 `SerializationSystem`/JSON 导入相关 INFO/ERROR 残留。

### 5.4 禁止回归项

- progression 内容（技能/星盘/刀意/战斗史/契约运行时）读写不回归——ItemSaveRoundTripTests 的 e2e 用例必须全绿。
- codec 掩码、原子写、`.bak` 语义、模板指纹校验不动、不测改。

## 6. 验证任务完成（退出标准）

- [x] `rg -n "getJsonSavePath|SerializedItem|SerializedStashSlot|SerializedStashTab|MigrateSaveDataV3toV4|MigrateLegacySpecializedSlots|restoreItems|useItemStore|SerializationSystem|GlobalSaveData" src/` 零命中（`StashTabType`、`SyncLegacySwordIntent` 等无关 legacy 词根除外）。
- [x] `rg -n "slot_.*\.json|global\.json|heirloom_vault\.json" src/` 仅剩 LOG_WARN 提示行。
- [x] `build.bat` 全量构建 + `ctest --test-dir build -C RelWithDebInfo -L ci` 全绿；性能套件基准达标（记录数值进评审报告）。
- [x] §5.3 烟测五步通过，日志证据截图/文本附评审报告。
- [x] `check_legacy_reintroduction.py` / `check_module_boundaries.py` PASS。
- [x] 设计文档含 D8 修订记录；评审按 `review.md` 得出 `提交`。
- [x] memory 已记录：决策（不做旧版兼容）、验证结果、风险残留。

## 7. 风险与回退

| 风险 | 等级 | 缓解 |
| --- | --- | --- |
| 未列全的测试依赖 DTO/restoreItems/ECS 回退 | 中 | T3/T6 前置 rg 清扫 + 每任务全量构建；漏网者按用例语义改写或删除 |
| `SharedStash` 实体槽位与 JSON 方法同文件，误伤存续功能 | 中 | 只删四个声明方法与成员；`putItem/takeItem/getTab/initialize/unlockNextTab` 不动；`StashSystem.cpp:49-52` 编译即验证 |
| 旧 JSON 档玩家数据彻底不可达 | 低 | 用户明确接受（开发期，无发布版本）；LOG_WARN 给出人话提示 |
| `.nmd` 中间开发构建档被误拒 | 低 | 现值 version=4 与 CURRENT 一致；仅 version≠CURRENT 才拒，与「不做旧版兼容」语义一致 |
| createSnapshot 基准口径变化导致预算误报 | 低 | T7 显式复基线并在报告记录新旧数值与理由 |
| 本计划删的是「设计文档写明保留」的能力（D8） | 流程 | T0 设计修订先行，评审时引用该修订记录 |

回退策略：本任务为纯删除，git revert 单提交即可整体回退；不引入任何新格式/新依赖。

## 8. 明确不做（Non-Goals）

- 不实现 JSON 调试导出（设计 Q3/JsonCodec 未来项，另立任务）。
- 不处理 `SharedStash` 实体双源（P4 High-06 长期项）、`InventoryComponent` 实体镜像的进一步瘦身。
- 不动引擎/渲染/静态资源 JSON 与 `LootFilter/MaterialRegistry/RunewordSystem` 等内容数据格式。
- 不做任何形式的存档版本迁移（含未来的版本提升策略另议）。
