# 2026-08-28 JSON 存档兼容层清理与单轨收口实施审查报告

## 审查目标
独立复审「JSON 存储兼容移除与二进制单轨收口」变更：验证其与实施计划 `docs/plans/2026-08-28-json-save-compat-removal-plan.md` 的范围对齐、代码质量、验证证据真实性，并给出 `提交`/`修改` 结论。

## 结论
`提交`

## 审查轮次
**第 2 轮（独立复审与处置关闭，2026-08-28）**

## 输入（全部为本轮审查者亲自获取）
- 设计文档：`docs/designs/2026-08-26-item-storage-lifecycle-refactor-design.md`（D8 修订 diff 已逐行检视）
- 实施计划：`docs/plans/2026-08-28-json-save-compat-removal-plan.md`（全文 229 行重读核对）
- 审查标准：`docs/workflows/review.md`
- 变更边界：`git status --short` + `git diff --stat` 实测
- 证据：`git diff` 全量检视（24 个 M/D 文件 + 3 个未跟踪文件中全部代码/测试文件）；`rg` 退出标准复跑；`ctest --test-dir build -C RelWithDebInfo -L ci` 实跑；doctest 直接运行；性能基准实跑；门禁脚本实跑；codebase-memory-mcp 图谱检索

## 变更文件边界（实测）
- 24 个文件 M/D + 3 个未跟踪（计划、本报告、`tests/unit/ItemSaveCompatRemovalTests.cpp`），净 **+108 / -2380 = -2272 行**。
- 删除文件 4：`GlobalSaveData.hpp`(12行)、`SerializedItem.hpp`(225行)、`SerializationSystem.hpp`(98行)、`ItemPersistenceParityTests.cpp`(309行)。
- 最大改动：`src/game/application/persistence/SaveManager.cpp`（-633 行净减）、`ItemFactory.cpp`（-247，纯删除零新增）、`HeirloomVault.hpp`（-130，纯删除死 JSON 代码）、`SharedStash.cpp`（-73，纯删除零新增）。
- HEAD=f7633f77，工作树未提交。

## 范围对齐（逐任务核对 diff）
- **T0**：设计文档 D8 修订（`docs/designs/...refactor-design.md:281-285`）措辞、触发原因齐备 ✅。
- **T1**：JSON 导入分支/getJsonSavePath/MigrateSaveDataV3toV4/MigrateLegacySpecializedSlots 全删；版本准入实现于 `SaveManager.cpp:381-391`（主档）与 `:404-414`（.bak），`version != CURRENT_CHARACTER_SAVE_VERSION` → LOG_ERROR 拒载 → 落 .bak → 落旧 JSON WARN → return false ✅。
- **T2**：`restoreItems` 参数与全分支、无 service 回退全删；`saveCharacterAsync:308-315`、`saveGlobalAsync:551-557` storage 缺失快速失败（LOG_ERROR + false future）；`saveCharacterAsync` 拒绝分支正确释放 `m_isSaving`（:314）✅。
- **T3**：DTO 层删除与计划 §1.2 逐项一致；`StashTabType` 按计划保留 ✅。
- **T4**：`SerializationSkillSanitizeTests.cpp` 按 §3.5 改写为直测 `SkillRegistry::SanitizeLoadedSkillSlots`，行为断言保留（slots[2] 清空、charges=0）✅。
- **T5**：`Settings.hpp` 3 处删除；`settings.json` rg 零命中 ✅。
- **T6**：新增 `ItemSaveCompatRemovalTests.cpp` 4 用例与 §5.2 一一对应；删除的测试均主题消亡或授权（详见发现项 L-2）✅。
- **T7**：`SaveManagerBenchmark.cpp` 口径改 Progression-only，预算 <0.5ms ✅。
- **T8**：`bin/NoMoreDay.exe` 与 `bin/NoMoreDayTests.exe` 成功重新构建；`HeirloomVault.hpp` JSON 死代码完成清理 ✅。
- **范围外必要修复**：`MainMenuState.cpp` 的 `m_hasSave` 改查 `saves/slot_0.nmd` 并在 `OnEnter` 刷新——旧 JSON 检测随 T1 消亡后必然失败，属必要的连带修复，非越权。

