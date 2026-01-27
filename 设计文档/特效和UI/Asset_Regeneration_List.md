# 美术资源需求清单 (Asset Requirement List) - 2026.01.27 更新

## 1. 概述 (Overview)

本清单基于 **Phase 11 (职业深度)** 和 **Phase 12 (局外成长)** 的开发需求整理，旨在补全项目当前缺失的核心 UI 与环境资源。
**核心风格**: **水墨黑暗幻想 (Dark Ink-Wash Fantasy)** + **金石质感 (Bronze & Jade)**。
**技术标准**: **2K 分辨率基准 (2560x1440)**。项目 UI 逻辑以 2K 为 1:1 参考，低分辨率屏幕将自动向下缩放。

---

## 2. 技术规格 (Technical Specs)

为保证在 2K 屏幕下的细节锐度并兼容缩放，请遵循以下规格：
*   **参考分辨率**: 2560x1440 (UI_REF_WIDTH = 2560)。
*   **全屏资源**: 必须为 2560x1440。
*   **UI 控件**: 优先使用 **九宫格 (9-Slice)** 技术以节省显存。
*   **通道**: VFX 与透明 UI 必须包含高质量 **Alpha 通道**，避免黑边。
*   **导出格式**: 无损 PNG。

---

## 3. 系统 UI 框架 (Core UI Framework)
**优先级**: **P0** (影响全量交互感)

| 资源名称 | 文件名建议 | 推荐规格 (2K 基准) | 视觉描述 |
| :--- | :--- | :--- | :--- |
| **通用背景板** | `panel_bg_ink.png` | 1024x1024 (9-Slice) | 深色宣纸或旧丝绸质感，带有隐约的云纹或龙纹暗刻。 |
| **通用边框** | `frame_bronze.png` | 256x256 (9-Slice) | 中式窗棂风格，乌木材质，边角带有青铜兽首或铜扣装饰。 |
| **交互按钮** | `btn_stone_set.png` | 256x96 (Atlas) | 包含 Normal (石质/乌木), Hover (金光/墨韵), Pressed (朱砂印) 三态。 |
| **滚动条** | `scrollbar_set.png` | Atlas | 轨道为细长铜管；滑块为玉佩、剑穗或云纹造型。 |
| **页签** | `tab_token.png` | 160x64 | 类似于书签或令牌。选中时高亮玉质并凸起。 |
| **鼠标指针** | `cursor_set.png` | 64x64 (Atlas) | 毛笔尖、剑尖或玉石指针。包含：普通、攻击、拾取、禁止。 |

---

## 4. 核心功能面板 (Gameplay Panels)
**优先级**: **P0** (服务于 Phase 11 & 12)

### A. 技能与天赋 (Skill & Talent Tree)
| 资源名称 | 文件名建议 | 推荐规格 | 视觉描述 |
| :--- | :--- | :--- | :--- |
| **星图/经络背景** | `bg_astrolabe.png` | 2048x2048 (Tiling) | 深邃的星空或人体经络图底纹。带有微弱的星轨连线。 |
| **节点图标** | `nodes_set.png` | 128x128 (Atlas) | **小**: 圆形穴位(灰/银/玉)。**中**: 菱形/方形。**大**: 八卦/太极图标。 |
| **连线纹理** | `link_energy.png` | 128x32 (Tiling) | 灵气流动的线条。未解锁为暗线，解锁为发光的流光线。 |
| **专精祭坛** | `bg_ascension_altar.png` | 1200x900 | 宏大的祭坛插画。背景展示剑圣/天剑/魔剑的虚影。 |

### B. 维度拼接 (Dimensional Mosaic)
| 资源名称 | 文件名建议 | 推荐规格 | 视觉描述 |
| :--- | :--- | :--- | :--- |
| **拼接台底座** | `bg_mosaic_loom.png` | 800x800 | 一个 3x3 网格的石质棋盘或祭坛，刻有古老的方位符文。 |
| **地图碎片** | `mosaic_tiles_set.png` | 256x256 (Atlas) | Tetris 形状的石板或符咒。不同纹理代表地形(岩浆/草地/虚空)。 |
| **词缀卡牌** | `card_affix_frame.png` | 160x240 | 类似塔罗牌或符箓的边框，用于悬浮显示区域词缀。 |

### C. 角色与背包 (Character & Inventory)
| 资源名称 | 文件名建议 | 推荐规格 | 视觉描述 |
| :--- | :--- | :--- | :--- |
| **装备槽位底图** | `slot_icons_ink.png` | 128x128 (Atlas) | 头盔、护甲、鞋子、武器等部位的水墨轮廓底图。 |
| **属性装饰线** | `divider_brush.png` | 512x32 | 毛笔笔触风格的分隔线。 |

