# 美术资源重绘与升级清单 (Asset Regeneration List)

## 1. 概述 (Overview)
本清单旨在整理《NoMoreDay》中所有需要通过高阶 AI 图像模型（如 SDXL, Midjourney, Flux 等）重绘的资源。
**核心风格**: 高保真像素艺术 (High-Fidelity Pixel Art) 结合 水墨修仙 (Ink-Wash Cultivation)。
**通用提示词 (Prompt Keywords)**: `pixel art`, `ink wash painting style`, `dark fantasy`, `ancient chinese aesthetics`, `mystical`, `high contrast`, `detailed texture`.

---

## 2. 角色与怪物 (Characters & Monsters)
**视觉目标**: 黑暗幻想风格，清晰的轮廓，适合 2D 骨骼动画或多帧序列。
**命名规范**: 基础变种 `_0` (Fodder), 进阶/远程 `_1` (Ranger/Tank), 精英/首领 `_2` (Elite/Boss)。

| 种族 (Race) | 对应文件 (Files) | 视觉描述与提示词参考 (Visual Description & Prompts) |
| :--- | :--- | :--- |
| **玩家 (Player)** | `characters/player_warrior.png` | **Cultivator Swordsman**: Traditional Hanfu robe (tattered), flying swords on back, determined expression. Ink wash style strokes. |
| **不死族 (Undead)** | `monster/skeleton_0.png` <br> `monster/skeleton_1.png` <br> `monster/skeleton_2.png` | **0 (Warrior)**: Bleached white bones, rusty iron sword, dark aura. <br> **1 (Archer)**: Skeleton with rotten wooden bow, ragged leather scraps. <br> **2 (General)**: Heavy dark iron armor, glowing blue eyes, holding a guan dao or heavy blade. |
| **恶魔 (Demon)** | `monster/demon_0.png` <br> `monster/demon_1.png` <br> `monster/demon_2.png` | **0 (Imp)**: Small, red skin, jagged horns, mischievous. <br> **1 (Soldier)**: Muscular, holding a pitchfork or flaming whip, charred skin. <br> **2 (Lord)**: Large wings, burning crown, ornate obsidian armor. |
| **堕落者 (Corrupted)** | `monster/warcraft_0.png` <br> `monster/warcraft_1.png` | **0 (Abomination)**: Twisted flesh mixed with void purple energy, multiple eyes. <br> **1 (Horror)**: Tentacles, shifting form, ink-blot monster. |
| **邪教徒 (Cultist)** | `monster/cultist_0.png` <br> `monster/cultist_1.png` | **0 (Acolyte)**: Hooded robe, face hidden in shadow, holding a dagger. <br> **1 (Sorcerer)**: Ornate robes with strange runes, levitating a dark orb or talisman. |
| **精灵 (Elves)** | `monster/elf_0.png` <br> `monster/elf_1.png` | **0 (Fallen Ranger)**: Pale skin, dark markings on face, broken bow, corrupted nature magic. <br> **1 (Blade Dancer)**: Dual wielding daggers, flowing dark green silk, agile pose. |
| **兽人 (Beast)** | `monster/beast_0.png` <br> `monster/beast_1.png` | **0 (Brute)**: Boar or Tiger head humanoid, savage, muscular, holding a crude axe. <br> **1 (Shaman)**: Wearing bone necklace, holding a totem staff, tribal tattoos. |
| **哥布林 (Goblin)** | `monster/goblin_0.png` <br> `monster/goblin_1.png` | **0 (Scavenger)**: Small, green/grey skin, carrying a large sack and a small knife. <br> **1 (Bomber)**: Carrying explosives or alchemical flasks, goggles. |
| **龙裔 (Dragonkin)** | `monster/dragon_0.png` <br> `monster/dragon_1.png` | **0 (Newborn)**: Lizard-man, scales, spear. <br> **1 (Ascendant)**: Half-dragon, wings, breathing fire/smoke, heavy scale armor. |
| **机械 (Machine)** | `monster/mech_0.png` <br> `monster/mech_1.png` | **0 (Sentry)**: Ancient bronze automaton, gear-driven, glowing core. Woodpunk/Clockpunk style. <br> **1 (Construct)**: Large stone and bronze golem, rune-powered joints. |
| **元素 (Elemental)** | `monster/elemental_0.png` <br> `monster/elemental_1.png` | **0 (Wisp)**: Ball of pure energy (Fire/Ice/Lightning), swirling. <br> **1 (Guardian)**: Humanoid shape formed of rocks and magma/ice, core exposed. |
| **史莱姆 (Slime)** | `monster/slime_0.png` <br> `monster/slime_1.png` | **0 (Drop)**: Simple blob, translucent, internal core visible. <br> **1 (Cube)**: Larger, engulfing bones or weapons inside. |
| **野兽 (Animal)** | `monster/animal_0.png` <br> `monster/animal_1.png` | **0 (Wolf/Snake)**: Corrupted wildlife, red eyes, leaking dark aura. <br> **1 (Alpha)**: Larger, spikes protruding from back, ink trail. |

