import json
import os

def update_astrolabe():
    path = 'assets/data/astrolabe.json'
    if not os.path.exists(path):
        print(f"File not found: {path}")
        return

    with open(path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    # Trait mapping
    mapping = {
        "SwordHeart": 100,
        "SwordIntentUnlock": 101,
        "MaxSwordIntent:": 200,
        "SwordIntentGain:": 201,
        "SwordIntentGrace:": 202
    }
    
    for node in data['nodes']:
        for effect in node.get('effects', []):
            val = effect.get('value', '')
            found = False
            for k, v in mapping.items():
                if val.startswith(k):
                    effect['trait_id'] = v
                    found = True
                    break
            if not found:
                effect['trait_id'] = 0
            
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=4, ensure_ascii=False)
    print("Updated astrolabe.json with trait_id")

if __name__ == "__main__":
    update_astrolabe()
