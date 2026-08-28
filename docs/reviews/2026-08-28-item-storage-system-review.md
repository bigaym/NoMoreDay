# 物品与存储系统重构 全面审查报告（四角度并发）

- 日期：2026-08-28
- 审查目标：物品与存储数据生命周期重构（P0a–P6 + JSON 兼容层移除单轨化）的系统级全面评审
- 结论：**提交**（第 3 轮复核后；首次审查与第 2 轮跟进审查结论均为「修改」，见文末各轮小节）
- 审查轮次：首次审查（系统级全面评审，独立于既有 P0 / P1-P3 / P5-P6 / json-compat-removal 轮次）；第 2 轮：修复验证跟进审查（同日，结论「修改」，打回 N1）；第 3 轮：N1/N2/N4/M7 修复复核（同日，结论「提交」）
- 审查标准：`docs/workflows/review.md`；硬规则引用 `conductor/code_standard.md`

## 1. 输入

- 设计：`docs/designs/2026-08-26-item-storage-lifecycle-refactor-design.md`（v2 综合修订版，D1–D8）
- 计划：`docs/plans/2026-08-26-item-storage-lifecycle-refactor-plan.md`（T-P0a-1 ~ T-P6-5 全勾选）
- JSON 移除计划/评审：`docs/plans/2026-08-28-json-save-compat-removal-plan.md`、`docs/reviews/2026-08-28-json-save-compat-removal-review.md`
- 基线：`docs/reviews/2026-08-26-item-storage-baseline.md`
- 验证证据：四路独立专项审查（逻辑正确性 / 功能完整性 / 性能 / 内存崩溃安全）+ 主审对最高严重度发现的源码逐条复核（见 §5 复核记录）

## 2. 变更文件边界

`git status --short`：工作区干净，变更已全部提交。审查范围为 `8f7708a1..HEAD` 六个提交：
`b45f680e`（P0a/P0b）→ `d9301e77`（P1-P3）→ `dc31393d`（注释中文化）→ `3f7c5a8e`（P4 codec）→ `f7633f77`（P5-P6）→ `d486c3fc`（JSON 单轨化）。合计 66 文件 +8524/-2046。

检视过的核心变更：
- 新子系统 `src/game/systems/item/storage/`（18 文件）：`ItemStore`、`ItemStorageTypes.hpp`、`ItemStorageService`、`ItemStorageConverter`、`ItemStorageAdapter`/`IItemStorageAdapter`、`ItemStorageComponents.hpp`、`ItemPersistenceCodec`、`ItemMigration`、`ItemTemplateRegistry`、`ItemTemplate.hpp`
- 持久化：`src/game/application/persistence/SaveManager.{hpp,cpp}`、`src/game/foundation/data/SaveData.hpp`
- UI 读路径：`src/game/application/ui/GameUiSnapshotBuilder.{hpp,cpp}`、`GameUiHost.cpp`、`GameUiSnapshot.hpp`
- 组合根：`src/app/Game.{hpp,cpp}`、`src/game/foundation/SharedContext.hpp`
- 退役/瘦身：`src/game/systems/SerializationSystem.hpp`（整删）、`src/game/systems/item/{SharedStash,HeirloomVault,ItemFactory}`、`src/game/systems/world/PortalSystem.cpp`
- 测试：`tests/unit/Item*`（7 套件）、`tests/unit/GameUiSnapshotBuilderTests.cpp`、`tests/performance/{ItemStore,SaveManager,UiSnapshot}Benchmark.cpp`

## 3. 范围对齐

- 计划勾选属实性抽查通过：T-P4-2 内存 Section 增量缓存（`ContainerDirtyFlags` + `InMemorySectionCache` 真实现）、T-P4-4 SerializationSystem 退役 + F5/F8 重定向（`GameplayState.cpp:704-728` → `saveCharacterAsync(registry,0)` / `loadCharacter(registry,0)`）、T-P4-7 `useItemStore` 全仓 0 命中、T-P5-2 version 缓存复用、T-P5-3 三张临时哈希表移除、T-P6-1 单例 `Get()` 清零。
- 事务 API 七项（Move/Swap/Split/Merge/Transfer/AutoDeposit/Sort）在 `ItemStorageService.hpp:43-53` 齐备且各有测试；设计承诺的 SlotFilterPredicate 缺失（见发现项 M1）。
- D8 决策变更（旧 JSON 档不再导入、按无存档处理）有完整文档修订与 `ItemSaveCompatRemovalTests.cpp` 负向测试锁定，属**有记录的有意决策**，不计缺陷；其玩家侧影响列入剩余风险。
- 范围外观察：运行时交互主路径（移动/整理等）实际仍走 ECS 轨（`InventorySystem.cpp:1036`、`StashSystem.cpp:353`），ItemStore 轨当前主要承载持久化与仓库域，`ItemStorageAdapter::GetDefaultAdapter` 无运行时调用方——存储轨的最终接线范围需在后续 Track 中显式声明。

## 4. 质量与风险评估（总评）

核心内存安全设计扎实：句柄池代际校验使 destroy 后残留句柄全程无 UAF（get/mutate/再 destroy 一致被 isValid 拦截）；异步保存为主线程值语义深拷贝快照 + Taskflow 任务全值捕获 + `Game::cleanup` 的 `wait_for_all()`（`Game.cpp:548`）与成员声明顺序保证退出时序安全；读档采用「CRC 全量通过才 `clearAll` 灌入」的事务性结构；全目录无裸 new/delete、无裸 std::thread、无 dynamic_cast（§5.2/§8.1/§6.1 合规）。