---

## 3. 技能图标 (Skill Icons)--已完成
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
**视觉目标**: 古朴、沉稳，结合暗金色调与重度质感，模拟古代法器、玉石或风化石碑。强调“金石之声”的视觉重量感。

### 基础控件 (Basic Controls)
| 资源名称 | 路径 | 文件名 | 提示词参考 (Prompt Idea) |
| :--- | :--- | :--- | :--- |
| **通用按钮** | `assets/textures/ui/` | `button_standard.png` | Stone tablet button, gold rim, ancient runes engraved, 3 states (normal, hover, pressed). |
| **确认/重要按钮**| `assets/textures/ui/` | `button_primary.png` | Jade or dark iron button, glowing magical energy, ornate design. |
| **复选框** | `assets/textures/ui/` | `checkbox.png` | Small stone square, carved, ink stroke checkmark when active. |
| **滑动条** | `assets/textures/ui/` | `slider_bar.png`, `slider_handle.png` | Bronze rail, jade token handle. |

### 容器与边框 (Containers & Borders)
| 资源名称 | 路径 | 文件名 | 提示词参考 (Prompt Idea) |
| :--- | :--- | :--- | :--- |
| **面板背景** | `assets/textures/ui/` | `panel_background.png` | Old dark paper texture, ink stains, subtle dragon pattern watermark. |
| **通用边框** | `assets/textures/ui/` | `border_frame_9slice.png` | 9-slice ready, dark wood with gold corners, ancient chinese window lattice style. |
| **工具提示背景** | `assets/textures/ui/` | `tooltip_bg.png` | Semi-transparent black ink wash, fading edges. |
| **分隔线** | `assets/textures/ui/` | `divider_line.png` | Ink brush stroke line, decorative knot in center. |

### HUD 元素 (HUD Elements)
| 资源名称 | 路径 | 文件名 | 提示词参考 (Prompt Idea) |
| :--- | :--- | :--- | :--- |
| **生命球/条框** | `assets/textures/ui/` | `hud_health_frame.png` | Red dragon carving, metallic texture, intricate details. |
| **生命填充** | `assets/textures/ui/` | `hud_health_fill.png` | Liquid blood/red ink texture. |
| **剑意/蓝条框** | `assets/textures/ui/` | `hud_mana_frame.png` | Blue phoenix or sword motif, metallic silver. |
| **剑意填充** | `assets/textures/ui/` | `hud_mana_fill.png` | Glowing cyan spirit energy, flowing liquid. |
| **经验条** | `assets/textures/ui/` | `hud_exp_bar.png` | Thin gold bar, filling with golden light. |
| **小地图边框** | `assets/textures/ui/` | `minimap_border.png` | Compass aesthetic, Bagua (Eight Trigrams) symbols, circular bronze frame. |

