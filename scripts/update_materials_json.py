import json
import os

def update_materials():
    path = 'assets/data/materials.json'
    if not os.path.exists(path):
        print(f"File not found: {path}")
        return

    with open(path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    # Misc=0, Mineral=1, Fragment=2, Rune=3, AffixShard=4
    mapping = {
        "Misc": 0,
        "Mineral": 1,
        "Fragment": 2,
        "Rune": 3,
        "Affix Shard": 4
    }
    
    for mat in data['materials']:
        cat = mat.get('category', 'Misc')
        mat['category_id'] = mapping.get(cat, 0)
            
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=4, ensure_ascii=False)
    print("Updated materials.json with category_id")

if __name__ == "__main__":
    update_materials()