主要风险集中在三类：
1. **持久化边界数据一致性**：SharedStash 被「角色档快照 + global.nmd」双源写入且加载顺序相互覆盖，正常游玩序列即可造成共享仓库物品永久丢失或复制（Blocker B1）；
2. **对抗畸形输入的健壮性**：decode 分配前无上界校验、灌入无句柄校验，CRC 自洽的损坏/恶意档可稳定触发 OOM→terminate（H2），global 档双损被静默重置（H3）；
3. **测试防护与设计合同的「删除未对齐」**：JSON 轨移除后 round-trip 用例 452→174 行，MaterialBank 持久化断言、套装 setHash 重算、Inventory 稀疏布局防护随之消失且无等价替代（H5、M9）；SlotFilterPredicate 为设计承诺与实现间唯一显式功能缺口（M1）。

## 5. 主审复核记录

四路专项审查的最高严重度发现经主审直接读源码复核确认：
- `SaveManager.cpp:387` `*storage = std::move(loadedStorage);`（loadCharacter 整体替换，含 SharedStash 容器态）→ B1 成立；
- `SaveManager.cpp:536-539` 主/备档均损坏后 `sharedStash->initialize(); return true;` → H3 成立；
- `ItemPersistenceCodec.cpp:477` `sectionData[i].resize(headers[i].length);` 与 `:540` `rawEntries.reserve(activeCount);` 均在内容校验前按文件声明值分配，decode 全程无 try/catch → H2 成立；
- `GameUiSnapshotBuilder.cpp:465-473` 缓存 key 比较字段无材料版本输入 → H4 成立；
- `rg "MaterialBank" tests/unit/ItemPersistenceCodecTests.cpp` = 0 命中 → H5 成立；
- `rg "SlotFilterPredicate"` src+tests = 0 命中；`rg "useItemStore"` src+tests = 0 命中；`beginSceneSwitch|addGroundPending` 在 storage 目录外无调用者 → M1/M11 及范围观察成立。

## 6. 发现项（按严重度排序）

### Blocker

**B1 SharedStash 双源真相：角色档快照覆盖 global.nmd，正常游玩序列可造成共享仓库物品永久丢失或复制**
- 位置：`src/game/application/persistence/SaveManager.cpp:341-342,387`、`src/app/Game.cpp:277-278`；设计 `docs/designs/2026-08-26-item-storage-lifecycle-refactor-design.md:269`
- 现象：角色档以 `dirtyMask=All` 编码，SharedStash 段被写入每个角色档（角色与共享仓库共用同一 `ItemStorageService`）。启动时序：`loadGlobal` 载入共享仓库 → 进入游戏 `loadCharacter` 执行 `*storage = std::move(loadedStorage)` 整体替换。推演：上次角色存档后在共享仓库放入物品 P2 → 退出（global.nmd 含 P2）→ 重启（loadGlobal 载入 P2）→ loadCharacter 用旧角色档快照覆盖 → P2 从活体状态消失 → 再次退出时 global 被旧快照回写 → **P2 永久丢失**；反向操作序列（放入→存档→取出→退出）则产生**物品复制**。设计 269 行明确「global 档承载共享仓库」，与现状矛盾。
- 为何成问题：review.md 严重度定义「损坏数据」即 Blocker；数据丢失/复制在正常操作序列可达，非边缘构造。
- 修复建议：角色档 encode 排除 SharedStash section（dirtyMask 去掉该位）；或 loadCharacter 恢复后从 global 重新同步 SharedStash。多角色/多 slot 场景下必须取前者。

### High

**H1 decode 对 CRC 自洽的畸形存档无分配上界，bad_alloc 未捕获导致 terminate**
- 位置：`src/game/systems/item/storage/ItemPersistenceCodec.cpp:477,540`；`src/game/application/persistence/SaveManager.cpp:379`（无 try-catch 包裹 decode）
- 现象：`sectionData[i].resize(headers[i].length)` 与 `rawEntries.reserve(activeCount)` 按文件内 uint32 声明值分配（sectionCount≤64 但单个 length 可声称 4GB；activeCount×≈208B 可声称 858GB），发生在任何内容校验之前；CRC 只证明文件自洽，不证明内存预算合法。异常外溢至 `loadCharacter` → `std::terminate`。
- 为何成问题：§2.2 稳定性要求；可被损坏/恶意存档文件稳定触发的崩溃面。
- 修复建议：`length ≤ 流实际剩余字节数`、`activeCount ≤ 段剩余字节/sizeof(entry)`、`personalUnlocked/occupied ≤ 容器容量上限`；`loadCharacter` 外层捕获 `std::exception` 走「读档失败保留原状态 + .bak 回退」分支。

