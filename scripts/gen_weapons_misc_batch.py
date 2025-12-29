import subprocess
import os

def generate_misc_assets():
    script_path = os.path.join(os.path.dirname(__file__), "asset_gen.py")
    output_width = 1024
    output_height = 1024

    # 1. 定义 10 种核心武器/副手类型 (基于设计文档)
    weapon_types = [
        "sword", "axe", "hammer", "dagger",       # 1H Melee
        "greatsword", "staff",                    # 2H
        "wand", "shield", "orb", "grimoire"       # Magic/Offhand
    ]

    # 2. 定义 10 种风格/元素主题 (10 Types * 10 Themes = 100 Assets)
    themes = [
        {"name": "iron",     "prompt": "worn iron, steel, realistic, mercenary style, battle hardened"},
        {"name": "fire",     "prompt": "burning, magma cracks, lava, ember, charred, fire magic"},
        {"name": "ice",      "prompt": "frozen, crystalline ice, frost aura, blue mist, winter"},
        {"name": "lightning","prompt": "crackling electricity, thunder, blue sparks, storm energy"},
        {"name": "poison",   "prompt": "dripping venom, green acid, snake motifs, toxic"},
        {"name": "necrotic", "prompt": "bone, skull, green ghostly flame, undead, death magic"},
        {"name": "holy",     "prompt": "gold, divine light, angel wings, pristine, paladin"},
        {"name": "void",     "prompt": "dark matter, purple galaxy, cosmic stars, eldritch horror"},
        {"name": "nature",   "prompt": "wooden, vines, leaves, blooming flowers, druid"},
        {"name": "arcane",   "prompt": "glowing runes, floating crystals, pink and blue magic, sorcery"}
    ]

    # 统一负面提示词
    negative_prompt = (
        "hand, person, character, arm, finger, holding, two items, "
        "multiple views, front and back view, scabbard, sheath, blurry, low quality, "
        "text, watermark, 3d render, realistic photo, background, messy, frame, human"
    )

    print(f"--- Starting Batch Generation: {len(weapon_types)} Types x {len(themes)} Themes = {len(weapon_types)*len(themes)} Assets ---")

    for w_type in weapon_types:
        print(f"\n--- Processing Type: {w_type.upper()} ---")
        for theme in themes:
            asset_name = f"{w_type}_{theme['name']}"
            
            # 构建提示词: [Theme Adjectives] [Weapon Type], [Details], [Style]
            prompt = f"{theme['prompt']} {w_type}, {theme['prompt']}, high detail, 2d game sprite"
            full_prompt = f"{prompt}, single object, isolated, centered, white background"
            
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