### 物品与交互 (Items & Interaction)
| 资源名称 | 路径 | 文件名 | 提示词参考 (Prompt Idea) |
| :--- | :--- | :--- | :--- |
| **槽位背景** | `assets/textures/ui/` | `slot_background.png` | Empty stone recess, simple square. |
| **稀有槽位** | `assets/textures/ui/` | `slot_rare_background.png` | Stone recess with faint golden glow border. |
| **装备背景** | `assets/textures/ui/` | `equip_slot_background.png` | Silhouette of weapon/armor type, dark ink style background. |
| **右键菜单** | `assets/textures/ui/` | `context_menu_bg.png` | Vertical scroll or hanging fabric style, dark. |
| **剑意层数图标**| `assets/textures/ui/` | `ui_sword_icon.png` | Tiny, sharp flying sword icon, metallic. |

---

## 5. 地图与环境 (Map & Environment)
**视觉目标**: 俯视视角 (Top-down)，无缝平铺 (Seamless Tiling)，水墨风格地表，与暗黑氛围结合。

| 类别 | 路径 | 文件名 | 提示词参考 (Prompt Idea) |
| :--- | :--- | :--- | :--- |
| **基础地表** | `assets/textures/environment/` | `tile_ground_dark.png` | Dark soil, ink wash texture, seamless tiling. |
| **草地** | `assets/textures/environment/` | `tile_grass_ink.png` | Sparse grass, desaturated green/black, ink strokes style. |
| **石板路** | `assets/textures/environment/` | `tile_paving_stone.png` | Cracked stone slabs, ancient ruin floor, mossy. |
| **水体** | `assets/textures/environment/` | `tile_water_ink.png` | Dark pool, animated potential, reflection of moon (abstract). |
| **墙体** | `assets/textures/environment/` | `wall_stone_ancient.png` | High stone wall, eroded, dark bricks. |
| **装饰物-树** | `assets/textures/environment/` | `env_tree_dead.png` | Twisted dead tree, gnarled branches, ink silhouette style. |
| **装饰物-竹** | `assets/textures/environment/` | `env_bamboo.png` | Dense bamboo cluster, misty, cultivation atmosphere. |
| **装饰物-岩石** | `assets/textures/environment/` | `env_rock_cluster.png` | Sharp rocks, mountain peak style miniature. |
| **传送门** | `assets/textures/environment/` | `env_portal_gate.png` | Glowing magical gate, swirling energy, ancient archway. |

---

## 6. 视觉特效贴图 (VFX Textures)
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

## 7. 装备与物品 (Equipment & Items)
**视觉目标**: 材质区分明显（铁、金、木、玉），具有修仙特色的外形。

*   **武器 (`assets/textures/equipment/axe`, `weapons/`)**:
    *   各种材质的剑、斧、法杖图标。--已完成
*   **盔甲 (`assets/textures/equipment/armor`)**:--已完成
*   **饰品 (`assets/textures/equipment/amulet`)**:--已完成
    *   `amulet_0.png` ~ `amulet_23.png`: 玉坠、护身符、古镜。
*   **材料/符文**:--已完成
    *   词缀碎片图标、1-33 号符文图标（需带有特定古字）。

---

## 8. 实施建议 (Implementation Notes)
1.  **尺寸规范**:
    *   怪物建议 256x256 或 512x512
    *   UI 按钮/图标: 64x64, 128x128
    *   面板背景: 512x512, 1024x1024 (或九宫格切片)
    *   地块贴图: 128x128 (Seamless)
    *   大型环境装饰: 256x256 ~ 512x512
2.  **批处理**: 使用 `scripts/asset_gen.py` 或类似的自动化管道进行切图和归档。
3.  **Alpha 通道**: VFX 资源必须包含高质量的 Alpha 通道，避免边缘出现黑边。
4.  **风格统一**: 保持“黑、白、金、青”的色板一致性，避免过于鲜艳的卡通色。
