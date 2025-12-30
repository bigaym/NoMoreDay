import subprocess
import os

def generate_armor_assets():
    script_path = os.path.join(os.path.dirname(__file__), "asset_gen.py")
    output_width = 1024
    output_height = 1024

    # 1. 定义 8 种核心防具/饰品类型 (基于设计文档)
    # 使用对 AI 绘图更友好的具体名词 (如 pauldrons 替代 shoulder)
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
        {"name": "iron",     "prompt": "Damascus steel, intricate engraving, polished silver, knightly, heavy metal"},
        {"name": "fire",     "prompt": "Molten obsidian, flowing lava, burning ember, charred rock, volcanic energy"},
        {"name": "ice",      "prompt": "Translucent glacial crystal, frost rime, frozen aura, winter chill, jagged ice shards"},
        {"name": "lightning","prompt": "Storm-forged metal, crackling blue lightning, voltaic arcs, thunder energy, electrified"},
        {"name": "poison",   "prompt": "Corroded metal, dripping venom, serpent scales, toxic green glow, acid burns"},
        {"name": "necrotic", "prompt": "Bleached bone, spectral ghost flames, soul trap, shadowy mist, grim dark fantasy"},
        {"name": "holy",     "prompt": "Polished gold, platinum filigree, divine radiance, angel wings motif, celestial light"},
        {"name": "void",     "prompt": "Dark matter, swirling galaxy, purple nebula, eldritch stars, cosmic abyss"},
        {"name": "nature",   "prompt": "Ancient oak wood, emerald leaves, glowing moss, druidic vines, forest spirit"},
        {"name": "arcane",   "prompt": "Floating mana shards, glowing runes, arcane geometry, pink and cyan magic, sorcery"}
    ]

    # 针对防具的负面提示词优化：严禁出现人体部位
    negative_prompt = (
        "hand, person, character, arm, leg, body, face, wearing, model, human, man, woman, "
        "multiple views, front and back view, blurry, low quality, "
        "text, watermark, 3d render, realistic photo, background, messy, frame"
    )

    print(f"--- Starting Armor/Jewelry Batch Generation: {len(armor_types)} Types x {len(themes)} Themes ---")

    for a_type in armor_types:
        print(f"\n--- Processing Type: {a_type.upper()} ---")
        for theme in themes:
            asset_name = f"{a_type}_{theme['name']}"
            
            # 构建提示词: 强调 item icon, empty, no body
            prompt = f"{theme['prompt']} {a_type}, {theme['prompt']}, high detail, 2d game sprite, item icon"
            full_prompt = f"{prompt}, single object, isolated, centered, white background, empty, no body"
            
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