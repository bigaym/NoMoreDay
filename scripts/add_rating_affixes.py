import json
import os

affixes_path = 'assets/data/affixes.json'

new_affixes = [
    {
        "id": "flat_dodge_rating",
        "type": 48,
        "nameTemplate": "Elusive",
        "isPrefix": True,
        "tiers": [
            {"tier": 1, "minLevel": 1, "minValue": 10.0, "maxValue": 15.0},
            {"tier": 2, "minLevel": 12, "minValue": 20.0, "maxValue": 30.0},
            {"tier": 3, "minLevel": 25, "minValue": 40.0, "maxValue": 55.0},
            {"tier": 4, "minLevel": 35, "minValue": 70.0, "maxValue": 90.0},
            {"tier": 5, "minLevel": 45, "minValue": 110.0, "maxValue": 140.0},
            {"tier": 6, "minLevel": 55, "minValue": 160.0, "maxValue": 200.0},
            {"tier": 7, "minLevel": 65, "minValue": 220.0, "maxValue": 280.0}
        ],
        "allowedTags": ["armor", "boots", "gloves", "jewelry"]
    },
    {
        "id": "percent_dodge_rating",
        "type": 49,
        "nameTemplate": "Evading",
        "isPrefix": True,
        "tiers": [
            {"tier": 1, "minLevel": 5, "minValue": 5.0, "maxValue": 10.0},
            {"tier": 2, "minLevel": 15, "minValue": 12.0, "maxValue": 18.0},
            {"tier": 3, "minLevel": 30, "minValue": 22.0, "maxValue": 30.0},
            {"tier": 4, "minLevel": 45, "minValue": 35.0, "maxValue": 45.0},
            {"tier": 5, "minLevel": 60, "minValue": 50.0, "maxValue": 65.0},
            {"tier": 6, "minLevel": 70, "minValue": 70.0, "maxValue": 85.0},
            {"tier": 7, "minLevel": 80, "minValue": 90.0, "maxValue": 110.0}
        ],
        "allowedTags": ["armor", "boots"]
    },
    {
        "id": "flat_block_rating",
        "type": 50,
        "nameTemplate": "Deflecting",
        "isPrefix": True,
        "tiers": [
            {"tier": 1, "minLevel": 1, "minValue": 10.0, "maxValue": 20.0},
            {"tier": 2, "minLevel": 15, "minValue": 30.0, "maxValue": 50.0},
            {"tier": 3, "minLevel": 30, "minValue": 60.0, "maxValue": 90.0},
            {"tier": 4, "minLevel": 45, "minValue": 110.0, "maxValue": 150.0},
            {"tier": 5, "minLevel": 60, "minValue": 180.0, "maxValue": 240.0},
            {"tier": 6, "minLevel": 70, "minValue": 280.0, "maxValue": 360.0},
            {"tier": 7, "minLevel": 80, "minValue": 400.0, "maxValue": 500.0}
        ],
        "allowedTags": ["armor", "weapon"]
    },
    {
        "id": "percent_block_rating",
        "type": 51,
        "nameTemplate": "Blocking",
        "isPrefix": True,
        "tiers": [
            {"tier": 1, "minLevel": 10, "minValue": 10.0, "maxValue": 20.0},
            {"tier": 2, "minLevel": 25, "minValue": 25.0, "maxValue": 35.0},
            {"tier": 3, "minLevel": 40, "minValue": 40.0, "maxValue": 55.0},
            {"tier": 4, "minLevel": 55, "minValue": 60.0, "maxValue": 80.0},
            {"tier": 5, "minLevel": 70, "minValue": 85.0, "maxValue": 110.0},
            {"tier": 6, "minLevel": 80, "minValue": 115.0, "maxValue": 145.0},
            {"tier": 7, "minLevel": 90, "minValue": 150.0, "maxValue": 200.0}
        ],
        "allowedTags": ["armor", "weapon"]
    }
]

def main():
    if not os.path.exists(affixes_path):
        print(f"Error: {affixes_path} not found.")
        return

    try:
        with open(affixes_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        existing_types = {item['type'] for item in data}
        
        added_count = 0
        for affix in new_affixes:
            if affix['type'] in existing_types:
                print(f"Skipping type {affix['type']} ({affix['id']}) - already exists.")
                continue
            data.append(affix)
            added_count += 1
            print(f"Added {affix['id']} (Type {affix['type']})")
            
        if added_count > 0:
            with open(affixes_path, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=4, ensure_ascii=False)
            print(f"Successfully added {added_count} new affixes to {affixes_path}.")
        else:
            print("No new affixes added.")

    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    main()
