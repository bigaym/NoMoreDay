"""
NoMoreDay Affix Asset Generator
Purpose: Automatically generates 2D icons for equipment affixes and legendary affixes 
         using ComfyUI and AI (Flux.1). It maps affix IDs to visual concepts 
         and standardizes outputs to 128x128 transparent PNGs.

Usage:
    - Generate all normal affixes:
        python scripts/gen_affix_assets.py
    - Generate everything (including legendary):
        python scripts/gen_affix_assets.py --legendary
    - Regenerate a specific icon:
        python scripts/gen_affix_assets.py --single strength --force

Dependencies:
    - ComfyUI server running at 127.0.0.1:8188
    - scripts/asset_gen.py (for core generation logic)
"""
import os
import json
import argparse
import sys
from PIL import Image

# Add root directory to path to import asset_gen
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
try:
    import scripts.asset_gen as asset_gen
except ImportError:
    # Fallback if the path logic above fails in some environments
    import asset_gen

AFFIX_DATA_PATH = "assets/data/affixes.json"
LEGENDARY_DATA_PATH = "assets/data/legendary_affixes.json"
OUTPUT_DIR = "assets/textures/icons/affixes"
LEGENDARY_OUTPUT_DIR = "assets/textures/icons/legendary"

# Mapping affix IDs or Types to visual keywords for AI generation
VISUAL_MAPPING = {
    # Attributes
    "strength": "muscular arm, iron fist, weightlifting, strength icon",
    "dexterity": "running boots, wing, swiftness, agility icon",
    "intelligence": "glowing brain, magic book, scroll, wisdom icon",
    "vitality": "heart, shield with cross, life force, health icon",
    "all_stats": "balanced scales, four-pointed star, holistic power",
    
    # Damage Types
    "flat_phys": "sharp sword, jagged metal, physical impact",
    "pct_phys": "crossed axes, heavy mace, physical power",
    "flat_fire": "burning ember, small flame, fire orb",
    "pct_fire": "raging bonfire, phoenix wing, fire blast",
    "flat_cold": "ice crystal, snowflake, frost bite",
    "pct_cold": "frozen glacier, blizzard, ice storm",
    "flat_light": "electric spark, small bolt, lightning strike",
    "pct_light": "thunderstorm, high voltage, chain lightning",
    "flat_poison": "venom drop, green skull, toxic cloud",
    "pct_poison": "poisonous snake, bubbling acid, decay",
    "flat_shadow": "dark mist, crescent moon, shadow essence",
    "pct_shadow": "void portal, black hole, shadow realm",

    # Combat Stats
    "crit_chance": "target, bullseye, critical precision",
    "crit_damage": "shattering glass, explosive impact, high damage",
    "attack_speed": "blurred blades, rapid striking, clock with sword",
    "cast_speed": "glowing hands, magic ritual, arcane acceleration",
    "accuracy": "eagle eye, crosshair, perfect aim",
    
    # Defensive
    "flat_armor": "steel plate, heavy shield, armor plating",
    "pct_armor": "fortified wall, impenetrable defense, castle icon",
    "flat_health": "plus sign, red potion, vitality essence",
    "flat_mana": "blue crystal, magic mana drop, arcane source",
    "resist_all": "rainbow shield, prismatic barrier, universal protection",
    "resist_fire": "fireproof shield, salamander skin, heat resistance",
    "resist_cold": "warm coat, campfire, frost resistance",
    "resist_light": "lightning rod, insulated shield, electric resistance",
    "thorns": "barbed wire, cactus, spike armor",
    "damage_reduction": "deflecting shield, absorption barrier",

    # Recovery
    "health_regen": "healing sprout, green cross, regeneration aura",
    "mana_regen": "mana spring, blue flowing water, psychic focus",
    "life_steal": "vampire fangs, blood drop, life drain",
    "life_on_hit": "life spark on impact, kinetic healing",

    # Utility
    "move_speed": "winged sandals, trail of dust, rapid movement",
    "cooldown_reduction": "hourglass, sand of time, skill refresh",
    "plus_all_skills": "golden crown, master badge, ultimate skill",
}

def get_affix_prompt(affix_id, name_template):
    # Try to find a specific mapping, otherwise use the name template
    visual = VISUAL_MAPPING.get(affix_id.lower())
    if not visual:
        # Generic fallback based on name
        visual = f"{name_template} magical essence, abstract RPG icon"
    
    prompt = f"{visual}, game icon, fantasy style, high quality, vector style, sharp edges, centered, symmetrical"
    return prompt

