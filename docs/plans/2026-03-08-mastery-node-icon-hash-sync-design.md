# 专精节点图标哈希同步设计

> 日期：2026-03-08
> 状态：已确认
> 范围：`assets/data/mastery_skill_trees.json` 的专精节点 `icon_id` 与普通技能节点图标规则对齐

---

## 背景

普通技能 `assets/data/skills.json` 中的节点 `icon_id` 已使用 `skill_node_<node_id>` 的 FNV1a-32 哈希值，运行时直接将该值作为纹理资源 id 传给 `AssetLoadingSystem::GetTexture(...)`。

三条剑修专精树 `assets/data/mastery_skill_trees.json` 当前虽然已经为节点补齐了 `1000~1224` 的图标编号，但这些值仍是裸节点 id，不是资源哈希，和普通技能数据格式不一致，因此不能直接复用现有渲染路径。

## 目标

- 让专精树节点 `icon_id` 与普通技能节点保持同一规则。
- 复用现有 Python 同步脚本，不新增运行时转换逻辑。
- 保持 JSON 结构不变，只更新 `icon_id` 的写法。

## 方案

采用数据侧同步方案：继续以 `skill_node_<node_id>` 为资源 key，使用与 `src/core/utils/HashUtils.hpp` 一致的 FNV1a-32 算法生成哈希，并将结果写回 JSON。

脚本侧扩展 `scripts/sync_skill_node_icon_ids.py`，使其默认同时处理：

- `assets/data/skills.json`
- `assets/data/mastery_skill_trees.json`

脚本仍通过 `assets/textures/skill_nodes/skill_nodes_<node_id>.png` 发现节点图标，只要图标存在，就将对应节点的 `icon_id` 同步为哈希值。

## 不做的事

- 不修改运行时 UI 渲染逻辑。
- 不在加载阶段为专精树做裸 id 到哈希的临时转换。
- 不改动资源头生成规则。

## 验收

- `mastery_skill_trees.json` 中 `1000~1224` 节点的 `icon_id` 被同步为哈希值。
- 同步脚本默认运行时同时覆盖普通技能树和专精树。
- Python 测试覆盖普通技能树与专精树两种输入。