---

## 5. HUD (Heads-Up Display)
**优先级**: **P1** (战斗沉浸感)

| 资源名称 | 文件名建议 | 推荐规格 | 视觉描述 |
| :--- | :--- | :--- | :--- |
| **生命球/框** | `hud_life_vessel.png` | 192x192 | 丹田处的红色鼎炉，或缠绕的赤龙边框。内部为红色液体/墨水。 |
| **剑意量表** | `hud_sword_gauge.png` | 48x192 | 竖向或横向的剑形量表。随着充能剑身发光，满层燃烧。 |
| **技能栏** | `hud_skill_rack.png` | 800x128 | 类似兵器架的底部托盘，用于放置技能图标。 |
| **状态边框** | `buff_frames_set.png` | 64x64 (Atlas) | **Buff**: 金色/青色云纹框。**Debuff**: 紫色/黑色荆棘框。 |
| **小地图框** | `hud_minimap_compass.png` | 256x256 | 罗盘 (Feng Shui Compass) 样式，带有八卦方位刻度。 |

---

## 6. 场景与环境 (Environment Tilesets)
**优先级**: **P1** (核心功能修复)

| 资源名称 | 文件名建议 | 推荐规格 | 视觉描述 |
| :--- | :--- | :--- | :--- |
| **虚空地表** | `tile_void_ground.png` | 256x256 (Seamless) | 虚空中的浮空石板路，边缘有碎裂感，深色调。 |
| **水墨草地** | `tile_ink_grass.png` | 256x256 (Seamless) | 黑白灰阶的草地纹理，点缀少量青色/金色花草。 |
| **墨池水体** | `tile_ink_water.png` | 256x256 (Seamless) | 深黑色液体，表面有白色反光或流动纹理。 |
| **古老墙体** | `wall_ancient_stone.png` | 256x256 | 残破的石墙，长满苔藓或刻有微弱发光的符文。 |
| **装饰物集** | `env_props_set.png` | Atlas | 枯树 (Dead Tree)、断剑 (Broken Sword)、石灯笼 (Lantern)、残佛 (Statue)。 |
| **传送门** | `env_portal_ink.png` | 256x512 | 一道撕裂空间的水墨口子，或发光的阵法门。 |

---

## 7. 视觉特效 (VFX)
**优先级**: **P1** (打击感核心)

| 资源名称 | 文件名建议 | 推荐规格 | 视觉描述 |
| :--- | :--- | :--- | :--- |
| **水墨溅射** | `vfx_ink_splatter.png` | 512x512 | **核心**。高质量 Alpha 通道水墨溅射图，用于受击反馈。 |
| **剑意光环** | `vfx_aura_noise.png` | 512x512 | 云雾状的噪声纹理，用于实现流动的剑意护体效果。 |
| **剑阵符文** | `vfx_rune_array.png` | 1024x1024 | 复杂的阵法逻辑图形，用于剑阵/法阵技能地面显示。 |

---

## 8. 主界面 (Main Menu & Polish)
**优先级**: **P2** (Phase 14)

| 资源名称 | 文件名建议 | 推荐规格 | 视觉描述 |
| :--- | :--- | :--- | :--- |
| **主背景** | `bg_main_menu.png` | 2560x1440 | 孤峰之巅，云海翻腾，一把巨剑插在山顶，背景是巨大的碎裂明月。 |
| **Logo** | `logo_title.png` | 1200x600 | 书法字体的“无尽长夜”或“NoMoreDay”，带有金属锈蚀与墨迹。 |
| **存档位** | `ui_save_slot.png` | 400x500 | “魂灯”或“命牌”样式。硬核模式死亡后显示为碎裂状态。 |

---

## 9. Gemini 3 (Nano Banana) High-Res Prompts
以下提示词专为 **Gemini 3 / Imagen 3** 设计。
**注意**: 提示词中的 "8k resolution" 仅控制画面的**细节密度**。您在生成时**必须**在工具中手动指定输出比例 (Aspect Ratio) 或分辨率。

### A. UI Material & Controls (UI材质与控件)

**Panel Background (深色宣纸背景)**
> **Setting**: Aspect Ratio 1:1 (Square)
> **Prompt:** High-resolution texture of ancient dark rice paper, extremely detailed fibers, subtle ink wash stains, faint dragon pattern watermark, dark fantasy aesthetic, elegant and mysterious, matte finish, soft lighting, 8k resolution, photorealistic material, seamless texture.

