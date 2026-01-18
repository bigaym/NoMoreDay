"""
NoMoreDay Affix Data Generator
Purpose: Generates 'affixes.json' containing definitions for all item modifiers.
         Includes math for 7 tiers of power scaling.
Usage: python scripts/gen_affix_data.py
"""
import json
import os

# Enum mapping (Must match AffixType in C++ exactly)
# Manual sync with ItemStats.hpp enum order
AffixType = {
    "Strength": 0, "Dexterity": 1, "Intelligence": 2, "Vitality": 3,
    "AllAttributes": 4,
    
    "FlatPhysicalDamage": 5, "FlatFireDamage": 6, "FlatColdDamage": 7, "FlatLightningDamage": 8, "FlatPoisonDamage": 9, "FlatShadowDamage": 10,
    "PercentPhysicalDamage": 11, "PercentFireDamage": 12, "PercentColdDamage": 13, "PercentLightningDamage": 14, "PercentPoisonDamage": 15, "PercentShadowDamage": 16,
    
    "CritChance": 17, "CritDamage": 18, "AttackSpeed": 19, "CastSpeed": 20, "Accuracy": 21,
    
    "FlatArmor": 22, "PercentArmor": 23, "FlatHealth": 24, "PercentHealth": 25, "FlatMana": 26,
    "ResistAll": 27, "ResistFire": 28, "ResistCold": 29, "ResistLightning": 30, "ResistPoison": 31, "ResistShadow": 32,
    "Thorns": 33, "DamageReduction": 34,
    
    "HealthRegen": 35, "ManaRegen": 36, "PercentHealthRegen": 37, "PercentManaRegen": 38,
    
    "MoveSpeed": 39, "CooldownReduction": 40, "LifeSteal": 41, "LifeOnHit": 42
}

definitions = []

def add_def(name_id, type_name, name_template, is_prefix, base_val, scale_val, variance, tags):
    tiers = []
    for t in range(1, 8):
        min_v = 0
        max_v = 0
        
        if name_id == "move_speed":
            min_v = 5.0 + (t * 2.0)
            max_v = min_v
        elif name_id == "crit_chance":
            min_v = 1.0 + (t * 0.8)
            max_v = min_v
        elif name_id == "attack_speed":
            min_v = 5.0 + (t * 1.5)
            max_v = min_v
        elif name_id == "all_stats":
            min_v = 2.0 * t
            max_v = min_v + 1.0
        elif name_id in ["health_regen", "mana_regen"]:
            min_v = 0.5 * t
            max_v = min_v + 0.5
        elif name_id in ["pct_health_regen", "pct_mana_regen"]:
            min_v = 10.0 * t
            max_v = min_v + 5.0
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
add_def("all_stats", "AllAttributes", "Balanced", False, 0, 0, 0, ["armor", "jewelry"])

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

add_def("health_regen", "HealthRegen", "Regenerating", True, 0, 0, 0, ["armor", "jewelry"])
add_def("mana_regen", "ManaRegen", "Flowing", True, 0, 0, 0, ["armor", "jewelry"])
add_def("pct_health_regen", "PercentHealthRegen", "Vitalizing", True, 0, 0, 0, ["armor", "jewelry"])
add_def("pct_mana_regen", "PercentManaRegen", "Spiritual", True, 0, 0, 0, ["armor", "jewelry"])

# Output
with open("assets/data/affixes.json", "w") as f:
    json.dump(definitions, f, indent=4)

print(f"Generated {len(definitions)} affix definitions.")
