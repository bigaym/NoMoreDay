# Implementation Plan: Legendary Affix System

- [x] 修改 `src/game/components/ItemStats.hpp`: 升级 `AffixType` 并实现 `Range Markers`。
- [x] 修改 `src/game/systems/item/ItemFactory.cpp`: 实现追加加载逻辑与语义化掉落过滤。
- [x] 实现 `scripts/gen_legendary_affixes.py`: 自动从设计文档解析并生成 50 个传奇词缀。
- [x] 编写并运行 `tests/TestLegendaryInfrastructure.cpp`: 验证“手动可赋予、随机不掉落”逻辑。