def process_affixes(data_path, output_dir, force=False):
    if not os.path.exists(data_path):
        print(f"Warning: File {data_path} not found.")
        return

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    with open(data_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    # Connect to ComfyUI via asset_gen
    if not asset_gen.is_server_running():
        asset_gen.start_comfyui()
    
    ws = asset_gen.websocket.WebSocket()
    ws.connect(f"ws://{asset_gen.SERVER_ADDRESS}/ws?clientId={asset_gen.CLIENT_ID}")

    for entry in data:
        affix_id = entry.get("id")
        name = entry.get("nameTemplate", affix_id)
        
        # Determine output filename
        file_name = f"icon_{affix_id}.png"
        file_path = os.path.join(output_dir, file_name)

        if os.path.exists(file_path) and not force:
            print(f"Skipping {affix_id} (already exists).")
            continue

        print(f"\n>>> Generating Asset for Affix: {affix_id} ({name})")
        prompt_text = get_affix_prompt(affix_id, name)
        
        workflow = asset_gen.get_default_workflow()
        workflow["6"]["inputs"]["text"] = f"{prompt_text}, white background, isolated, 2d game sprite"
        workflow["5"]["inputs"]["width"] = 512 # Generation size (will be downscaled)
        workflow["5"]["inputs"]["height"] = 512

        try:
            image_paths = asset_gen.get_images(ws, workflow, output_dir, f"tmp_{affix_id}", True)
            
            if image_paths:
                # Rename the first generated image to our desired name
                # and ensure it's 128x128
                final_tmp_path = image_paths[0]
                with Image.open(final_tmp_path) as img:
                    img_res = img.resize((128, 128), Image.Resampling.LANCZOS)
                    img_res.save(file_path, "PNG")
                
                # Cleanup tmp files
                for p in image_paths:
                    if os.path.exists(p): os.remove(p)
                print(f"Success: {file_path}")
            else:
                print(f"Failed to generate image for {affix_id}")
        except Exception as e:
            print(f"Error during generation for {affix_id}: {e}")

    ws.close()

def main():
    parser = argparse.ArgumentParser(description="NoMoreDay Affix Asset Generator")
    parser.add_argument("--force", action="store_true", help="Force regenerate existing assets")
    parser.add_argument("--legendary", action="store_true", help="Also generate assets for legendary affixes")
    parser.add_argument("--single", type=str, help="Generate only a single affix by ID")
    
    args = parser.parse_args()

    print("Starting Affix Asset Generation...")
    
    if args.single:
        # Mock a small data list for single generation
        single_data = [{"id": args.single, "nameTemplate": args.single}]
        # We need to decide which output dir to use
        process_affixes_list(single_data, OUTPUT_DIR, args.force)
    else:
        # Process normal affixes
        process_affixes(AFFIX_DATA_PATH, OUTPUT_DIR, args.force)
        
        # Process legendary affixes if requested
        if args.legendary:
            process_affixes(LEGENDARY_DATA_PATH, LEGENDARY_OUTPUT_DIR, args.force)

def process_affixes_list(entries, output_dir, force=False):
    # Re-using the logic from process_affixes but with a direct list
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    if not asset_gen.is_server_running():
        asset_gen.start_comfyui()
    
    ws = asset_gen.websocket.WebSocket()
    ws.connect(f"ws://{asset_gen.SERVER_ADDRESS}/ws?clientId={asset_gen.CLIENT_ID}")

    for entry in entries:
        affix_id = entry.get("id")
        file_name = f"icon_{affix_id}.png"
        file_path = os.path.join(output_dir, file_name)

        if os.path.exists(file_path) and not force:
            print(f"Skipping {affix_id} (already exists).")
            continue

        print(f"Generating: {affix_id}")
        prompt_text = get_affix_prompt(affix_id, affix_id)
        workflow = asset_gen.get_default_workflow()
        workflow["6"]["inputs"]["text"] = f"{prompt_text}, white background, isolated, 2d game sprite"
        
        image_paths = asset_gen.get_images(ws, workflow, output_dir, f"tmp_{affix_id}", True)
        if image_paths:
            final_tmp_path = image_paths[0]
            with Image.open(final_tmp_path) as img:
                img_res = img.resize((128, 128), Image.Resampling.LANCZOS)
                img_res.save(file_path, "PNG")
            for p in image_paths: os.remove(p)

    ws.close()

if __name__ == "__main__":
    main()
