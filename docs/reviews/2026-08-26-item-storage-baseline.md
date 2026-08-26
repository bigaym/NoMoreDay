# 物品系统与生命周期重构性能基线报告（P0a Baseline Report）

- **日期**：2026-08-26
- **阶段**：P0a 护栏（基准与观测）
- **关联设计**：`docs/designs/2026-08-26-item-storage-lifecycle-refactor-design.md`
- **关联实施计划**：`docs/plans/2026-08-26-item-storage-lifecycle-refactor-plan.md`
- **测试环境**：Windows x64 / MSVC 19.51 / RelWithDebInfo / doctest

---

## 1. 现状基准实测数据汇总

| 基准测试项 | 负载规模 | Mean | P99 | 目标/预算对照 | 状态 |
|---|---|---|---|---|---|
| `UiSnapshot - Baseline Build` | 40 背包 + 10 装备 + 432 仓库(3Tab) + 20 地面 | **0.022 ms** | **0.035 ms** | 现状全量构建基线（P5 目标净帧 0ms） | 已建立基线 |
| `SaveManager::createSnapshot` | 1000 件实体物品快照 | **0.122 ms** | **0.170 ms** | 现状快照创建 < 10.0ms | 达标 |
| `SaveManager::restoreFromSnapshot` | 1000 件实体物品反序列化恢复 | **0.414 ms** | **0.535 ms** | 现状快照恢复 < 15.0ms | 达标 |
| `ItemFactory::createWeapon` | 1000 件武器生成 | **1.295 ms** | **1.978 ms** | 批量生成 < 5.0ms | 达标 |
| `ItemFactory::createArmor` | 1000 件护甲生成 | **1.413 ms** | **1.798 ms** | 批量生成 < 5.0ms | 达标 |
| `ItemFactory::legendaryAffixStress` | 1000 件传奇物品 (6 词缀 roll) | **2.514 ms** | **2.777 ms** | 词缀滚动压力测试 | 达标 |
| `Stash::Sort` | 144 件物品仓库整理 | **21 µs** | - | 内存排序 | 达标 |
| `Stash::Search` | 432 件物品名称匹配搜索 | **26 µs** | - | 现有字符串匹配 | 达标 |
| `Stash::AutoDeposit` | 40 件物品自动存放 | **8 µs** | - | 转移存放 | 达标 |
| `ItemStore` | 骨架基准用例 | 0.001 ms | 0.001 ms | 待 P2 落地后填充 10k 实例实测 (<10ns/move) | 骨架就绪 |

---

## 2. Tracy 区段接入确认

已在以下关键路径完成 `ZoneScopedN` 埋点接入：
1. `GameUiSnapshotBuilder::Build` (`ZoneScopedN("GameUiSnapshotBuilder::Build")`)
2. `SaveManager::createSnapshot` (`ZoneScopedN("SaveManager::createSnapshot")`)
3. `SaveManager::restoreFromSnapshot` (`ZoneScopedN("SaveManager::restoreFromSnapshot")`)
4. `ItemFactory::serializeItem` (`ZoneScopedN("ItemFactory::serializeItem")`)
5. `ItemFactory::restoreItem` (`ZoneScopedN("ItemFactory::restoreItem")`)

---

## 3. 后续阶段性能验收对照目标（P2 ~ P5）

1. **P2 (ItemStore)**：
   - `ItemStore::move` < 10ns / 次；
   - 10k 实例 `visit` 零分配遍历 < 0.1ms；
2. **P4 (二进制 Codec)**：
   - 1000 件物品存档创建（memcpy 快照冻结）< 0.2ms；
   - 1000 件物品存档写盘/恢复 < 2.0ms；
3. **P5 (UI 快照增量)**：
   - 净帧（version 未变且 options 未变）`Build` 物品构建成本 = 0ms（跳过重建）。