**H2 decode 灌入阶段不校验句柄与结构有效性，悬垂/重复句柄静默入槽**
- 位置：`src/game/systems/item/storage/ItemPersistenceCodec.cpp:764-798`、`src/game/systems/item/storage/ItemStorageService.cpp:598-605`（setSlotHandle 仅查槽位边界）、`src/game/systems/item/storage/ItemStore.cpp:196-201`
- 现象：阶段 3 直接 `setSlotHandle`，不验证 `store.isValid(h)`，失败返回值被忽略（未来容量调整时物品静默蒸发）；sockets 嵌套句柄无存在性校验；重复 index 使 `m_activeCount` 虚高（`:201` 先于 `:204` 过滤赋值），将导致 `ItemMigration.cpp:143` 守恒校验永久误报；空闲槽 gen 一律重置 1，与运行时 destroy 单调递增语义不一致，「读档→槽位复用」序列下旧 gen=1 句柄可复活。Inventory capacity 字段写而不读（`:597-598`）。
- 为何成问题：破坏「句柄有效=可解引」不变量；CRC 只保证段内完整，不保证 ItemInstances 段与容器段间一致。
- 修复建议：灌入前独立 pass：校验句柄 isValid、index 去重、sockets 存在性，非法置 `{0,0}` 并 LOG_WARN；空闲槽 gen 延续或设偏移；activeCount 按实际插入数累计；capacity 参与校验或删除。

**H3 loadGlobal 主档与备份均损坏时静默重置共享仓库并返回 true**
- 位置：`src/game/application/persistence/SaveManager.cpp:459-539`（双损路径 `:536-539`）
- 现象：主档 decode 失败 → .bak decode 失败 → `sharedStash->initialize(); return true;`。上层拿到 true 视为成功，玩家无任何提示丢失整个共享仓库。
- 为何成问题：review.md 硬否决条款 4「隐藏失败、吞掉必要诊断」；与 `loadCharacter` 失败返回 false 的语义不一致。
- 修复建议：区分「文件不存在（首次初始化，返回 true 合理）」与「存在但损坏（显式 LOG_ERROR + 可提示的三态返回）」。

**H4 saveGlobalAsync 将全池物品实例写入 global.nmd，跨角色边界数据泄漏**
- 位置：`src/game/application/persistence/SaveManager.cpp:561,584-588`；`src/game/systems/item/storage/ItemPersistenceCodec.cpp:71-84`
- 现象：`snapshot.getStoreMutable() = storage->getStore()` 拷贝整个池，`ItemInstances` 段编码全池（visit 不区分归属），global.nmd 因此包含玩家背包/装备/传家宝等私有实例，尽管 `loadGlobal` 只读 SharedStash 段。global 体积随角色物品数线性膨胀；多角色时等同私有数据交叉泄漏。
- 修复建议：global 快照构造改为仅遍历 SharedStash 槽位句柄、`create` 到干净池（与 `loadGlobal` 重建逻辑对称），再编码。

**H5 MaterialBank 容器在 codec 层 round-trip 零测试断言**
- 位置：`tests/unit/ItemPersistenceCodecTests.cpp`（rg "MaterialBank" 0 命中）、`tests/unit/ItemSaveRoundTripTests.cpp`（现存仅 setGold 三处）
- 现象：codec 编码管线包含 MaterialBank section（`ItemPersistenceCodec.cpp:366-390`），但该 section 的编解码正确性（数量、字节布局、恢复）无任何测试；`ItemStorageServiceTests.cpp:204` 只测运行时记账。材料银行是核心经济资源，section 若有 bug 现有测试全盲。
- 修复建议：CodecTests「Round-Trip Full Service」补 MaterialBank 槽位与数量恢复断言。

### Medium

**M1 SlotFilterPredicate 未实现，sortContainer 仅覆盖 3/7 容器且显式丢弃 container 参数**
- 位置：`src/game/systems/item/storage/ItemStorageService.hpp:51-52`、`ItemStorageService.cpp:491-503`
- 现象：`rg "SlotFilterPredicate"` 全仓 0 命中；`sortContainer` 只挂 Inventory/PersonalStash/SharedStash，`(void)container;` 丢弃参数误导调用者；Equipment/BagSlots/HeirloomVault/MaterialBank 无排序能力。设计事务 API 承诺（Sort+SlotFilterPredicate）与实现不符且计划未记录裁剪。
- 修复建议：补齐谓词与容器支持，或在设计/计划中显式记录裁剪决策并从签名移除无效参数。

**M2 mergeStack 仅按 baseId+rarity 匹配，实例差异字段静默丢失**
- 位置：`src/game/systems/item/storage/ItemStorageService.cpp:368-371,392-397`
- 现象：不比对 itemLevel/affixes；maxStack>1 且实例有差异（如 itemLevel 不同的材料堆，影响分解产出）时合并并保留 to 侧字段，from 侧差异随 destroy 丢失。测试未覆盖差异实例场景。
- 修复建议：mergeStack 增加 itemLevel 与词缀非空比对，不一致返回 TypeMismatch 走 swap；补测试。

**M3 净帧 UI「构建成本=0」不成立：缓存命中路径仍每帧深拷贝 4 个容器**
- 位置：`src/game/application/ui/GameUiSnapshotBuilder.cpp:487-491`、`GameUiSnapshot.hpp:89-96`
- 现象：命中分支 `snapshot.inventory = m_cachedInventory` 等值拷贝（元素含 vector/string 成员）。缓解事实：P5 精简后常规项 affixes/name 为空，仅外层 vector 每帧分配，故 `<0.05ms` 断言可过；但容器增大后预算无余量，且违反 §2.1 净帧禁堆分配的字面承诺。
- 修复建议：命中路径改复用机制（const 指针/写时复制），或至少外层 reserve+帧间缓冲复用。

