import json
import os

# Enum mapping (Must match AffixType in C++)
AffixType = {
    "Strength": 0, "Dexterity": 1, "Intelligence": 2, "Vitality": 3,
    "FlatPhysicalDamage": 4, "FlatFireDamage": 5, "FlatColdDamage": 6, "FlatLightningDamage": 7, "FlatPoisonDamage": 8, "FlatShadowDamage": 9,
    "PercentPhysicalDamage": 10, "PercentFireDamage": 11, "PercentColdDamage": 12, "PercentLightningDamage": 13, "PercentPoisonDamage": 14, "PercentShadowDamage": 15,
    "CritChance": 16, "CritDamage": 17, "AttackSpeed": 18, "CastSpeed": 19, "Accuracy": 20,
    "FlatArmor": 21, "PercentArmor": 22, "FlatHealth": 23, "PercentHealth": 24, "FlatMana": 25,
    "ResistAll": 26, "ResistFire": 27, "ResistCold": 28, "ResistLightning": 29, "ResistPoison": 30, "ResistShadow": 31,
    "Thorns": 32, "DamageReduction": 33,
    "MoveSpeed": 34, "CooldownReduction": 35, "LifeSteal": 36, "LifeOnHit": 37
}

definitions = []

def add_def(name_id, type_name, name_template, is_prefix, base_val, scale_val, variance, tags):
    tiers = []
    for t in range(1, 8):
        # Linear scaling logic from C++: (base * scale) + random(0, variance)
        # Here we define min/max for the tier
        # Original: (base * tier) + [0, variance]
        # But wait, original code was: value = rollVal(base, variance)
        # rollVal = (base * scale) + random(0, variance)
        # So min = base * tier, max = base * tier + variance
        
        # But MoveSpeed was: 5.0 + (tier * 2.0)
        # So min = 5 + 2*t
        
        # Let's support "linear" mode vs "base" mode
        
        min_v = 0
        max_v = 0
        
        if name_id == "move_speed":
            min_v = 5.0 + (t * 2.0)
            max_v = min_v # No variance in original code?
        elif name_id == "crit_chance":
            min_v = 1.0 + (t * 0.8)
            max_v = min_v
        elif name_id == "attack_speed":
            min_v = 5.0 + (t * 1.5)
            max_v = min_v
        else:
            min_v = base_val * t
            max_v = min_v + variance
            
        tiers.append({
            "tier": t,
            "minLevel": (t - 1) * 10 + 1,
            "minValue": float(f"{min_v:.2f}"),
            "maxValue": float(f"{max_v:.2f}")
        })
        
    definitions.append({
        "id": name_id,
        "type": AffixType[type_name],
        "nameTemplate": name_template,
        "isPrefix": is_prefix,
        "tiers": tiers,
        "allowedTags": tags
    })

# Suffixes
add_def("strength", "Strength", "Strength", False, 3.0, 0, 2.0, ["armor", "weapon", "jewelry"])
add_def("dexterity", "Dexterity", "Dexterity", False, 3.0, 0, 2.0, ["armor", "weapon", "jewelry"])
add_def("intelligence", "Intelligence", "Intelligence", False, 3.0, 0, 2.0, ["armor", "weapon", "jewelry"])
add_def("vitality", "Vitality", "Vitality", False, 3.0, 0, 2.0, ["armor", "weapon", "jewelry"])

add_def("crit_chance", "CritChance", "Deadly", False, 0, 0, 0, ["weapon", "gloves", "jewelry"])
add_def("attack_speed", "AttackSpeed", "Rapid", False, 0, 0, 0, ["weapon", "gloves"])
add_def("move_speed", "MoveSpeed", "Quick", False, 0, 0, 0, ["boots"])

add_def("resist_all", "ResistAll", "Prismatic", False, 5.0, 0, 3.0, ["armor", "jewelry"])
add_def("resist_fire", "ResistFire", "Fire Res", False, 5.0, 0, 3.0, ["armor", "jewelry"])
add_def("resist_cold", "ResistCold", "Cold Res", False, 5.0, 0, 3.0, ["armor", "jewelry"])
add_def("resist_lightning", "ResistLightning", "Light Res", False, 5.0, 0, 3.0, ["armor", "jewelry"])

# Prefixes
add_def("flat_health", "FlatHealth", "Vigorous", True, 10.0, 0, 5.0, ["armor", "jewelry"])
add_def("flat_mana", "FlatMana", "Mystic", True, 8.0, 0, 4.0, ["armor", "jewelry"])

add_def("flat_phys", "FlatPhysicalDamage", "Sharp", True, 2.0, 0, 2.0, ["weapon", "jewelry"])
add_def("flat_fire", "FlatFireDamage", "Burning", True, 2.0, 0, 2.0, ["weapon", "jewelry"])

add_def("pct_phys", "PercentPhysicalDamage", "Cruel", True, 5.0, 0, 3.0, ["weapon"])
add_def("pct_fire", "PercentFireDamage", "Fiery", True, 5.0, 0, 3.0, ["weapon"])
add_def("pct_light", "PercentLightningDamage", "Shocking", True, 5.0, 0, 3.0, ["weapon"])

add_def("flat_armor", "FlatArmor", "Reinforced", True, 10.0, 0, 5.0, ["armor"])
add_def("pct_armor", "PercentArmor", "Hardened", True, 10.0, 0, 5.0, ["armor"])

# Output
with open("assets/data/affixes.json", "w") as f:
    json.dump(definitions, f, indent=4)

print(f"Generated {len(definitions)} affix definitions.")