**Bronze & Jade Frame (青铜镶玉边框)**
> **Setting**: Aspect Ratio 1:1 (Square)
> **Prompt:** A rectangular game UI frame made of ancient oxidized bronze with intricate cloud carvings. Inlaid with translucent glowing green jade stones at the corners. The bronze has realistic patina and rust details. The jade has subsurface scattering and inner glow. Isolated on a black background. Macro photography style, sharp focus, 8k resolution, high fidelity, concept art for a dark fantasy RPG.

**Jade Button (玉石交互按钮)**
> **Setting**: Aspect Ratio 1:1 (Square) or 3:2
> **Prompt:** A rectangular button UI element made of polished dark cyan jade. Engraved with ancient Chinese golden runes. The jade is semi-transparent with a soft inner light. High gloss finish, realistic stone texture, smooth edges. Isolated on black background. 8k resolution, game asset style.

### B. Environment Tilesets (环境地块)

**Void Stone Ground (虚空石板地表)**
> **Setting**: Aspect Ratio 1:1 (Square)
> **Prompt:** Top-down view of a cracked void stone floor tile for a video game. Dark grey and black obsidian rock with glowing violet fissures and faint runic carvings. Ancient weathering, realistic rock texture, sharp details. Seamless tiling pattern. Dark fantasy style, ominous atmosphere. High detail, 8k resolution, Unreal Engine 5 render style.

**Ink Grass (水墨风格草地)**
> **Setting**: Aspect Ratio 1:1 (Square)
> **Prompt:** Top-down view of a grassy field painted in a traditional Chinese ink wash style. Desaturated greens and greys, black ink strokes defining the grass blades. Artistic, expressive, yet detailed enough for a game texture. Seamless tiling pattern. High contrast, sharp details, 4k resolution.

**Ancient Wall (古老遗迹墙体)**
> **Setting**: Aspect Ratio 1:1 (Square)
> **Prompt:** Texture of an ancient ruined stone wall, massive grey bricks covered in dark moss and faint glowing cracks. Realistic stone surface details, weathering, erosion. Dark fantasy dungeon atmosphere. Seamless tiling texture. 8k resolution, photorealistic.

### C. VFX & Special Effects (视觉特效)

**Ink Splatter (水墨溅射)**
> **Setting**: Aspect Ratio 1:1 (Square)
> **Prompt:** A dynamic, explosive black ink splatter visual effect on a pure white background. High contrast, fluid motion, intricate droplets and streaks flying outward. Traditional Chinese calligraphy style mixed with dark fantasy aggression. The ink looks wet, heavy, and viscous. High resolution, 8k, macro photography, sharp focus, suitable for game VFX texture.

**Sword Intent Aura (剑意光环噪声)**
> **Setting**: Aspect Ratio 1:1 (Square)
> **Prompt:** Abstract texture of flowing spiritual energy, resembling wisps of smoke and clouds. Cyan and white color palette. Soft, ethereal, swirling patterns. Noise texture for a game shader. Seamless, high contrast, 4k resolution, magical atmosphere.

**Rune Array (剑阵法阵)**
> **Setting**: Aspect Ratio 1:1 (Square)
> **Prompt:** A complex, circular magic array glowing with golden light on a black background. Composed of ancient Chinese trigrams (Bagua) and sword motifs. Symmetrical, intricate geometric lines, magical energy. High contrast, neon glow, sharp vector-like lines. 8k resolution, game VFX asset.

### D. Main Illustrations (主插画)

**Main Menu Background (孤峰巨剑)**
> **Setting**: Aspect Ratio 16:9 (Landscape)
> **Prompt:** A breathtaking cinematic landscape of a lonely mountain peak piercing through a sea of dark clouds. A colossal, ancient rusty sword is embedded into the mountain summit. A shattered moon hangs in the dark night sky, casting cold pale light. Dark fantasy art style, ink wash painting influences, epic scale, mysterious and melancholic. 8k resolution, highly detailed, wallpaper quality, masterpiece.

**Ascension Altar (专精祭坛)**
> **Setting**: Aspect Ratio 4:3 or 16:9
> **Prompt:** An ancient stone altar floating in a void, surrounded by floating broken islands. Three spectral avatars stand behind the altar: a Sword Saint, a Sky Sword summoner, and a Demon Blade wielder. Mystical atmosphere, glowing runes, fog and clouds. Wide angle shot, cinematic composition, dark fantasy RPG concept art. 8k resolution, high fidelity.