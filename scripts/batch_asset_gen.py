"""
NoMoreDay Batch Asset Generator
===============================
Purpose: 
    Automates the generation of high-quality 2D game assets for the NoMoreDay project.
    It acts as a wrapper for `scripts/asset_gen.py`, orchestrating the creation of
    monsters, player characters, environment tiles, equipment, and VFX textures.

Key Features:
    - Organized Asset Groups: Structured by race (Undead, Demon, etc.) and archetypes (Tank, Warrior, Mage, etc.).
    - Style Consistency: Enforces the "High-Fidelity Pixel Art + Ink Wash" aesthetic via optimized BASE_STYLE prompts.
    - Directory Management: Automatically prefixes assets with their target directories (e.g., monster/, vfx/).
    - Model Flexibility: Supports passing specific FLUX.1/FLUX.2 models to the underlying generator.

Usage:
    python scripts/batch_asset_gen.py [options]

Options:
    --category <name>   Generate only a specific category (e.g., monsters, players, vfx).
                        If omitted, generates all defined assets.
    --model <name>      Specify a custom model name (e.g., flux-2-klein-9b.safetensors).

Examples:
    1. Generate all monsters:
       python scripts/batch_asset_gen.py --category monsters

    2. Generate everything using a specific model:
       python scripts/batch_asset_gen.py --model flux-2-klein-base-4b.safetensors

    3. Generate only VFX assets:
       python scripts/batch_asset_gen.py --category vfx

Dependencies:
    - scripts/asset_gen.py (The core generation script)
    - ComfyUI (Running locally at 127.0.0.1:8188)
"""
import subprocess
import os
import time

# Configuration for Asset Tiers
TIER_SUFFIX = ["_0", "_1", "_2"] # Fodder, Elite, Boss

# Base Style Keywords (FLUX-optimized: Descriptive & Natural)
BASE_STYLE = "2d game sprite, high-fidelity pixel art style, combined with traditional chinese ink wash painting aesthetics, dark fantasy, ancient chinese mystical atmosphere, high contrast, detailed texture, clean white background, isolated."

