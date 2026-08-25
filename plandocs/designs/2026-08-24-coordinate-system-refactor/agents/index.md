# 子代理工作包索引（Coordinate System Refactor）

每个工作包是一个**专业子代理**的独立任务单，串行落地、每步独立验证。子代理之间共享同一工作树与设计契约，但其文件范围不重叠（Phase 1 除外，因为它是全局地基）。

| 顺序 | 子代理 | 职责 | 状态 |
| --- | --- | --- | --- |
| 1 | engine-coord | 引擎坐标单一来源（CoordSystem + MVP/FRAGCOORD） | 🚧 In Progress |
| 2 | ui-coord | UI 坐标收口到 UiViewport | ⏳ Queued |
| 3 | text-msdf | MSDF/UV/Y 翻转收口 | ⏳ Queued |
| 4 | qa-coord | round-trip + guard + 像素矩阵 | ⏳ Queued |
| 5 | reviewer | 审阅 & docs/reviews 报告 | ⏳ Queued |
