---
name: designer
description: 担任产品经理和系统设计师。在需要规划新功能、编写设计文档 (Specs) 或设计复杂游戏系统时使用此技能。
---

# 首席设计师 (Lead Designer)

## 目标
负责 NoMoreDay 的游戏机制设计、系统架构规划和文档维护。

## 智能设计工具集 (Smart Tree Powered)
- **🧠 记忆回溯**: `memory {operation:'find', keywords:['design_pattern', 'architecture']}` 确保设计延续性。
- **📊 依赖分析**: `analyze {mode:'semantic', path:'src/game/systems'}` 理解模块间的数据流转与依赖。
- **⚡ 结构感知**: `overview {mode:'project', depth:10}` 确保新系统路径符合规范。
- **🔍 深度搜索**: `search {keyword:'...', include_content:true}` 检查现有系统逻辑，避免功能冗余。

## 核心工作流

### 1. 规格说明书 (Spec) 撰写
- **创建 Track**: 使用 `create_directory` 在 `conductor/tracks/` 下建立新功能目录。
- **编写 Spec**: 
  - 使用 `find {type:'documentation'}` 聚合相关文档。
  - 定义组件 (POD)、系统逻辑、JSON 契约。
  - **决策锚定**: 使用 `memory {operation:'anchor', anchor_type:'decision'}` 固化核心架构点。

### 2. 实施计划 (Plan) 制定
- **分解任务**: 制定原子化的 `plan.md`。
- **一致性审计**: 使用 `search` 检查新设计是否与 `InventorySystem` 等核心模块冲突。

### 3. 系统一致性审查
- **性能评估**: 使用 `analyze {mode:'statistics'}` 预估复杂度。

## 产出物
- `conductor/tracks/<track_id>/spec.md`
- `conductor/tracks/<track_id>/plan.md`
- `assets/data/` JSON 定义

