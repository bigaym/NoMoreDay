import json
import os

def update_biomes():
    path = 'assets/data/biomes.json'
    if not os.path.exists(path):
        print(f"File not found: {path}")
        return

    with open(path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    # Town -> 1, Cave -> 2, others start from 3
    mapping = {
        "town": 1,
        "cave": 2
    }
    
    next_id = 3
    for biome in data['biomes']:
        bid = biome['id']
        if bid in mapping:
            biome['numeric_id'] = mapping[bid]
        else:
            biome['numeric_id'] = next_id
            next_id += 1
            
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=4, ensure_ascii=False)
    print("Updated biomes.json with numeric_id")

if __name__ == "__main__":
    update_biomes()
