"""
NoMoreDay Weapons & Misc Batch Generator
Purpose: Generates icons for 10 types of weapons/offhands across 10 elemental themes.
         Automates the creation of a diverse loot visual library.
Usage: python scripts/gen_weapons_misc_batch.py
"""
import subprocess
import os

def generate_misc_assets():
    script_path = os.path.join(os.path.dirname(__file__), "asset_gen.py")
    output_width = 768
    output_height = 768

    # 1. 定义 10 种核心武器/副手类型 (基于设计文档)
    weapon_types = [
        "sword", "axe", "hammer", "dagger",       # 1H Melee
        "greatsword", "staff",                    # 2H
        "wand", "shield", "orb", "grimoire"       # Magic/Offhand
    ]

    # 2. 定义 10 种风格/元素主题 (10 Types * 10 Themes = 100 Assets)
    themes = [
        {"name": "iron",     "prompt": "forged from cold-rolled Damascus steel with intricate geometric engravings, polished silver sheen, heavy and battle-worn"},
        {"name": "fire",     "prompt": "crafted from molten obsidian, glowing with flowing orange lava, pulsating burning embers and charred rock textures"},
        {"name": "ice",      "prompt": "made of translucent glacial crystal, encased in a frost rime with a cold blue mist and jagged frozen ice shards"},
        {"name": "lightning","prompt": "a storm-forged weapon crackling with vibrant blue lightning arcs and voltaic energy, surrounded by electric sparks"},
        {"name": "poison",   "prompt": "corroded dark metal dripping with neon green venom, featuring serpent scale textures and toxic fuming acid bubbles"},
        {"name": "necrotic", "prompt": "sculpted from bleached ancient bone and skull fragments, wreathed in spectral ghost-green flames and shadowy soul mist"},
        {"name": "holy",     "prompt": "divine platinum filigree with polished gold accents, adorned with celestial angel wings and radiating a warm white halo"},
        {"name": "void",     "prompt": "an eldritch artifact made of dark matter, containing a swirling purple nebula and sparkling cosmic stars from the abyss"},
        {"name": "nature",   "prompt": "entwined with ancient druidic oak wood, blooming emerald leaves, glowing moss, and enchanted vines of the forest"},
        {"name": "arcane",   "prompt": "floating mana crystals held by glowing magical runes, featuring arcane geometry and pink-to-cyan shimmering energy"}
    ]

    # 统一负面提示词
    negative_prompt = (
        "human hands, character holding item, human body, multiple objects, collage, "
        "blurry, photo, realism, text, watermark, messy background, cluttered, scabbard"
    )

    print(f"--- Starting Batch Generation: {len(weapon_types)} Types x {len(themes)} Themes = {len(weapon_types)*len(themes)} Assets ---")

    for w_type in weapon_types:
        print(f"\n--- Processing Type: {w_type.upper()} ---")
        for theme in themes:
            asset_name = f"{w_type}_{theme['name']}"
            
            # 构建提示词: 自然语言化的描述
            full_prompt = (
                f"A high-quality 2D game icon of a {w_type}, {theme['prompt']}. "
                f"Clean digital painting style, sharp edges, centered composition, "
                f"isolated on a plain white background, masterpiece, professional game art."
            )
            
            print(f"Generating: {asset_name}")
            
            cmd = [
                "python", script_path,
                "--prompt", full_prompt,
                "--negative", negative_prompt,
                "--name", asset_name,
                "--width", str(output_width),
                "--height", str(output_height)
            ]

            try:
                subprocess.run(cmd, check=True, capture_output=False)
            except subprocess.CalledProcessError as e:
                print(f"Error generating {asset_name}: {e}")

    print("\n--- All tasks completed! Check assets/textures/ ---")

if __name__ == "__main__":
    # 确保在脚本目录下运行
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    
    try:
        generate_misc_assets()
    except KeyboardInterrupt:
        print("\nGeneration cancelled by user.")
