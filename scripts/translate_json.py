import json
import os
import sys

# Encoding set to utf-8
sys.stdout.reconfigure(encoding='utf-8')

file_path = 'assets/data/profession_talents.json'
absolute_path = os.path.join(os.getcwd(), file_path)

if not os.path.exists(absolute_path):
    print(f"File not found: {absolute_path}")
    sys.exit(1)

with open(absolute_path, 'r', encoding='utf-8') as f:
    data = json.load(f)

# Translation mappings
professions = {
    "Blade Ascendant": ("剑修", "御剑而行，以气化形"),
    "Mage": ("秘术师", "掌控元素，毁灭万物"),
    "Priest": ("神官", "神圣治愈，净化心灵"),
    "Knight": ("骑士", "重甲守护，坚不可摧"),
    "Ranger": ("游侠", "百步穿杨，陷阱伏击"),
    "Berserker": ("狂战士", "怒气爆发，愈战愈勇")
}

node_types = {
    "Minor": "基础训练",
    "Major": "进阶训练",
    "Core": "核心觉醒"
}

desc_map = {
    "Minor stat boost": "少量属性提升",
    "Major stat boost": "大幅属性提升",
    "Unlock core mechanics": "解锁核心机制",
    "Sword Cultivator": "御剑而行，以气化形",
    "Elemental Master": "掌控元素，毁灭万物",
    "Healer": "神圣治愈，净化心灵",
    "Defender": "重甲守护，坚不可摧",
    "Sharpshooter": "百步穿杨，陷阱伏击",
    "Rage Warrior": "怒气爆发，愈战愈勇"
}

# Translate profession stars
for star in data['profession_stars']:
    eng_name = star['name_key']
    if eng_name in professions:
        star['name_key'] = professions[eng_name][0]
        star['desc_key'] = professions[eng_name][1]

# Translate nodes
for node in data['nodes']:
    # Handle name_key
    name = node['name_key']
    
    # Extract parts: "Blade Ascendant Minor 1" -> ["Blade Ascendant", "Minor", "1"]
    # But names vary.
    # Pattern: [Profession Name] [Type] [Number/Suffix]
    
    matched_prof = None
    for prof_eng in professions.keys():
        if name.startswith(prof_eng):
            matched_prof = prof_eng
            break
            
    if matched_prof:
        suffix = name[len(matched_prof):].strip() # "Minor 1"
        
        # Determine type from suffix if contained
        node_type_eng = None
        if "Minor" in suffix:
            node_type_eng = "Minor"
        elif "Major" in suffix:
            node_type_eng = "Major"
        elif "Core" in suffix:
            node_type_eng = "Core"
            
        if node_type_eng:
            # Reconstruct name
            prof_cn = professions[matched_prof][0]
            type_cn = node_types[node_type_eng]
            
            rest = suffix.replace(node_type_eng, "").strip() # "1" or ""
            
            # Roman numerals for good measure if it's a number
            if rest.isdigit():
                val = int(rest)
                roman = ["", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"]
                if val <= 10:
                    rest = roman[val]
            
            new_name = f"{prof_cn} {type_cn}"
            if rest:
                new_name += f" {rest}"
                
            node['name_key'] = new_name
            
    # Handle desc_key
    desc = node['desc_key']
    if desc in desc_map:
        node['desc_key'] = desc_map[desc]

with open(absolute_path, 'w', encoding='utf-8') as f:
    json.dump(data, f, ensure_ascii=False, indent=2)

print("Translation complete.")
