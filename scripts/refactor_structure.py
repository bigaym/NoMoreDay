import os
import shutil
import re

SRC_DIR = "src"
TEST_DIR = "tests"

# Mappings (Regex Pattern for File -> Destination Folder inside src/)
# Order matters! More specific rules first.
MOVES = [
    # --- App ---
    (r"^main\.cpp$", "app"),
    (r"^core[/\\]Game\..*", "app"),
    (r"^core[/\\]Settings\..*", "app"),
    (r"^core[/\\]SharedContext\.hpp", "app"),

    # --- Core ---
    (r"^tools[/\\]Logger\..*", "core/logging"),
    (r"^tools[/\\]CrashHandler\..*", "core/logging"),
    (r"^utils[/\\]PhysicsUtils\.hpp", "core/math"),
    (r"^utils[/\\]Tilemask\.hpp", "core/math"),
    (r"^utils[/\\]UUID\.hpp", "core/math"),
    (r"^utils[/\\]ThreadSafeRandom\.hpp", "core/math"),
    (r"^utils[/\\]Parallel\.hpp", "core/threading"),
    
    # --- Engine: Render ---
    (r"^systems[/\\]RenderSystem\..*", "engine/render"),
    (r"^core[/\\]UIRenderer\..*", "engine/render"),
    (r"^systems[/\\]GPU.*\..*", "engine/render"), 
    (r"^components[/\\]GPUData\.hpp", "engine/render"),
    (r"^utils[/\\]GPUUtils\.hpp", "engine/render"),
    (r"^core[/\\]ComputeBuffer\.hpp", "engine/render"),

    # --- Engine: Input ---
    (r"^systems[/\\]InputSystem\..*", "engine/input"),

    # --- Engine: Resource ---
    (r"^core[/\\]ResourceManager\..*", "engine/resource"),
    (r"^core[/\\]AssetRegistry\.hpp", "engine/resource"),
    (r"^core[/\\]EquipmentAssetRegistry\.hpp", "engine/resource"),
    (r"^core[/\\]UIAssetRegistry\.hpp", "engine/resource"),
    (r"^core[/\\]AssetLoadingSystem\..*", "engine/resource"),

    # --- Engine: Scene ---
    (r"^core[/\\]SceneManager\..*", "engine/scene"),
    (r"^core[/\\]StateManager\..*", "engine/scene"),
    (r"^core[/\\]State\.hpp", "engine/scene"),

    # --- Engine: Physics ---
    (r"^systems[/\\]PhysicsSystem\..*", "engine/physics"),
    (r"^systems[/\\]SpatialGrid\.hpp", "engine/physics"),

    # --- Game: Components ---
    (r"^components[/\\](?!GPUData\.hpp).*", "game/components"),

    # --- Game: Systems: Combat ---
    (r"^systems[/\\]CombatSystem\..*", "game/systems/combat"),
    (r"^systems[/\\]DamagePipeline\..*", "game/systems/combat"),
    (r"^systems[/\\]DamagePopupManager\.hpp", "game/systems/combat"),
    (r"^systems[/\\]StatsSystem\..*", "game/systems/combat"),
    (r"^systems[/\\]EffectSystem\..*", "game/systems/combat"),
    (r"^systems[/\\]RegenerationSystem\.hpp", "game/systems/combat"),
    (r"^systems[/\\]ProgressionSystem\..*", "game/systems/combat"),
    (r"^systems[/\\]XPAwardingSystem\..*", "game/systems/combat"),

    # --- Game: Systems: Skill ---
    (r"^systems[/\\]SkillSystem\..*", "game/systems/skill"),
    (r"^systems[/\\]ProjectileSystem\..*", "game/systems/skill"),
    (r"^systems[/\\]AstrolabeSystem\..*", "game/systems/skill"),
    (r"^systems[/\\]SummonSystem\..*", "game/systems/skill"),

    # --- Game: Systems: AI ---
    (r"^systems[/\\]AISystem\..*", "game/systems/ai"),
    (r"^systems[/\\]EnemyBehavior\.cpp", "game/systems/ai"),

    # --- Game: Systems: World ---
    (r"^systems[/\\]MapSystem\..*", "game/systems/world"),
    (r"^core[/\\]LevelManager\..*", "game/systems/world"),
    (r"^systems[/\\]FogOfWarSystem\..*", "game/systems/world"),
    (r"^systems[/\\]PortalSystem\..*", "game/systems/world"),
    (r"^systems[/\\]MovementStanceSystem\..*", "game/systems/world"),
    (r"^systems[/\\]EnemySpawnSystem\..*", "game/systems/world"),

    # --- Game: Systems: Item ---
    (r"^systems[/\\]InventorySystem\..*", "game/systems/item"),
    (r"^systems[/\\]DropSystem\..*", "game/systems/item"),
    (r"^systems[/\\]CraftingSystem\..*", "game/systems/item"),
    (r"^core[/\\]ItemFactory\..*", "game/systems/item"),
    (r"^core[/\\]LootFilter\..*", "game/systems/item"),
    (r"^core[/\\]LootTable\.hpp", "game/systems/item"),

    # --- Game: Systems: UI ---
    (r"^systems[/\\]UICrafting\..*", "game/systems/ui"),
    (r"^systems[/\\]UISystem\..*", "game/systems/ui"),
    (r"^systems[/\\]UI.*\..*", "game/systems/ui"),
    (r"^systems[/\\]PlayerHUD\..*", "game/systems/ui"),
    (r"^systems[/\\]MonsterHealthBarSystem\..*", "game/systems/ui"),
    (r"^core[/\\]UIContext\.hpp", "game/systems/ui"),

    # --- Game: States ---
    (r"^states[/\\](?!State\.hpp).*", "game/states"),

    # --- Game: Data (Registries) ---
    (r"^core[/\\]\w+Registry\..*", "game/data"),
]

