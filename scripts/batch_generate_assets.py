import subprocess
import os
import time

"""
Batch Asset Generator for NoMoreDay
Based on Asset_Regeneration_List.md
Optimized for FLUX.1 / Qwen3-8B Text Encoder
Style: Dark Ink-Wash Fantasy (Bronze & Jade)
"""

# Common style suffix for Flux to ensure consistency. 
# FLUX prefers natural language and descriptive style definitions over comma-separated tags.
STYLE_SUFFIX = ". The art style is Dark Ink-Wash Fantasy, a blend of traditional Chinese ink painting and gritty dark fantasy. Key elements include ancient oxidized bronze textures, translucent jade materials, and a high-contrast palette of deep black ink against pale cyan spiritual energy. The image should look like a high-fidelity 2D game asset, 8k resolution, sharp focus, masterpiece quality."

# Asset Definitions
# Format: category, filename (no ext), prompt, target_w, target_h, remove_bg
ASSETS = [
    # --- 3. Core UI Framework ---
    {
        "category": "UI_Core",
        "name": "panel_bg_ink",
        "prompt": "A high-resolution texture of dark ancient rice paper or old silk. It features a subtle, faint dragon pattern watermark and ink wash stains. The surface has a matte finish with fiber details. Dark and elegant atmosphere.",
        "w": 1024, "h": 1024, "remove_bg": False
    },
    {
        "category": "UI_Core",
        "name": "frame_bronze",
        "prompt": "A square game UI frame made of ancient, oxidized bronze with intricate Chinese window lattice carvings. The corners are decorated with bronze beast heads and inlaid with glowing, translucent green jade stones. Realistic metal patina and rust details.",
        "w": 512, "h": 512, "remove_bg": True
    },
    {
        "category": "UI_Core",
        "name": "btn_stone_normal",
        "prompt": "A rectangular UI button made of polished dark stone or ebony wood. It is engraved with ancient golden Chinese runes. The texture is smooth and heavy.",
        "w": 256, "h": 96, "remove_bg": True
    },
    {
        "category": "UI_Core",
        "name": "btn_stone_hover",
        "prompt": "A rectangular UI button made of polished dark stone or ebony wood, glowing with inner golden light. Engraved runes are bright and active. High gloss finish.",
        "w": 256, "h": 96, "remove_bg": True
    },
    {
        "category": "UI_Core",
        "name": "btn_stone_pressed",
        "prompt": "A rectangular UI button made of dark stone, appearing slightly depressed. The engraved runes glow with a deep red cinnabar color.",
        "w": 256, "h": 96, "remove_bg": True
    },
    {
        "category": "UI_Core",
        "name": "scrollbar_track",
        "prompt": "A vertical scrollbar track made of a thin, oxidized bronze pipe or groove. It has subtle engraved patterns.",
        "w": 32, "h": 512, "remove_bg": True
    },
    {
        "category": "UI_Core",
        "name": "scrollbar_thumb",
        "prompt": "A scrollbar slider thumb shaped like a small jade ornament or sword pommel. Translucent green stone texture.",
        "w": 32, "h": 64, "remove_bg": True
    },
    {
        "category": "UI_Core",
        "name": "tab_token",
        "prompt": "A UI tab element shaped like an ancient jade token or bookmark. It has carved cloud patterns and a semi-transparent, warm jade texture.",
        "w": 160, "h": 64, "remove_bg": True
    },
    {
        "category": "UI_Core",
        "name": "cursor_brush",
        "prompt": "A mouse cursor icon shaped like a sharp calligraphy brush tip or a stylized sword point. It combines ink black handle with a glowing jade tip.",
        "w": 64, "h": 64, "remove_bg": True
    },
    {
        "category": "UI_Core",
        "name": "cursor_hand",
        "prompt": "A mouse cursor icon shaped like a skeletal or spiritual hand made of ink smoke. It is reaching out to interact.",
        "w": 64, "h": 64, "remove_bg": True
    },

    # --- 4. Gameplay Panels ---
    {
        "category": "UI_Panels",
        "name": "bg_astrolabe",
        "prompt": "A background texture representing a deep starry sky overlayed with an ancient Chinese astronomy map and human meridian lines. Faint, glowing star trails connect node points. Mystical and dark.",
        "w": 1024, "h": 1024, "remove_bg": False
    },
    {
        "category": "UI_Panels",
        "name": "node_star",
        "prompt": "A glowing star node icon for a skill tree, appearing as a floating piece of jade or a magical stone. It emits a soft inner light.",
        "w": 128, "h": 128, "remove_bg": True
    },
    {
        "category": "UI_Panels",
        "name": "node_hex",
        "prompt": "A hexagonal node icon made of polished bronze. It has a carved symbol in the center.",
        "w": 128, "h": 128, "remove_bg": True
    },
    {
        "category": "UI_Panels",
        "name": "link_energy",
        "prompt": "A horizontal beam of spiritual energy flow, looking like a brush stroke of light. Cyan color, glowing, seamless tiling horizontally.",
        "w": 128, "h": 32, "remove_bg": True
    },
    {
        "category": "UI_Panels",
        "name": "bg_ascension_altar",
        "prompt": "An illustration of a magnificent ancient stone altar floating in the void. Background features spectral avatars of a Sword Saint, Sky Sword, and Demon Blade. Atmospheric fog.",
        "w": 1200, "h": 900, "remove_bg": False
    },
    {
        "category": "UI_Panels",
        "name": "bg_mosaic_loom",
        "prompt": "Top-down view of a stone chessboard altar or weaving loom. It features a 3x3 grid engraved with ancient directional runes and mystical markings.",
        "w": 800, "h": 800, "remove_bg": True
    },
    {
        "category": "UI_Panels",
        "name": "mosaic_tile_void",
        "prompt": "A Tetris-shaped stone tablet piece representing 'Void' terrain. It looks like cracked obsidian rock with glowing violet fissures.",
        "w": 256, "h": 256, "remove_bg": True
    },
     {
        "category": "UI_Panels",
        "name": "mosaic_tile_lava",
        "prompt": "A Tetris-shaped stone tablet piece representing 'Lava' terrain. Dark rock with veins of glowing orange magma.",
        "w": 256, "h": 256, "remove_bg": True
    },
    {
        "category": "UI_Panels",
        "name": "card_affix_frame",
        "prompt": "A vertical card frame for UI, styled like a Taoist talisman or tarot card. Made of old paper with red ink seals and a bronze border.",
        "w": 160, "h": 240, "remove_bg": True
    },
    {
        "category": "UI_Panels",
        "name": "slot_icon_helmet",
        "prompt": "A minimalist ink wash painting icon of a samurai helmet. Bold, expressive black brush strokes on a white background.",
        "w": 128, "h": 128, "remove_bg": True
    },
    {
        "category": "UI_Panels",
        "name": "slot_icon_armor",
        "prompt": "A minimalist ink wash painting icon of chest armor. Bold, expressive black brush strokes on a white background.",
        "w": 128, "h": 128, "remove_bg": True
    },
    {
        "category": "UI_Panels",
        "name": "slot_icon_weapon",
        "prompt": "A minimalist ink wash painting icon of a sword. Bold, expressive black brush strokes on a white background.",
        "w": 128, "h": 128, "remove_bg": True
    },
    {
        "category": "UI_Panels",
        "name": "divider_brush",
        "prompt": "A horizontal divider line that looks like a single, powerful black ink brush stroke. Tapered ends, texture of dry bristles.",
        "w": 512, "h": 32, "remove_bg": True
    },

    # --- 5. HUD ---
    {
        "category": "UI_HUD",
        "name": "hud_life_vessel",
        "prompt": "A UI health orb shaped like an ancient bronze cauldron or ding. It is filled with vibrant red liquid (blood or cinnabar) and decorated with dragon carvings.",
        "w": 192, "h": 192, "remove_bg": True
    },
    {
        "category": "UI_HUD",
        "name": "hud_sword_gauge",
        "prompt": "A vertical resource gauge shaped like a stylized sword. The blade fills up with glowing cyan energy. Ancient weapon design with bronze hilt.",
        "w": 64, "h": 256, "remove_bg": True
    },
    {
        "category": "UI_HUD",
        "name": "hud_skill_rack",
        "prompt": "A horizontal UI bar for skill icons, styled like an ancient weapon rack or wooden tray. Dark wood texture with bronze fittings.",
        "w": 800, "h": 128, "remove_bg": True
    },
    {
        "category": "UI_HUD",
        "name": "frame_buff",
        "prompt": "A small square icon frame for positive status effects. Golden cloud patterns, glowing slightly.",
        "w": 64, "h": 64, "remove_bg": True
    },
    {
        "category": "UI_HUD",
        "name": "frame_debuff",
        "prompt": "A small square icon frame for negative status effects. Dark purple thorny vines or rusty iron chains.",
        "w": 64, "h": 64, "remove_bg": True
    },
    {
        "category": "UI_HUD",
        "name": "hud_minimap_compass",
        "prompt": "A circular Feng Shui compass (Luopan) for a minimap frame. Made of wood and bronze, featuring intricate Bagua symbols and directional markings. Top-down view.",
        "w": 256, "h": 256, "remove_bg": True
    },

    # --- 6. Environment ---
    {
        "category": "Environment",
        "name": "tile_void_ground",
        "prompt": "Top-down texture of a void stone floor. The surface is dark grey and black obsidian rock with glowing violet fissures and faint runic carvings. Seamless tiling pattern.",
        "w": 512, "h": 512, "remove_bg": False
    },
    {
        "category": "Environment",
        "name": "tile_ink_grass",
        "prompt": "Top-down texture of a grassy field painted in traditional Chinese ink wash style. Desaturated greens and greys with black ink strokes defining grass blades. Seamless tiling pattern.",
        "w": 512, "h": 512, "remove_bg": False
    },
    {
        "category": "Environment",
        "name": "tile_ink_water",
        "prompt": "Top-down texture of a deep, black ink pool. The surface is oily and reflective with white highlights, resembling liquid ink. Seamless tiling pattern.",
        "w": 512, "h": 512, "remove_bg": False
    },
    {
        "category": "Environment",
        "name": "wall_ancient_stone",
        "prompt": "Texture of an ancient ruined stone wall. Massive grey bricks covered in dark moss and faint glowing cracks. Weathered and eroded.",
        "w": 512, "h": 512, "remove_bg": False
    },
    {
        "category": "Environment",
        "name": "prop_dead_tree",
        "prompt": "A gnarled dead tree rendered in an ancient ink wash style. Twisted black branches, expressive brush strokes, minimal foliage.",
        "w": 256, "h": 256, "remove_bg": True
    },
    {
        "category": "Environment",
        "name": "prop_broken_sword",
        "prompt": "An ancient, rusty sword stuck in the ground at an angle. Covered in moss and cracks. Isometric view.",
        "w": 128, "h": 128, "remove_bg": True
    },
    {
        "category": "Environment",
        "name": "prop_lantern",
        "prompt": "A traditional stone lantern (toro) glowing with a blue spirit fire. Ancient and weathered stone texture.",
        "w": 128, "h": 192, "remove_bg": True
    },
    {
        "category": "Environment",
        "name": "env_portal_ink",
        "prompt": "A dimensional rift portal appearing as a tear in reality made of swirling black ink. It looks like a vortex of calligraphy strokes and dark energy.",
        "w": 256, "h": 512, "remove_bg": True
    },

    # --- 7. VFX ---
    {
        "category": "VFX",
        "name": "vfx_ink_splatter",
        "prompt": "A dynamic, explosive black ink splatter effect. Intricate droplets and streaks flying outward in a fluid motion. High contrast black on white.",
        "w": 512, "h": 512, "remove_bg": True
    },
    {
        "category": "VFX",
        "name": "vfx_aura_noise",
        "prompt": "An abstract texture of flowing spiritual energy, resembling wisps of smoke and clouds in cyan and white. Soft, ethereal, swirling noise pattern. Seamless.",
        "w": 512, "h": 512, "remove_bg": False
    },
    {
        "category": "VFX",
        "name": "vfx_rune_array",
        "prompt": "A complex, circular magic array glowing with golden light. Composed of ancient Chinese trigrams (Bagua) and sword motifs. Symmetrical geometric lines.",
        "w": 512, "h": 512, "remove_bg": True
    },

    # --- 8. Main Menu ---
    {
        "category": "Illustrations",
        "name": "bg_main_menu",
        "prompt": "A breathtaking cinematic landscape of a lonely mountain peak piercing through a sea of dark clouds. A colossal, ancient rusty sword is embedded into the summit. A shattered moon hangs in the dark night sky.",
        "w": 1280, "h": 720, "remove_bg": False
    },
    {
        "category": "Illustrations",
        "name": "ui_save_slot",
        "prompt": "A UI frame for a save slot, styled like a soul lamp or life tablet. It looks ancient and mystical. If active, it glows; if empty, it looks cold and dark.",
        "w": 400, "h": 500, "remove_bg": True
    },
]

def run_generation():
    print(f"Starting Batch Generation for {len(ASSETS)} assets...")
    print(f"Global Style: {STYLE_SUFFIX}")
    
    for i, asset in enumerate(ASSETS):
        print(f"\n[{i+1}/{len(ASSETS)}] Generatng: {asset['name']} ({asset['category']})")
        
        # Construct full sentence prompt
        full_prompt = asset['prompt'] + STYLE_SUFFIX
        output_dir = os.path.join("assets", "generated", asset['category'])
        
        cmd = [
            "python", "scripts/asset_gen.py",
            "--prompt", full_prompt,
            "--name", asset['name'],
            "--target-width", str(asset['w']),
            "--target-height", str(asset['h']),
            "--output-dir", output_dir
        ]
        
        if not asset['remove_bg']:
            cmd.append("--no-remove-bg")
            
        # Execute
        try:
            subprocess.run(cmd, check=True)
            # Small cooldown to be safe with ComfyUI queue
            time.sleep(1)
        except subprocess.CalledProcessError as e:
            print(f"!!! Error generating {asset['name']}: {e}")
            
    print("\nBatch Generation Complete!")

if __name__ == "__main__":
    run_generation()
