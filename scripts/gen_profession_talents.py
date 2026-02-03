import json

def main():
    professions = [
        { "id": 0, "name": "Blade Ascendant", "desc": "Sword Cultivator" },
        { "id": 1, "name": "Mage", "desc": "Elemental Master" },
        { "id": 2, "name": "Priest", "desc": "Healer" },
        { "id": 3, "name": "Knight", "desc": "Defender" },
        { "id": 4, "name": "Ranger", "desc": "Sharpshooter" },
        { "id": 5, "name": "Berserker", "desc": "Rage Warrior" }
    ]

    nodes = []

    for prof in professions:
        pid = prof["id"]
        
        # Tier 1: Minor nodes (5 per profession)
        for i in range(5):
            node = {
                "id": (pid + 1) * 1000 + 100 + i,
                "profession": pid,
                "type": "Minor",
                "tier": 1,
                "sector_index": i, # Distribute across sector slots
                "max_points": 5,
                "name_key": f"{prof['name']} Minor {i+1}",
                "desc_key": "Minor stat boost",
                "modifiers": [],
                "icon_id": "icon_minor"
            }
            nodes.append(node)

        # Tier 2: Major nodes (2 per profession)
        for i in range(2):
            node = {
                "id": (pid + 1) * 1000 + 200 + i,
                "profession": pid,
                "type": "Major",
                "tier": 2,
                "sector_index": i * 2, # Spread them out
                "max_points": 3,
                "name_key": f"{prof['name']} Major {i+1}",
                "desc_key": "Major stat boost",
                "damage_modifiers": [],
                "icon_id": "icon_major"
            }
            nodes.append(node)

        # Tier 3: Core node (1 per profession)
        node = {
            "id": (pid + 1) * 1000 + 300,
            "profession": pid,
            "type": "Core",
            "tier": 3,
            "sector_index": 0,
            "max_points": 1,
            "name_key": f"{prof['name']} Core",
            "desc_key": "Unlock core mechanics",
            "effects": [],
            "icon_id": "icon_core"
        }
        nodes.append(node)

    data = {
        "version": 1,
        "profession_stars": [
            { "profession": p["id"], "name_key": p["name"], "desc_key": p["desc"] }
            for p in professions
        ],
        "nodes": nodes
    }

    with open("assets/data/profession_talents.json", "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    
    print(f"Generated {len(nodes)} nodes for {len(professions)} professions.")

if __name__ == "__main__":
    main()