header_map = {}

def get_files(root_dir):
    files = []
    for root, dirs, filenames in os.walk(root_dir):
        for f in filenames:
            files.append(os.path.join(root, f))
    return files

def normalize_path(path):
    return path.replace("\\", "/")

def main():
    print("Starting refactor...")
    
    # 1. Build a list of files to move
    files_to_move = []
    all_files = get_files(SRC_DIR)
    
    for file_path in all_files:
        rel_path = os.path.relpath(file_path, SRC_DIR)
        rel_path_norm = normalize_path(rel_path)
        
        dest_folder = None
        for pattern, dest in MOVES:
            if re.search(pattern, rel_path_norm):
                dest_folder = dest
                break
        
        if dest_folder:
            files_to_move.append((file_path, dest_folder))
        else:
            # If it's pch.hpp or something not matched, keep it or check manually
            if "pch.hpp" in rel_path_norm:
                pass # Keep in src/
            else:
                print(f"Warning: No rule for {rel_path_norm}")

    # 2. Execute Moves and Build Header Map
    for src_path, dest_folder in files_to_move:
        file_name = os.path.basename(src_path)
        dest_dir_full = os.path.join(SRC_DIR, dest_folder)
        
        if not os.path.exists(dest_dir_full):
            os.makedirs(dest_dir_full)
            
        dest_path = os.path.join(dest_dir_full, file_name)
        
        # Move
        print(f"Moving {src_path} -> {dest_path}")
        shutil.move(src_path, dest_path)
        
        # Record mapping: filename -> new_rel_path_from_src
        # Assumption: Filenames are unique enough or we handle collision?
        # For this project, filenames like "Common.hpp" might collide if we are not careful.
        # But for includes, people usually include "components/Common.hpp".
        # We need to map the HEADER NAME to the NEW FULL PATH relative to src.
        # But we also need to handle <path/Header.hpp> logic.
        
        new_rel_path = normalize_path(os.path.join(dest_folder, file_name))
        header_map[file_name] = new_rel_path
        
    # Remove empty dirs
    for root, dirs, files in os.walk(SRC_DIR, topdown=False):
        for name in dirs:
            try:
                os.rmdir(os.path.join(root, name))
            except OSError:
                pass # Not empty

    # 3. Update Includes
    # We scan ALL files in src and tests.
    scan_dirs = [SRC_DIR, TEST_DIR]    
    # Pre-calculate replacements: filename -> "new/path/filename"
    # We will look for #include "..." and Extract the filename.
    # If filename in header_map, replace entire string with header_map[filename].
    
    # Issues: 
    # #include "../core/Game.hpp" -> filename is Game.hpp -> app/Game.hpp
    # #include "components/Combat.hpp" -> filename is Combat.hpp -> game/components/Combat.hpp
    
    for d in scan_dirs:
        if not os.path.exists(d): continue
        for root, dirs, files in os.walk(d):
            for f in files:
                if f.endswith((".cpp", ".hpp", ".h", ".c")):
                    full_path = os.path.join(root, f)
                    with open(full_path, 'r', encoding='utf-8') as file:
                        content = file.read()
                    
                    new_content = content
                    
                    def replace_include(match):
                        full_include = match.group(1) # e.g. ../core/Game.hpp
                        filename = os.path.basename(full_include)
                        
                        if filename in header_map:
                            return f'#include "{header_map[filename]}"'
                        return match.group(0)

                    # Regex for #include "..."
                    # We only touch "" includes, not <> 
                    new_content = re.sub(r'#include\s+"([^"]+)"', replace_include, new_content)
                    
                    if new_content != content:
                        with open(full_path, 'w', encoding='utf-8') as file:
                            file.write(new_content)
                        print(f"Updated includes in {full_path}")

    # 4. Generate new source list for CMake
    sources = []
    for root, dirs, files in os.walk(SRC_DIR):
        for f in files:
            if f.endswith(".cpp"):
                path = os.path.join(root, f)
                rel = os.path.relpath(path, ".")
                sources.append(normalize_path(rel))
    
    with open("new_sources.txt", "w") as f:
        for s in sorted(sources):
            f.write(s + "\n")
    
    print("Done. New source list written to new_sources.txt")

if __name__ == "__main__":
    main()