**M4 StashSystem::sortTab 比较器使用字符串比较分支，违反 §7.2**
- 位置：`src/game/systems/item/StashSystem.cpp:371-380`（调用方 `GameUiCommandHandler.cpp:912`）
- 现象：整理路径 std::sort 比较器执行 `name`（std::string）比较 + 每次比较 2 次 `try_get<ItemComponent>` 稀疏集查找；432 槽全排序数千次字符串比较。零分配合规但规则被突破且无授权记录。
- 修复建议：对齐 `ItemStorageService::sortContainer`（baseId 排序）或 decorate-sort-undecorate 预取排序键。

**M5 异步保存链路同档数据最多 4 份驻留，增量 Section 缓存机制整体闲置**
- 位置：`src/game/application/persistence/SaveManager.cpp:339-348,583-594`、`ItemPersistenceCodec.cpp:332-427,353-355`
- 现象：stringstream→`str()`→vector 三重全量拷贝 + encode 内部段驻留；全部调用方传 `cache=nullptr, dirtyMask=All`，T-P4-2 的增量能力从未被真实执行；缓存命中分支还按值拷贝 payload 抵消增量收益。
- 修复建议：encode 增加直写 `std::vector<uint8_t>` 重载；接入真实脏掩码或显式移除机制；命中路径改 `shared_ptr<const vector>` 引用。

**M6 SaveFileAtomic 降级路径非原子、无 fsync、.bak 失败带病继续**
- 位置：`src/game/systems/item/storage/ItemPersistenceCodec.cpp:844-861,874-892`
- 现象：主路径 rename 在 MSVC 下为 `MoveFileExW(REPLACE_EXISTING)` 原子✓；但降级 `copy_file(overwrite_existing)` 中途失败/掉电 → target 损坏；`out.flush()` 仅到 OS 缓冲无 `FlushFileBuffers`；`.bak` 创建失败仅 LOG_WARN 后继续覆盖，极端时序新档旧档同损。
- 修复建议：降级改「copy 到第二 temp→rename」；写入后 fsync 等价物再 rename；备份失败中止本次保存保留旧档（fail-safe 优于带病覆盖）。

**M7 套装 setHash 重算与 Inventory 稀疏布局两项回归防护随 d486c3fc 删除且无等价替代**
- 位置：`tests/unit/ItemSaveRoundTripTests.cpp`（452→174 行；"Set Item Hash Automatic Recalculation"、"Sparse Inventory Slot Layout Preservation" 被删）
- 现象：现存测试中 setHash 0 命中；稀疏槽断言仅存 PersonalStash（CodecTests:123-124,239-245），Inventory/Equipment 无槽位索引保持断言。P0b 建立的数据丢失防护之一消失。
- 修复建议：补套装哈希重算断言与 Inventory 含空洞槽位索引恢复断言。

**M8 预算硬门禁不一致：核心性能承诺无 CI 保护**
- 位置：`tests/performance/ItemStoreBenchmark.cpp:80-85,137-140,172-175,202-205,235-238`、`SaveManagerBenchmark.cpp:196,237-238,266-267,283-284`
- 现象：move<10ns、freeze<0.2ms、restore<2ms 等超标仅 LOG_WARN；仅 UI 基准有 `CHECK` 硬断言。预算退化为打印数字。
- 修复建议：move/freeze/restore 三项改 `CHECK`（阈值放宽 2-3× 抵消机器抖动），与 UI 基准口径统一。

**M9 ItemMigration/D4 迁移令牌与 GroundPending 完全未接线（死代码+恒真校验）**
- 位置：`src/game/systems/item/storage/ItemMigration.cpp:53,69`、`ItemStorageService.cpp:735-748`、`ItemStorageComponents.hpp:12`；场景切换实际走 `PortalSystem.cpp:144-145`→`requestSave`
- 现象：begin/end/addGroundPending 除定义外零调用者；`GroundItemComponent` 无使用点；D4 的句柄回收保证在当前路径恒为空操作，未来接入掉落系统时若绕过 begin/end，防护全部旁路且以「迁移失败」形式才暴露。
- 修复建议：在 SceneManager 切换点接入 begin/end（DEBUG 断言可见），或显式记录为后续 Track 范围并移除恒真校验的虚假安全感。

### Low

- **L1** autoDeposit 对 MaterialBank/HeirloomVault 恒返回 ContainerFull 死路径（`ItemStorageService.cpp:450-477`；账户制无槽位，未来接入易误判「银行满」）→ 入口显式拒绝不支持的 targetKind。
- **L2** loadGlobal 主/备两段 ~35 行复制粘贴、重建不销毁槽位原句柄（二次调用产生池孤儿）、恒 return true（`SaveManager.cpp:459-539`）→ 提取 `restoreSharedStashFrom` 私有函数。
- **L3** TemplateFingerprint 仅覆盖 baseId 集合，模板数值改动不可检测；registry 空时静默跳过校验（`ItemPersistenceCodec.cpp:54-61,525`）→ 指纹纳入模板数值哈希；count==0 时 LOG_WARN。
- **L4** ItemStorageAdapter 静态 fallback 单例跨 Game 生命周期悬垂 service 指针（`ItemStorageAdapter.cpp:7-29`，当前无调用方）→ 每次由 ctx 构造或 Game 结束时显式重置。
- **L5** clearContainer(GroundPending) 只清列表不销毁句柄，泄漏式状态（`ItemStorageService.cpp:785-786`）→ 复用 `clearGroundPending(true)` 或删分支。
- **L6** canStoreItem 在 null service 时返回 true，掩盖装配错误（`ItemStorageAdapter.cpp:109-113`）→ 返回 false + 一次性 LOG_ERROR。
- **L7** move<10ns 可达性存疑：move 链含 unordered_map 模板查找 + atomic RMW，cache miss 已超预算；存储轨当前无真实负载检验（`ItemStorageService.cpp:236-266`、`ItemTemplateRegistry.cpp:62-68`）→ 模板查找改密集直索引；基准补冷缓存场景。
- **L8** ItemStore::visit 为 O(池容量) 而非 O(活跃数)，长期游玩空洞率上升后 encode 同步受害（`ItemStore.hpp:100-113`）→ 维护活跃句柄稠密列表或基准补 50% 空洞率用例。
- **L9** findByName 每次构造临时 std::string（`ItemTemplateRegistry.cpp:70-77`）→ 透明哈希 string_view 查找。
- **L10** saveGlobalAsync 无防重入守卫，与 saveCharacterAsync 的 `m_isSaving.exchange` 不对称（`SaveManager.cpp:542`）→ 补独立原子守卫。
- **L11** Q4 回收站 telemetry 零落地且无跟踪条目；`src/game/foundation/components/InventoryComponent.hpp:40-41` 残留指向已删除 SerializationSystem 的过时注释。
- **L12** splitStack 的 proto 完整拷贝含 sockets[6]，可堆叠+有孔物品拆分后双实例共享子句柄，无双引用防御（`ItemStorageService.cpp:339-350`；此前评审遗留的「create 返回值未检查」已确认不适用，ItemStore::create 为 noexcept）→ 拆分入口断言 socketCount==0 或清空 proto.sockets。

