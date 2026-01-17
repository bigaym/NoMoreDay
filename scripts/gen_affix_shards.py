import re
import json
import os

def parse_affix_types(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Find AffixType enum
    match = re.search(r'enum class AffixType : uint16_t \{(.*?)\};', content, re.DOTALL)
    if not match:
        print("Could not find AffixType enum")
        return {}

    enum_content = match.group(1)
    
    # Remove comments
    enum_content = re.sub(r'//.*', '', enum_content)
    
    affixes = {}
    current_val = 0
    
    # Split by comma and process each entry
    entries = enum_content.split(',')
    for entry in entries:
        entry = entry.strip()
        if not entry:
            continue
            
        if '=' in entry:
            name, val_str = entry.split('=')
            name = name.strip()
            val_str = val_str.strip()
            
            # Handle markers or explicit values
            if val_str.isdigit():
                current_val = int(val_str)
            elif 'Start' in val_str or 'End' in val_str:
                # We don't really need to resolve these markers for the names, 
                # but let's try to find their values if they were defined before
                if val_str in affixes:
                    current_val = affixes[val_str]
                else:
                    # Best effort for simple markers
                    continue 
            else:
                continue
        else:
            name = entry
        
        if name and not (name.endswith('_Start') or name.endswith('_End') or name == 'Count'):
            # Only include Normal range affixes for shards
            if 0 <= current_val <= 999:
                affixes[name] = current_val
        
        current_val += 1
        
    return affixes

def generate_shards():
    hpp_path = 'src/game/components/ItemStats.hpp'
    json_path = 'assets/data/materials.json'
    
    affix_map = parse_affix_types(hpp_path)
    if not affix_map:
        return

    # Load existing materials
    if os.path.exists(json_path):
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
    else:
        data = {"materials": []}

    existing_ids = {m['id'] for m in data['materials']}
    
    # Generate Shards
    new_materials = []
    for name, val in affix_map.items():
        shard_id = 4000 + val
        if shard_id in existing_ids:
            continue
            
        # Format name: "Strength" -> "Strength Shard"
        display_name = re.sub(r'([a-z])([A-Z])', r'\1 \2', name) + " Shard"
        
        shard = {
            "id": shard_id,
            "name": display_name,
            "description": f"Powerful residue containing {display_name.replace(' Shard', '')}.",
            "category": "Affix Shard",
            "rarity": "Magic",
            "icon": f"icon_shard_{val}",
            "max_stack": 9999
        }
        new_materials.append(shard)

    # Add Legendary Essence
    if 4999 not in existing_ids:
        new_materials.append({
            "id": 4999,
            "name": "Legendary Essence",
            "description": "Pure essence extracted from legendary powers.",
            "category": "Affix Shard",
            "rarity": "Legendary",
            "icon": "icon_essence_legendary",
            "max_stack": 999
        })

    # Merge and Sort
    data['materials'].extend(new_materials)
    data['materials'].sort(key=lambda x: x['id'])

    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=4, ensure_ascii=False)
    
    print(f"Added {len(new_materials)} new affix shards to {json_path}")

if __name__ == "__main__":
    generate_shards()
