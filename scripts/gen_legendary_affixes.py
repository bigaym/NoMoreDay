"""
NoMoreDay Legendary Affix Generator
Purpose: Parses 'legendary_affixes.md' and converts it to 'legendary_affixes.json'.
         Maps design document table entries to game-ready data structures.
Usage: python scripts/gen_legendary_affixes.py
"""
import json
import re

def parse_markdown_table(content):
    tables = []
    current_table = []
    in_table = False
    
    for line in content.split('\n'):
        line = line.strip()
        if not line.startswith('|'):
            if in_table:
                tables.append(current_table)
                current_table = []
                in_table = False
            continue
            
        # Skip separator lines like |---| or |:---|
        if re.match(r'^\|[\s:-|]+\|$', line):
            continue
            
        in_table = True
        cols = [c.strip() for c in line.split('|')[1:-1]]
        if cols:
            current_table.append(cols)
            
    if current_table:
        tables.append(current_table)
    return tables

def generate_legendary_affixes():
    with open('设计文档/legendary_affixes.md', 'r', encoding='utf-8') as f:
        content = f.read()
    
    tables = parse_markdown_table(content)
    # Filter out header rows (contains "编号" or "英文ID")
    data_tables = []
    for t in tables:
        rows = [r for r in t if len(r) > 0 and r[0] != '编号' and r[0] != 'No' and not r[0].startswith(':--')] 
        if rows:
            data_tables.append(rows)
        
    affixes = []
    
    # Base mapping for tags from doc to game tags
    tag_map = {
        "手套": "gloves",
        "武器": "weapon",
        "胸甲": "chest",
        "肩甲": "shoulder",
        "头盔": "head",
        "鞋子": "boots",
        "副手": "offhand",
        "饰品": "jewelry",
        "腿甲": "legs",
        "戒指": "ring",
        "护身符": "amulet",
        "腰带": "belt",
        "头部": "head"
    }

    for table in data_tables:
        for row in table:
            if len(row) < 5: continue
            
            try:
                raw_id = int(row[0])
            except ValueError:
                continue
                
            english_id = row[1].strip('`')
            chinese_name = row[2].strip('*')
            description = row[3]
            slots = row[4].split('/')
            
            # Map slots to tags
            allowed_tags = []
            for s in slots:
                s = s.strip()
                if s in tag_map:
                    allowed_tags.append(tag_map[s])
                else:
                    # Fallback or generic
                    allowed_tags.append("armor")
            
            # Deduplicate tags
            allowed_tags = list(set(allowed_tags))
            
            # ID offset
            storage_id = 1000 + raw_id
            
            # Heuristic for isPrefix
            is_prefix = True
            if any(k in chinese_name for k in ["之", "回响", "者"]):
                is_prefix = False
                
            aff = {
                "id": english_id,
                "type": storage_id,
                "nameTemplate": chinese_name,
                "isPrefix": is_prefix,
                "tiers": [
                    {
                        "tier": 7,
                        "minLevel": 1,
                        "minValue": 1.0, 
                        "maxValue": 1.0
                    }
                ],
                "allowedTags": allowed_tags
            }
            
            if raw_id >= 101:
                aff["requiredSkillTags"] = ["blade_ascendant"]
                
            affixes.append(aff)
            
    with open('assets/data/legendary_affixes.json', 'w', encoding='utf-8') as f:
        json.dump(affixes, f, ensure_ascii=False, indent=4)
    
    print(f"Generated {len(affixes)} legendary affixes.")

if __name__ == "__main__":
    generate_legendary_affixes()