### Best Practice

- **BP1** reinterpret_cast 低层边界（POD↔字节流，`ItemPersistenceCodec.cpp:21,415-421,438,460,480,749,850`）均合规但缺 §6.1 要求的理由注释，建议逐处补注固化审计结论。
- **BP2** ItemInstance 缺 `static_assert(sizeof(ItemInstance)==N)` 布局断言（`ItemStorageTypes.hpp:156-157`），整块序列化契约仅靠人工纪律。
- **BP3** `ItemStore::mutate` noexcept 内联执行调用方 lambda，lambda 抛出即 terminate（`ItemStore.hpp:56-63`），建议注释声明「fn 不得抛出」契约。
- **BP4** ItemMigration 失败分支缺定位信息（哪个容器/第几个句柄失效），漏 end 无机制可检测（`ItemMigration.cpp:52-148`）→ service 记录活动 migrationId，失败 LOG 带首个失效句柄。

## 7. 核查通过清单（重要承诺逐项属实）

- 句柄池代际防护：destroy 后残留句柄经 get/mutate/再 destroy 全部被拦截，无 UAF 路径
- 异步保存线程安全：主线程值语义快照 + Taskflow 全值捕获 + `Game.cpp:548` `wait_for_all()` + 成员声明顺序保证析构时序
- decode 事务性结构：CRC 全量通过才 `clearAll` 灌入，旧 store 经移动赋值整体替换，无跨档污染
- §5.2/§6.1/§8.1 合规：无裸 new/delete、无 dynamic_cast、无裸 std::thread
- CRC32 per-section + 损坏注入 6 种 SUBCASE（截断/magic/版本/头 CRC/载荷/尾部）全部断言 decode==false
- 主档→.bak→按无存档的回退链路闭环（`SaveManager.cpp:375-422`，版本篡改回退有测试）
- F5/F8 重定向、useItemStore 0 残留、SerializationSystem 删除无引用、三单例 Get() 清零、PortalSystem 经 `ctx->requestSave` 解耦
- 事务 API 七项存在且有测试；迁移令牌类型与校验逻辑在册
- 旧 JSON 档拒载不崩溃不部分加载，决策有文档与负向测试（`ItemSaveCompatRemovalTests.cpp` 4 用例）

## 8. 最佳实践建议（修复方案）

按优先级分四批，前两批为解锁「提交」结论的必改项：

**第一批（数据正确性，对应 B1/H3/H4）**
1. 角色档 encode 的 dirtyMask 排除 SharedStash section；global 快照仅含 SharedStash 引用子图（干净池 create），角色/全局写入路径彻底分离；
2. loadGlobal 双损路径显式失败（三态：成功/首次初始化/损坏），损坏时 LOG_ERROR 并向 UI 暴露提示通道；
3. 补一条端到端回归：跨「存角色档→共享仓库变更→退出→重启」序列断言物品不丢不复制。

**第二批（崩溃面与防御深度，对应 H1/H2）**
4. decode 分配上界三连检（length≤剩余字节、activeCount≤段余量/entrySize、槽位计数≤容量上限）+ `loadCharacter` 外层异常兜底走 .bak 回退；
5. 灌入前句柄校验 pass（isValid/去重/sockets 存在性）+ gen 延续策略 + activeCount 过滤后累计。

**第三批（测试防护与合同收口，对应 H5/M1/M7/M8）**
6. 补 MaterialBank codec round-trip、套装 setHash 重算、Inventory 稀疏布局三组断言；
7. SlotFilterPredicate 做实现或裁剪决策并同步设计文档；
8. ItemStore/SaveManager 基准预算改 CHECK 硬门禁。

**第四批（工程质量，对应 M2-M6/M9、L1-L12、BP1-BP4）**
9. 净帧缓存复用改造、sortTab 去字符串比较、异步保存编码链去多重拷贝、SaveFileAtomic fsync+降级修正、ItemMigration 接线或显式裁剪、其余卫生项按清单处理。

## 9. 剩余风险（修改通过后仍需显式接受）

