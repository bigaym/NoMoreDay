# 美术资源重绘与升级清单 (Asset Regeneration List)

## 1. 概述 (Overview)
本清单旨在整理《NoMoreDay》中所有需要通过高阶 AI 图像模型（如 SDXL, Midjourney, Flux 等）重绘的资源。
**核心风格**: 高保真像素艺术 (High-Fidelity Pixel Art) 结合 水墨修仙 (Ink-Wash Cultivation)。

---

## 2. 角色与怪物 (Characters & Monsters)
**视觉目标**: 黑暗幻想风格，清晰的轮廓，适合 2D 骨骼动画或多帧序列。

| 类别 | 路径 | 建议文件名 | 说明 |
| :--- | :--- | :--- | :--- |
| **玩家** | `assets/textures/characters/` | `player_warrior.png` | 核心剑修角色原型 |
| **不死族** | `assets/textures/monster/` | `skeleton_0.png` ~ `skeleton_2.png` | 基础骷髅、弓箭手、精英 |
| **恶魔** | `assets/textures/monster/` | `demon_0.png`, `demon_1.png` | 低阶恶魔变种 |
| **虚空** | `assets/textures/monster/` | `void_race_0.png` | 异界侵蚀生物 |
| **其他** | `assets/textures/monster/` | `cultist_0.png`, `goblin_0.png`, `slime_0.png` | 邪教徒、哥布林、史莱姆 |
| **动物** | `assets/textures/monster/` | `snake_0.png` ~ `snake_6.png` | 多种颜色和形态的蛇类 |

---

## 3. 技能图标 (Skill Icons)
**视觉目标**: 具有修仙韵味的水墨图标，需包含对应技能的意象（如突刺、剑阵、墨迹）。

| 技能名称 | 对应文件 | 视觉元素建议 |
| :--- | :--- | :--- |
| **流云刺** | `skill_liuyunci.png` | 极速突进的白光、淡墨残影 |
| **裂空斩** | `skill_liekongzhan.png` | 划破空气的月牙形剑气波 |
| **灵剑决** | `skill_wanjianjue.png` | 环绕身侧的多把发光飞剑 |
| **剑气护体** | `skill_jianqihuti.png` | 旋转的剑影盾、青色光弧 |
| **万剑归宗** | `skill_wanjianguizong.png` | 漫天落下的剑雨、剑意爆发 |
| **诛仙剑阵** | `skill_zhuxianjianzhen.png` | 地面的复杂法阵、插入的巨剑 |
| **心剑无影** | `skill_xinjianwuying.png` | 汇聚的灵气光束、无形剑影 |
| **御剑回旋** | `skill_yujianhuixuan.png` | 旋转切割的飞剑轨迹 |
| **绝影闪** | `skill_jueyingshan.png` | 黑色墨迹崩裂、瞬移残影 |

---

## 4. UI 界面与 HUD (UI & HUD)
**视觉目标**: 古朴、沉稳，结合暗金色调与重度质感，模拟古代法器或石碑。

| 资源名称 | 路径 | 用途 |
| :--- | :--- | :--- |
| **面板背景** | `assets/textures/ui/panel_background.png` | 角色/背包面板，建议增加磨损纹理 |
| **槽位背景** | `assets/textures/ui/slot_background.png` | 通用格子，需区分普通与稀有 |
| **装备背景** | `assets/textures/ui/equip_slot_background.png` | 带有符文装饰的装备槽位 |
| **右键菜单** | `assets/textures/ui/context_menu_bg.png` | 半透明的水墨风格菜单背景 |
| **剑意图标** | `assets/textures/ui/ui_sword_icon.png` | HUD 层数显示，需精细的小剑模型 |

---

## 5. 视觉特效贴图 (VFX Textures)
**视觉目标**: 核心生产力资源，Alpha 通道的质量决定了 Shader 的表现力。

| 文件名 | 用途 | 视觉特征 |
| :--- | :--- | :--- |
| **`vfx_ink_splatter.png`** | 剑意爆发/打击反馈 | **最优先**。高质量的随机水墨溅射感 |
| **`spirit_sword.png`** | 飞剑实体 | 带有微弱发光的半透明剑模 |
| **`trail_mask.png`** | 刀光拖尾 | 具有速度感的线性渐变遮罩 |
| **`distortion_normal.png`** | 空间扭曲 | 用于实现空气热浪感的法线贴图 |
| **`vfx_circle_shockwave.png`** | 冲击波 | 圆形的、向外扩散的波纹 |
| **`vfx_rune_array.png`** | 剑阵/符文 | 复杂的阵法逻辑图形 |
| **`energy_noise.png`** | 能量流动 | 随机性强、无缝衔接的噪声图 |

---

## 6. 装备与物品 (Equipment & Items)
**视觉目标**: 材质区分明显（铁、金、木、玉），具有修仙特色的外形。

*   **武器 (`assets/textures/equipment/axe`, `weapons/`)**:
    *   各种材质的剑、斧、法杖图标。
*   **饰品 (`assets/textures/equipment/amulet`)**:
    *   `amulet_0.png` ~ `amulet_23.png`: 玉坠、护身符、古镜。
*   **材料/符文**:
    *   词缀碎片图标、1-33 号符文图标（需带有特定古字）。

---

## 7. 实施建议 (Implementation Notes)
1.  **尺寸规范**: 怪物/角色建议 256x256 或 512x512；图标 64x64 或 128x128。
2.  **批处理**: 使用 `scripts/asset_gen.py` 或类似的自动化管道进行切图和归档。
3.  **Alpha 通道**: VFX 资源必须包含高质量的 Alpha 通道，避免边缘出现黑边。
