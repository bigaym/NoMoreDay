
"""
NoMoreDay FNV1a-32 Hash Checker
Purpose: Calculates FNV1a-32 hashes for strings to match C++ compile-time hashes.
         Used for verifying asset IDs and component tags.
Usage: python scripts/check_hash.py
"""
def fnv1a_32(string):
    hash_val = 0x811c9dc5
    for char in string:
        hash_val ^= ord(char)
        hash_val = (hash_val * 0x01000193) & 0xFFFFFFFF
    return hash_val

ids_to_check = [
    "equip_sword_sword_0",
    "equip_sword_sword_1",
    "equip_sword_sword_2",
    "equip_sword_sword_3",
    "equip_sword_sword_4",
    "equip_sword_sword_5",
    "equip_sword_sword_6",
    "equip_sword_sword_7",
    "equip_sword_sword_8",
    "equip_sword_sword_9",
    "equip_sword_sword_10",
]

for s in ids_to_check:
    print(f"{s}: {fnv1a_32(s)}")

target = 1648725103
# Try some other patterns
categories = ["sword", "axe", "dagger", "hammer", "greatsword", "staff", "wand", "helmet", "chest", "pauldrons", "gauntlets", "leggings", "boots", "shield", "amulet", "ring"]
for cat in categories:
    for i in range(30):
        s = f"equip_{cat}_{cat}_{i}"
        if fnv1a_32(s) == target:
            print(f"FOUND MATCH: {s}")