- D8 决策：旧 v3/v4 JSON 存档不再迁移，老玩家进度按新档开始（有记录、有测试锁定，属产品决策）；
- D4 迁移令牌未接线：在掉落系统产生地面句柄前无实害，接入时必须先完成接线；
- 存储轨（ItemStorageAdapter）运行时零调用方：性能预算未经真实负载检验，L7/L8 的预算声明在小池热缓存基准下成立；
- TemplateFingerprint 强度不足：模板数值改动对旧档不可检测（L3），在补强前接受。

## 10. 下一步动作

1. 按第一批/第二批修复 B1、H1-H4（对应数据正确性与崩溃面）；
2. 补第三批测试防护后复跑 `ctest -R "nmd.tests.item"` 与 `-L performance`；
3. 完成一、二批后进行跟进审查（本轮次文件内追加轮次小节），复核通过后重新给出结论。

---

# 第 2 轮跟进审查（修复验证）

- 日期：2026-08-28
- 结论：**修改**（首轮结论「修改」的第 1–4 批修复整体质量高，核心 Blocker/High 全部有效解决；因新增发现 N1 打回，范围仅限测试卫生一项）
- 复查范围：工作区未提交修改（18 文件 +607/-131，`git status --short`），对应首轮报告 §8 修复批次第 1–4 批
- 复查方法：diff 全文逐行审阅 + 新增/修改代码源码级复核（不经测试输出推断正确性）+ 上轮 31 项发现项逐项核对 + 相邻模块回归测试

## 11. 本轮变更内容

工作区修改为对首轮发现项的集中修复，按批次与发现项对应关系：

- **数据正确性（B1/H3/H4）**：`SaveManager.cpp` 角色档 `dirtyMask` 排除 SharedStash 槽位 section；`saveGlobalAsync` 改为经 `restoreSharedStashFrom` 克隆共享仓子图到干净快照（替代全池拷贝）；`loadGlobal` 双损路径改显式 `return false` + LOG_ERROR，无文件路径仍初始化空仓。
- **崩溃面（H1/H2）**：`ItemPersistenceCodec.cpp` decode 增加 header/section length ≤ 流剩余字节、activeCount ≤ 段余量、convCount ≤ 100、dmgCount ≤ 100、nameLen ≤ 256、capacity ≤ 1000 等分配上界校验；`SaveManager.cpp` `loadCharacter` 外层 try-catch 走失败回退。灌入前 `sanitizeHandle` 校验 pass（isValid/去重/sockets 存在性）、gen ≥ 1 延续、activeCount 去重累计。
- **测试防护（H5/M1/M7/M8）**：MaterialBank codec round-trip 测试；Inventory 稀疏布局测试恢复；`SlotFilterPredicate` 实现 + `participatingIndices` 原位槽位保持 + HeirloomVault 排序支持；两个 Benchmark 全部预算项加 `CHECK` 硬门禁。
- **工程质量（M2/M4/M6/L1/L2/L4/L5/L9/L10/L12/BP1–BP3）**：`mergeStack` 全字段 + side table 比对（配套 `SkillDefs.hpp`/`Stats.hpp` 新增 `operator==`）；`StashSystem::sortTab` 改 baseId 排序；`.bak` 失败中止保存；autoDeposit 显式拒绝不支持容器；`restoreSharedStashFrom` 提取 + `CloneItemDeep` 递归克隆；Adapter fallback 修正 + `canStoreItem` null → false；`clearContainer(GroundPending)` 接 `clearGroundPending(true)`；透明哈希 `find(string_view)`；`saveGlobalAsync` 防重入守卫；splitStack 清空 proto sockets；§6.1 注释、`static_assert(sizeof(ItemInstance)==192)`、`mutate` noexcept 契约注释。

## 12. 上轮发现项处置（31 项逐项核对）

**已解决（23）**：B1、H1–H5、M1、M2、M4、M6、M8、L1–L6、L9、L10、L12、BP1–BP3。

- B1 复核：`charDirtyMask` 排除 SharedStash 槽位 section 后，角色档不再携带共享仓槽位引用，跨重启「存角色 → 变更共享仓 → 重启」序列不再被旧快照覆盖/回写；`loadCharacter` 末尾经 `restoreSharedStashFrom(*storage, preserved)` 保留共享仓内容并克隆重建（含嵌套 sockets 与 side table），旧的「槽位引用指向已替换池」悬垂问题一并消除。多角色隔离成立。
- H1/H2 复核：分配上界校验发生在 `resize`/`reserve` 之前；`loadCharacter` 异常不再外溢 terminate；`restoreRawEntries` index 去重、gen ≥ 1、sockets 句柄经 `sanitizeHandle` 验证后置 `{0,0}`，`m_activeCount` 按过滤后插入数累计，`ItemMigration` 守恒误报根因消除。
- H3 复核：主/备双损显式 `return false`，仅「文件不存在」走空仓初始化，语义与 `loadCharacter` 对齐。
- H4 复核：`saveGlobalAsync` 快照构造改为仅共享仓句柄克隆进干净池，global.nmd 不再含角色私有实例。
- M2 复核：`mergeStack` 比对 itemLevel/affixes/sockets/side table，不一致返回 `TypeMismatch` 走 swap；`DamageModifier`/`StatConversion` 的 `operator==` 为 default 且正确。
- M8 复核：swap/move/visit/freeze/create/destroy/createSnapshot/restore/encode/decode 十项预算全部 `CHECK` 化，阈值 3–4× 于 LOG 预算，机器抖动余量合理。

