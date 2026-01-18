"""
NoMoreDay Armor/Jewelry Batch Generator
Purpose: Generates icons for 8 types of armor across 10 elemental themes.
         Wraps 'asset_gen.py' with specialized prompts for equipment.
Usage: python scripts/gen_armor_jewelry_batch.py
"""
import subprocess
import os

def generate_armor_assets():
    script_path = os.path.join(os.path.dirname(__file__), "asset_gen.py")
    output_width = 1024
    output_height = 1024

    # 1. 定义 8 种核心防具/饰品类型 (基于设计文档)
    armor_types = [
        "helmet",       # Head
        "pauldrons",    # Shoulder
        "chest_armor",  # Chest
        "gauntlets",    # Hands
        "leggings",     # Legs
        "boots",        # Feet
        "amulet",       # Neck
        "ring"          # Ring
    ]

    # 2. 沿用 10 种风格/元素主题，保持游戏视觉一致性
    themes = [
        {"name": "iron",     "prompt": "heavy plate armor forged from Damascus steel with intricate engravings, polished silver sheen, battle-hardened and solid"},
        {"name": "fire",     "prompt": "crafted from obsidian and volcanic rock, glowing with internal lava flows and flickering orange embers"},
        {"name": "ice",      "prompt": "sculpted from glacial ice crystals, featuring translucent blue frost layers and sparkling snow rime"},
        {"name": "lightning","prompt": "forged from celestial conductors, crackling with electric blue arcs and high-voltage energy sparks"},
        {"name": "poison",   "prompt": "corroded metal scales dripping with toxic green venom, featuring serpent-eye motifs and acidic fumes"},
        {"name": "necrotic", "prompt": "made of bleached humanoid bones and ancient ribs, wreathed in ghastly soul-flames and shadowy aura"},
        {"name": "holy",     "prompt": "shining golden plates with platinum wings filigree, radiating a pure divine light and celestial purity"},
        {"name": "void",     "prompt": "an eldritch relic of dark matter, containing a deep purple cosmos with swirling stardust and nebulae"},
        {"name": "nature",   "prompt": "grown from ancient sentient oak and vines, adorned with glowing emerald leaves and magical forest moss"},
        {"name": "arcane",   "prompt": "imbued with mana crystals and floating runic inscriptions, shimmering with arcane geometry and violet energy"}
    ]

    # 针对防具的负面提示词优化：严禁出现人体部位
    negative_prompt = (
        "human body, person wearing armor, face, limbs, skin, mannequin, blurry, "
        "3d render, photo, realism, text, watermark, messy background, collage"
    )

    print(f"--- Starting Armor/Jewelry Batch Generation: {len(armor_types)} Types x {len(themes)} Themes ---")

    for a_type in armor_types:
        print(f"\n--- Processing Type: {a_type.upper()} ---")
        for theme in themes:
            asset_name = f"{a_type}_{theme['name']}"
            
            # 构建提示词: 强调单件物品和专业图标感
            full_prompt = (
                f"A professional 2D game icon of a single {a_type}, {theme['prompt']}. "
                f"Isolated on a clean white background, digital painting style, sharp details, "
                f"centered, orthographic view, no wearer, masterpiece."
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
        generate_armor_assets()
    except KeyboardInterrupt:
        print("\nGeneration cancelled by user.")