ASSET_GROUPS = {
    "monsters": [
        # --- Undead (不死族) ---
        {"name": "monster/skeleton_0", "prompt": "Skeleton warrior, physical warrior, holding a rusty sword, bleached bones, dark aura"},
        {"name": "monster/skeleton_1", "prompt": "Skeleton archer, physical ranged, rotten wooden bow, ragged leather scraps"},
        {"name": "monster/skeleton_2", "prompt": "Skeleton guard, tank with heavy iron shield, dark iron plate armor, glowing blue eyes"},
        {"name": "monster/skeleton_3", "prompt": "Skeleton shadow, agility melee, dual bone daggers, tattered cloak, ghost-like aura"},
        {"name": "monster/skeleton_4", "prompt": "Skeleton lich, magic ranged, holding a skull staff, green soul fire, tattered wizard robes"},

        # --- Demon (恶魔) ---
        {"name": "monster/demon_0", "prompt": "Imp, agility melee scout, small red skin, jagged horns, fast movement"},
        {"name": "monster/demon_1", "prompt": "Demon soldier, physical warrior, muscular charred skin, hell-forged axe"},
        {"name": "monster/demon_2", "prompt": "Demon bulwark, tank with massive molten shield and spikes, heavy plate armor"},
        {"name": "monster/demon_3", "prompt": "Demon marksman, physical ranged, burning crossbow, winged silhouette"},
        {"name": "monster/demon_4", "prompt": "Demon sorcerer, magic ranged, purple fireballs, ornate horned crown"},

        # --- Corrupted (堕落者) ---
        {"name": "monster/warcraft_0", "prompt": "Abomination, physical warrior, flesh golem with multiple arms, twisted purple void energy"},
        {"name": "monster/warcraft_1", "prompt": "Void horror, magic ranged, floating eye, ink-wash tentacles, shifting void form"},
        {"name": "monster/warcraft_2", "prompt": "Fleshy shield, tank, massive wall of muscle and bone, impenetrable skin"},
        {"name": "monster/warcraft_3", "prompt": "Void slasher, agility melee, beast-like form, sharp claws, moving through ink mist"},
        {"name": "monster/warcraft_4", "prompt": "Plague spitter, physical ranged, hunchbacked monster spitting organic projectiles"},

        # --- Cultist (邪教徒) ---
        {"name": "monster/cultist_0", "prompt": "Cultist acolyte, physical warrior, hooded robe, serrated mace, face in shadow"},
        {"name": "monster/cultist_1", "prompt": "Cultist sorcerer, magic ranged, ancient chinese talismans flying around, levitating dark orb"},
        {"name": "monster/cultist_2", "prompt": "Cultist zealot, tank with tower shield, heavy priest armor, glowing runes"},
        {"name": "monster/cultist_3", "prompt": "Cultist shadow, agility melee, dark silken robes, hidden daggers, poison aura"},
        {"name": "monster/cultist_4", "prompt": "Cultist sniper, physical ranged, heavy repeating crossbow, ritual tattoos"},

        # --- Elves (精灵) ---
        {"name": "monster/elf_0", "prompt": "Fallen elf warrior, physical warrior, dark green armor, curved blade"},
        {"name": "monster/elf_1", "prompt": "Fallen elf ranger, physical ranged, elegant black wood bow, dark light arrow"},
        {"name": "monster/elf_2", "prompt": "Fallen elf defender, tank, vine-wrapped shield, amber eyes"},
        {"name": "monster/elf_3", "prompt": "Fallen elf blade dancer, agility melee, dual swords, fast pose"},
        {"name": "monster/elf_4", "prompt": "Fallen elf druid, magic ranged, wooden staff, dark thorns aura"},

        # --- Beast (兽人) ---
        {"name": "monster/beast_0", "prompt": "Beast brute, physical warrior, tiger head humanoid, heavy axe, muscular"},
        {"name": "monster/beast_1", "prompt": "Beast shaman, magic ranged, bone necklace, totem staff, tribal tattoos"},
        {"name": "monster/beast_2", "prompt": "Beast ironhide, tank with bone shield, primitive stone armor"},
        {"name": "monster/beast_3", "prompt": "Beast slinger, physical ranged, stone javelin, savage leather wraps"},
        {"name": "monster/beast_4", "prompt": "Beast pouncer, agility melee, sharp claws, animalistic crouch"},
        
        # --- Goblins/Yecha (妖群/夜叉) ---
        {"name": "monster/goblin_0", "prompt": "Fat gulping demon, tank, massive belly like a cauldron, using a heavy iron cooking pot as a shield, gluttonous expression"},
        {"name": "monster/goblin_1", "prompt": "Bone-blade yecha, physical warrior, holding a jagged bone club, savage dry-brush ink strokes, muscular green/grey skin"},
        {"name": "monster/goblin_2", "prompt": "Mist-stalking imp, agility melee, small translucent ink body, long obsidian claws, glowing yellow cat-eyes"},
        {"name": "monster/goblin_3", "prompt": "Crow-feather slinger, physical ranged, hunched goblin throwing sharpened obsidian disks, wearing a tattered crow-feather cloak"},
        {"name": "monster/goblin_4", "prompt": "Soul-lure shaman, magic ranged, small bent figure holding a tall staff with a blue spirit lantern, summoning ghostly ink mists"},

        # --- Mechanisms (机关偶人) ---
        {"name": "monster/mech_0", "prompt": "Bronze bell guardian, tank, massive ancient temple bell as torso, stone pillar as a club, heavy gears visible in joints"},
        {"name": "monster/mech_1", "prompt": "Thousand-arm automaton, physical warrior, wooden and bronze construct, multiple arms holding iron guandaos, rotating gear core"},
        {"name": "monster/mech_2", "prompt": "Spider-needle construct, agility melee, spindly bronze legs, lighting-fast puncturing needle arm, clockwork precision"},
        {"name": "monster/mech_3", "prompt": "Dragon-fire ballista, physical ranged, mobile mechanical dragon on wheels, launching iron bolts, steam and ink smoke"},
        {"name": "monster/mech_4", "prompt": "Rune-core sentry, magic ranged, levitating bronze sphere with spinning celestial rings, shooting concentrated spirit beams"},

        --- Elementals (五行灵体) ---
        {"name": "monster/elemental_0", "prompt": "Mountain-root golem, tank (Earth), rough dry-brush rock textures, body made of boulders and ancient roots, immoveable weight"},
        {"name": "monster/elemental_1", "prompt": "Burning ashen wraith, physical warrior (Fire), aggressive fluid brushstrokes, charred ink core, humanoid shape of dancing embers"},
        {"name": "monster/elemental_2", "prompt": "Static-spark spirit, agility melee (Lightning), jagged sharp ink lines, appearing in electrical flashes, fragmented energy form"},
        {"name": "monster/elemental_3", "prompt": "Glacier-shard elemental, physical ranged (Ice), shooting sharp translucent ink spikes, frozen jagged edges, cold blue aura"},
        {"name": "monster/elemental_4", "prompt": "Swirling vortex entity, magic ranged (Wind/Water), soft ink washes with a deep dark core, pulling magical energy into a spiral"},
    ],
    "players": [
        {"name": "characters/player_warrior", "prompt": "noble cultivation swordsman, flowing cyan robes, white headband, holding a glowing spiritual jade sword, heroic standing pose"},
        {"name": "characters/player_assassin", "prompt": "shadow blade cultivator, tight dark grey hanfu, masked face, holding dual spiritual daggers, agile assassin stance"},
        {"name": "characters/player_mage", "prompt": "talisman master taoist, white and gold robes, levitating multiple yellow paper charms, holding a wooden ritual sword"},
        {"name": "characters/player_heavy", "prompt": "temple guardian monk, bare chest with prayer beads, holding a massive golden staff, radiant aura"},
        {"name": "characters/player_archer", "prompt": "spirit archer, light green silk armor, ethereal bow made of light, elegant pose"},
    ],
    "portals": [
        {"name": "environment/env_portal_ghostly", "prompt": "mystical portal gate, swirling cyan spectral energy, ancient weathered stone archway with ghost runes"},
        {"name": "environment/env_portal_abyssal", "prompt": "nightmarish void portal, purple and black swirling core, dark tentacles emerging from the frame"},
        {"name": "environment/env_portal_divine", "prompt": "radiant celestial portal, golden sunlight beams, white clouds, ornate lotus flower frame"},
        {"name": "environment/env_portal_infernal", "prompt": "hellish demon gate, molten lava core, obsidian spikes, burning demonic runes"},
        {"name": "environment/env_portal_arcane", "prompt": "shimmering mana portal, bright blue energy sparks, floating crystal shards around the gateway"},
    ],
    "decorations": [
        {"name": "environment/env_tree_dead", "prompt": "twisted gnarled dead tree, black ink-like bark, spooky silhouette, gallows-like branches"},
        {"name": "environment/env_bamboo_misty", "prompt": "cluster of thick green bamboo, heavy morning mist, ink wash textures, zen peaceful atmosphere"},
        {"name": "environment/env_rock_cluster", "prompt": "sharp jagged obsidian rocks, mountain peak miniaturized, cracks with faint spirit glow"},
        {"name": "environment/env_statue_broken", "prompt": "broken ancient stone statue of a deity, moss-covered, head missing, intricate carvings"},
        {"name": "environment/env_incense_burner", "prompt": "large bronze incense burner, curly smoke rising, ancient chinese temple style, weathered material"},
    ],
    "tiles": [
        {"name": "environment/tile_ground_dark", "prompt": "seamless dark soil texture, ink wash painting style, cracked earth with faint mystical energy"},
        {"name": "environment/tile_grass_ink", "prompt": "seamless sparse grass texture, desaturated green and black ink strokes, mystical meadow"},
        {"name": "environment/tile_paving_stone", "prompt": "seamless ancient cracked stone slabs, mossy weathered rock, ruin floor texture"},
        {"name": "environment/tile_water_ink", "prompt": "seamless dark rippling water, ink pool aesthetic, abstract moonlight reflections"},
        {"name": "environment/wall_stone_ancient", "prompt": "ancient stone wall bricks, high contrast ink wash, eroded and covered in frost/vines"},
    ],
    "equipment": [
        {"name": "equipment/item_helmet_jade", "prompt": "ornate cultivation helmet made of polished green jade, golden carvings, ancient chinese crown style"},
        {"name": "equipment/item_pauldrons_iron", "prompt": "heavy iron pauldrons for shoulders, lion head carvings, dark metallic texture with gold edges"},
        {"name": "equipment/item_armor_chest", "prompt": "heavy plated chest armor, dragon motifs embossed, dark iron and gold trim"},
        {"name": "equipment/item_gauntlets_leather", "prompt": "reinforced leather gauntlets, obsidian plate reinforcements, glowing runes on the knuckles"},
        {"name": "equipment/item_leggings_silk", "prompt": "flowing silk leggings with protective bronze plates, intricate cloud patterns, cultivation style"},
        {"name": "equipment/item_boots_flying", "prompt": "silk cultivation boots with wing-like patterns, glowing white spirit energy at the soles"},
        {"name": "equipment/item_amulet_mirror", "prompt": "ancient bronze bagua mirror amulet, red string, mystical carvings, glowing center"},
        {"name": "equipment/item_ring_dragon", "prompt": "golden ring shaped like a coiled dragon, clutching a small glowing pearl"},
        {"name": "equipment/item_ring_phoenix", "prompt": "silver ring in the shape of a phoenix wing, set with a burning ruby"},
        {"name": "equipment/item_weapon_main_sword", "prompt": "legendary cultivation longsword, glowing white spirit edge, hilt decorated with ying-yang symbol"},
        {"name": "equipment/item_weapon_off_shield", "prompt": "sturdy bronze shield with a taotie mask carving, glowing emerald eyes, defensive aura"},
    ],
    "vfx": [
        {"name": "vfx/vfx_ink_splatter", "prompt": "artistic ink splatter explosion, random organic shapes, high contrast black ink, splatter effect for combat"},
        {"name": "vfx/spirit_sword", "prompt": "translucent blue glowing spirit sword, ethereal blade, trail of light particles"},
        {"name": "vfx/vfx_rune_array", "prompt": "complex circular magical array, ancient chinese runes and trigrams, glowing cyan light"},
        {"name": "vfx/trail_mask", "prompt": "linear wind slash trail, white and blue gradient, speed lines, sharp cutting motion"},
        {"name": "vfx/energy_noise", "prompt": "seamless mystical energy noise texture, clouds of spirit power, soft white and grey flow"},
        {"name": "vfx/distortion_normal", "prompt": "swirling air distortion pattern, heat wave effect, glass-like ripples, space warping texture"},
        {"name": "vfx/vfx_circle_shockwave", "prompt": "circular shockwave expanding outwards, blue and white energy ripples, forceful impact visual"},
    ],
}