**部分解决（2）**：

- **M5**：`saveGlobalAsync` 快照驻留已从全池拷贝降为共享仓子图克隆，主要驻留面消除；但角色档路径 stringstream→`str()`→vector 三重拷贝与 encode 直写重载仍缺，增量 section 缓存仍无真实调用方（全部调用方 `dirtyMask=All`）。
- **M7**：Inventory 稀疏布局测试已恢复；套装 setHash 重算断言仍缺。

**维持开放（6，均为上轮建议级，本轮未采纳且无回归恶化）**：M3（UI 缓存命中每帧深拷贝）、M9（ItemMigration/GroundPending 死代码未接线）、L7（move<10ns 可疑）、L8（visit O(池容量)）、L11（telemetry/过时注释）、BP4（ItemMigration 失败缺定位）。

## 13. 本轮新发现

### N1（Medium，测试卫生——本轮打回原因）新增测试无条件删除/覆盖真实存档槽位

- 位置：`tests/unit/ItemSaveRoundTripTests.cpp` 新增用例（Multi-Character SharedStash Isolation / Corrupted Global Save）使用 `saves/slot_1.nmd`、`saves/slot_2.nmd`、`saves/global.nmd`，用例开头无条件 `std::remove` 或以损坏数据覆盖写入。
- 现象：开发者本机存在真实 slot_1/slot_2/global 存档时，跑一次单测即永久丢失（Corrupted 用例更是先把真实 global.nmd 覆盖为损坏数据）。上轮新增的 In-Flight Save Guard 用例已采用 `slot_98` 避让惯例，本轮新用例违反该惯例。
- 修复建议：改用专用测试槽位（如 `slot_97/98` 与独立 global 测试文件名），或用例内临时目录隔离；将「测试不得触碰常用存档槽位」写入测试规范。

### N2（Medium，解耦残留）角色档 ItemInstances/SideTables 段仍序列化 SharedStash 实例

- 位置：`src/game/systems/item/storage/ItemPersistenceCodec.cpp:71-84`（`buildItemInstancesSection` 经 `service.getStore()` 全池 visit）。
- 现象：`dirtyMask` 仅排除 SharedStash **槽位** section，实例数据段仍含共享仓物品字节（side tables 同理）。后果：角色档体膨胀、decode 后产生无槽位引用的孤儿池位，与 B1「写入路径彻底分离」的表述不完全一致。属既有行为非本轮引入，无正确性影响（槽位不可达即不参与 sanitize/迁移守恒外的逻辑）。
- 修复建议：encode 按槽位可达性过滤 ItemInstances/SideTables，或 decode 灌入后清理不可达实例；亦可显式记录接受（数据冗余换实现简单）。

### N3（Low）L3 指纹算法变更使全部存量档拒载，未递增存储版本

- 位置：`src/game/systems/item/storage/ItemPersistenceCodec.cpp:553-564`（mismatch → `return false`）、`SaveData.hpp`（`CURRENT_CHARACTER_SAVE_VERSION` 未动）。
- 现象：指纹混入模板数值哈希后，所有既有存档 fingerprint 不匹配即拒载。与项目「模板变化 = 拒载」的既有哲学一致且开发期无玩家存量，可接受；建议在变更记录中明示「本次修改后旧测试档/开发档全部失效」。

### N4（Low）`CloneItemDeep` 克隆失败静默丢物品

- 位置：`src/game/application/persistence/SaveManager.cpp`（`restoreSharedStashFrom` 内 `if (newH)` 跳过、恒 `return true`）。
- 现象：共享仓恢复时若干净池 `create` 失败（池满），该物品静默丢失且函数仍返回 true，上层无失败信号。池满在当前容量参数下极端边缘，但与 H3 修复确立的「失败显式化」原则不一致。
- 修复建议：失败计数/LOG_ERROR，考虑返回 bool 供上层区分「完整恢复/部分恢复」。

## 14. 验证证据

- `build.bat`（RelWithDebInfo）全量构建通过（含 render ABI 治理检查、资产校验）；
- `ctest -R "nmd.tests.item.unit|nmd.tests.performance"`：2/2 通过（item 0.76s 含新 H5/M7/M1 用例；performance 14.32s 含新 M8 CHECK 门禁）；
- `ctest -R "nmd.tests.combat.unit|nmd.tests.skill.unit|nmd.tests.ui.unit"`：3/3 通过（M2 新增 `operator==` 与 M4 排序键变更无相邻回归）；
- 主审源码复核：B1 序列推演、H1/H2 校验位置、`buildItemInstancesSection` 全池遍历、`clearContainer` 不销毁池实例（`ItemStorageService.cpp:777`）均逐行确认。

## 15. 本轮结论与下一步（第 2 轮）

**结论：修改。**

- 必改（解除打回）：N1（测试污染真实存档，改动成本极低）；
- 建议随下一批处理：N2（与 M5 同属 encode 路径改造，可合并实施）、N4；
- 显式接受/记录：N3（在变更记录中注明存量档失效）；
- 维持开放并排期：M3、M5、M7（setHash 部分）、M9、L7、L8、L11、BP4——均不阻塞存储轨当前阶段，但 M9（迁移令牌接线）须在掉落系统接入前完成。

N1 修复后本报告可更新为「提交」，无需再起新轮全面复审（复核范围限 `tests/unit/ItemSaveRoundTripTests.cpp` 槽位改动 + 复跑 item 单测）。

