# Blade Ascendant Design-Config Alignment (2026-02-23)

## Purpose

This document re-aligns design sections `3.1..3.9` (`设计文档/职业设计草案_剑修.md`) with current config artifacts:

- `assets/data/skills.json`
- `assets/data/skill_contracts_compact.json`

Focus is structural/config alignment and explicit semantic gaps for follow-up implementation.

## Global Summary

- Node scale: skills `1..9` are all `25` nodes (within design target `24-26`).
- Contract trigger: each skill has exactly one `Trigger` role in config.
- Contract synergy/transmuter: each skill has `>=1` synergy and exactly `2` transmuters.
- Remaining major gap: many newly aligned nodes are still config-only (data contract exists, behavior implementation incomplete).

## Per-Skill Key Node Alignment

| Skill | Design Trigger | Config Trigger Node | Design Synergy | Config Synergy IDs | Config Transmuter IDs | Status |
|---|---|---|---|---|---|---|
| 1 流云刺 | 瞬狱影爆 (60%, CD3) | `[114]` | 影之突袭 | `[130]` | `[170,171]` | Structural aligned, semantic effect pending |
| 2 裂空斩 | 回响斩 (40%, CD2) | `[233]` | 灵剑追击 | `[230]` | `[270,250]` | Structural aligned, semantic effect pending |
| 3 灵剑决 | 巨剑裂空 (30%, CD4) | `[373]` | 剑阵共鸣 | `[330]` | `[370,371]` | Structural aligned, semantic effect pending |
| 4 剑气护体 | 瞬身反打 (CD2) | `[451]` | 设计文档标注缺失 | `[430]` | `[470,471]` | Structural aligned, semantic effect pending |
| 5 万剑归宗 | 天诛 (CD2) | `[533]` | 万剑归阵 | `[530]` | `[570,571]` | Structural aligned, semantic effect pending |
| 6 剑阵·诛仙 | 阵斩回响 (50%, CD3) | `[633]` | 流云穿阵 | `[630]` | `[670,671]` | Structural aligned, semantic effect pending |
| 7 心剑·无影 | 寂灭 (CD3) | `[713]` | 剑意化无 | `[730]` | `[770,750]` | Structural aligned, semantic effect pending |
| 8 御剑·回旋 | 巨剑共鸣 (CD3) | `[831]` | 拔血流云 | `[830]` | `[870,871]` | Structural aligned, semantic effect pending |
| 9 绝影绝剑 | 逆命反噬 (CD0.5) | `[951]` | 设计文档标注缺失 | `[930]` | `[970,950]` | Structural aligned, semantic effect pending |

## Contract Density Snapshot (Config Current)

| Skill | Nodes | Trigger | Synergy | Transmuter | Keystone |
|---|---:|---:|---:|---:|---:|
| 1 | 25 | 1 | 1 | 2 | 7 |
| 2 | 25 | 1 | 1 | 2 | 8 |
| 3 | 25 | 1 | 1 | 2 | 9 |
| 4 | 25 | 1 | 1 | 2 | 10 |
| 5 | 25 | 1 | 1 | 2 | 8 |
| 6 | 25 | 1 | 1 | 2 | 9 |
| 7 | 25 | 1 | 1 | 2 | 9 |
| 8 | 25 | 1 | 1 | 2 | 8 |
| 9 | 25 | 1 | 1 | 2 | 7 |

## Alignment Decision

- Keep current config IDs as compatibility baseline for runtime safety.
- Do not force aggressive ID reshuffle in this pass.
- Move semantic closure to a dedicated implementation track: `skill_node_effect_implementation_20260223`.

## Follow-up

- Track: `conductor/tracks/skill_node_effect_implementation_20260223/`
- Goal: implement behavior, trigger, synergy, transmuter, and visual/runtime effects for newly aligned nodes with performance-safe execution.

