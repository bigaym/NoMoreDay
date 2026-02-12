# Project Tracks

This file tracks all major active tracks for the project.
Completed tracks are archived in [./archive/tracks_archive.md](./archive/tracks_archive.md).

---

## [x] Track: Rendering Foundation Migration (rendering_foundation_migration_20260212)

> **目标**: 建立 RenderGraph 架构，拆解 RenderSystem 巨型函数，引入 FBO 池化与画质分级管理，为 HDR/后处理铺平道路。
> **文档**: [spec.md](./archive/rendering_foundation_migration_20260212/spec.md) | [plan.md](./archive/rendering_foundation_migration_20260212/plan.md)
> **状态**: COMPLETED (2026-02-12), 已归档

## [x] Track: Biome Generation System (biome_generation_system_20260208)

> **鐩爣**: 瀹炵幇瀹屾暣鐨勭敓鐗╃兢钀藉湴鍥剧敓鎴愮郴缁燂紝鏀寔 27 绉嶇兢钀介鏍?> **鏂囨。**: [spec.md](./tracks/biome_generation_system/spec.md) | [plan.md](./tracks/biome_generation_system/plan.md)
> **棰勮宸ユ椂**: 7-10 澶?
### Sub-Tracks

| Sub-Track | 名称 | 状态 | 进度 |
|-----------|------|------|------|
| **1.x** | 数据驱动层 (BiomeConfig, JSON, 枚举) | ✅ 已完成 | 100% |
| **2.x** | 渲染与物理增强 (空气墙, Shader, 物理) | ✅ 已完成 | 100% |
| **3.x** | 生成算法演进 (A/B/C组策略) | ✅ 已完成 | 100% |
| **4.x** | 动态交互逻辑 (可破坏, 刷怪墙/加速带) | ✅ 已完成 | 100% |
| **5.x** | 生态集成 (怪物池, 掉落表) | ✅ 已完成 | 100% |
| **6.x** | 测试与打磨 | ✅ 已完成 | 100% |

### 蹇€熼摼鎺?
- 璁捐鏂囨。: [鍦板浘鐢熺墿缇よ惤涓庡煄闀囩毊鑲よ璁?md](../璁捐鏂囨。/鍦板浘鐢熺墿缇よ惤涓庡煄闀囩毊鑲よ璁?md)
- 鐜版湁瀹炵幇: `src/game/systems/world/MapSystem.cpp`, `src/game/data/BiomeRegistry.hpp`

---

## [ ] Track: Next Milestone (TBD)