## 16. 第 3 轮复核（N1/N2/N4/M7 修复验证）

- 日期：2026-08-28
- 复核范围：相对第 2 轮审查时的增量修改（`ItemSaveRoundTripTests.cpp` 测试隔离重写、`ItemPersistenceCodec.cpp` N2 可达集过滤、`SaveManager.{hpp,cpp}` N4 显式化 + `SetSaveDirectory` 路径隔离、`ItemPersistenceCodecTests.cpp` H5 用例适配），增量约 +180/-30 行；逐行审阅增量 diff 并与第 2 轮已审内容比对确认无范围外变更。

**已解决（主审逐项复核确认）**：

- **N1（已解决）**：`SaveManager` 新增 `SetSaveDirectory`（`SaveManager.hpp:64`，默认 `"saves"` 保持生产行为），全部存档路径经 `getSavePath/getTempPath/getGlobal*Path` 解析，`SaveManager.cpp` 内 `"saves/"` 硬编码清零（仅余注释）。所有单测迁移至 `build/test_saves_isolation/` 临时目录 + 槽位 96/97/98/99，每用例 `create_directories` + 对称清理，绝不触碰开发机真实存档。
- **N2（已解决）**：`ItemPersistenceCodec.cpp` 新增 `collectReachableIndices`（按 dirtyMask 容器位遍历槽位、DFS 嵌套 sockets 收集可达 index），`buildItemInstancesSection`/`buildItemSideTablesSection` 参数化 `allowedIndices` 只序列化可达实例。角色档（`All & ~SharedStash`）实例段不再含共享仓物品，global 档（窄 mask）不再含角色私有物品，双源实例级解耦达成。
- **N4（已解决）**：`CloneItemDeep` 增加 `outSuccess` 传播 + `LOG_ERROR`，`restoreSharedStashFrom` 改返回 `allCloned` 真实结果。残留（低危）：`loadCharacter`/`loadGlobal` 调用处未消费返回值，部分失败仅 LOG 无上层信号，池满极端场景下物品静默缺失——接受，接线掉落系统时一并处理。
- **M7（完全解决）**：新增 `Set Name Hash Automatic Recalculation` 单测锁定 JSON 序列化排除 `setNameHash` + 反序列化重算行为。

**N2 引入的语义变化（主审确认自洽）**：`encode` 未脏且无缓存的段由「重建」改为「跳过」（`else if (isDirty)`）。两个真实调用方均满足新契约：`saveCharacterAsync` 传 `All & ~SharedStash`（覆盖含 ProgressionData/EconomyMetadata/MaterialBank 的全部必需段）；`saveGlobalAsync` 窄 mask（TemplateFingerprint|ItemInstances|ItemSideTables|SharedStash）依赖跳段实现 global.nmd 瘦身，`loadGlobal` 只读该四段。连带效应：无槽位引用的实例（如 GroundPending 持有句柄）不再进档，与 M9「迁移令牌未接线」合并跟踪。

- **N3（显式记录）**：L3 模板指纹强化（混入 type/slot/minLevel/maxStack）后存量档全部拒载，开发期接受，已在变更中注明。

**本轮新发现（均为低危观察项，不阻塞）**：

- **N5（Low，范围外观察）**：`tests/performance/ParticleTrailBenchmark.cpp:205` `CHECK(dispatchOverheadMs < 0.2)` 在本机两次实跑超标（0.357/0.243ms，值波动大）。该文件不在本轮变更集、上轮同测试通过，判定为既有 flaky 微基准（无预热/噪声隔离、阈值贴线）。建议另案放宽阈值或改多次取样取中位数。
- **N6（Low，隐含契约）**：`else if (isDirty)` 跳段语义要求调用方 dirtyMask 覆盖全部必需段；且 mask 缺 `TemplateFingerprint` 位时产物在非空注册表下 decode 必拒载。当前调用方均满足，建议在 `ItemPersistenceCodec.hpp:199` 的 `dirtyMask` 参数注释中写明该契约，防止未来新调用方误用。

## 17. 最终审查结论

**结论：提交**

- 自动化测试（主审本机实跑，RelWithDebInfo）：
  - `build.bat` 全量构建通过（Exit 0，含 render ABI 治理与资产校验）；
  - `nmd.tests.item.unit`：Passed（0.74s，含 N1/N2/B1/H3/M7 全链路新用例）；
  - `nmd.tests.ci.nonperf`：Passed（6.40s）；
  - `nmd.tests.performance`：**Failed**——唯一失败为 `ParticleTrailBenchmark.cpp:205`（见 N5，范围外既有 flaky，与本轮变更无关）；本轮修改直接相关的 ItemStore/SaveManager/Codec 全部基准与 `CHECK` 硬门禁（swap/move/visit/freeze/create/destroy/createSnapshot/restore/encode/decode）两轮实跑全部通过；
  - 第 2 轮已验证：`nmd.tests.combat/skill/ui.unit` 3/3 通过（operator== 与排序键变更无相邻回归）。
- 阻塞项 B1、H1–H5、M1、M2、M4、M6–M8、N1–N4 全部解决（N4 余留返回值未消费，已记录）。
- 剩余风险与维持开放项：M3、M5、M9（含 GroundPending 不进档观察）、L7（本轮实测 swap≈19ns、move≈17ns 超 LOG 预算但低于硬门禁，维持开放有据）、L8、L11、BP4、N5、N6，见第 13/16 节与 §9。