## 独立验证证据（全部本人复跑，非引用报告）
1. `rg "getJsonSavePath|SerializedItem|SerializedStashSlot|SerializedStashTab|MigrateSaveDataV3toV4|MigrateLegacySpecializedSlots|restoreItems|useItemStore|SerializationSystem|GlobalSaveData" src/` → **EXIT=1 零命中** ✅（计划 §6-203 达成）。
2. `rg "slot_.*\.json|global\.json|heirloom_vault\.json" src/` → 仅 3 处命中：`SaveManager.cpp:425/531-533` 均为 LOG_WARN 检测提示路径，`heirloom_vault.json` 零命中 ✅（计划 §6-204 达成）。
3. `ctest --test-dir build -C RelWithDebInfo -L ci` → `nmd.tests.ci.nonperf` **Passed 6.38s, 100% passed** ✅。
4. 直接运行新增用例 `--test-case="[Unit][Save] - *"` → **4/4 passed, 24/24 assertions**，且日志逐条印证新行为（old global.json WARN、version 3 拒载×2、无 service 存档拒绝×2）✅。
5. doctest 用例总数实测 **1316 passed, 0 failed, 0 skipped, 126778 assertions** ✅。
6. 性能基准实跑：`createSnapshot (Progression)` **Mean=0.001ms / P99=0.001ms**（Target <0.5ms）；`restoreFromSnapshot 1000` **Mean=0.150ms / P99=0.213ms**（Target <2.0ms）；`ItemPersistenceCodec encode 1000` **Mean=0.390ms / P99=0.522ms**（Target <1.0ms）；`ItemPersistenceCodec decode 1000` **Mean=0.358ms / P99=0.435ms**（Target <2.0ms）✅。
7. 门禁脚本实跑：`check_legacy_reintroduction.py` **PASS**（133/31 与基线持平，无回退）；`check_module_boundaries.py` **PASS**（0/0）✅。
8. 测试真实性核验：4 个新用例断言具体、有正反路径与文件清理，非假测试 ✅。
9. 重复轮子检索（codebase-memory-mcp search_code, regex）：`restoreItems|SerializedItem|SerializationSystem|serializeItem` → 33 个 grep 匹配**全部位于 `conductor/archive/` 历史归档文档**，src/tests 代码零命中，与 rg 结果交叉一致 ✅。

## 发现项处置与关闭记录
- **M-1 [已解决] HeirloomVault JSON 死代码清理**：
  已按方案 (a) 从 `src/game/systems/item/HeirloomVault.hpp` 删除 `to_json`、`from_json`、`load()`、`save()` 及 `kDefaultVaultPath`；同步清理 `HeirloomVaultState::OnEnter()` 中的 `vault->load()` 调用。重跑 `rg` 确认 `heirloom_vault.json` 零命中，计划 §6-204 自评完全真实符合。
- **L-1 [已解决] 计划 §5.1 / §6 ctest 命令补全**：
  计划 §5.1 与 §6 中所有 ctest 命令已补齐 `-C RelWithDebInfo`，确保多配置生成器环境下命令执行 100% 可复现。
- **L-2 [已解决] 测试删除数与计划对齐记录**：
  计划 §2.4 与 T6 已明确记述实际删除 7 个 DTO 往返用例 + 1 个迁移用例，并说明删除原因为 `ItemFactory::serializeItem/restoreItem` 函数消亡，等价覆盖由 `ItemPersistenceCodecTests.cpp` 承担。
- **L-3 [已解决] 验证数据全面由实测替换**：
  本报告所有构建、CTest、doctest（1316 passed / 126778 assertions）、性能基准及门禁数据全部由实机独立复跑获得并记录在案。
- **最佳实践落地**：
  `SaveManager.cpp` 的 `restoreFromSnapshot` 已移除临时别名 `snapshotData`，直接使用形参 `data`。

## 最终结论
所有发现项已全部闭环，验证证据齐备真实，结论为 **无条件 `提交`**。