def generate_asset(name, prompt, model=None, width=None, height=None):
    full_prompt = f"{prompt}, {BASE_STYLE}"
    cmd = [
        "python", "scripts/asset_gen.py",
        "--prompt", full_prompt,
        "--name", name
    ]
    if model:
        cmd.extend(["--model", model])
    if width:
        cmd.extend(["--width", str(width)])
    if height:
        cmd.extend(["--height", str(height)])
    
    print(f"\n>>> Generating: {name}...")
    print(f">>> Prompt: {full_prompt}")
    
    try:
        # Run and wait for it to finish
        result = subprocess.run(cmd, check=True)
        return True
    except subprocess.CalledProcessError:
        print(f"!!! Failed to generate {name}")
        return False

def main():
    import argparse
    parser = argparse.ArgumentParser(description="NoMoreDay Batch Asset Generator")
    parser.add_argument("--category", type=str, help="Specific category to generate (monsters, players, portals, etc.)")
    parser.add_argument("--model", type=str, help="Specific model to use in asset_gen.py")
    parser.add_argument("--width", type=int, default=512, help="Specific width to use in asset_gen.py")
    parser.add_argument("--height", type=int, default=512, help="Specific height to use in asset_gen.py")
    args = parser.parse_args()

    start_time = time.time()
    total_generated = 0
    
    print("NoMoreDay Batch Asset Generation Started (FLUX2 & Ink-Wash Style)")
    
    target_categories = [args.category] if args.category else ASSET_GROUPS.keys()

    for group in target_categories:
        if group not in ASSET_GROUPS:
            print(f"Warning: Category '{group}' not found in ASSET_GROUPS")
            continue

        assets = ASSET_GROUPS[group]
        print(f"\n=== Processing Category: {group} ===")
        for asset in assets:
            success = generate_asset(asset["name"], asset["prompt"], model=args.model, width=args.width, height=args.height)
            if success:
                total_generated += 1
            
            time.sleep(1)

    end_time = time.time()
    duration = (end_time - start_time) / 60
    print(f"\nBatch Job Finished!")
    print(f"Total Assets Generated: {total_generated}")
    print(f"Total Time Taken: {duration:.2f} minutes")

if __name__ == "__main__":
    main()
