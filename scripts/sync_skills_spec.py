"""
NoMoreDay Skill Specification Synchronizer
Purpose: Parses the 'Blade Ascendant' Markdown design doc and synchronizes
         the skill tree structure into 'skills.json'.
Usage: python scripts/sync_skills_spec.py
"""
import json
import re
import os
import sys

# Configuration
DESIGN_DOC_PATH = r"设计文档/职业设计草案_剑修.md"
SKILLS_JSON_PATH = r"assets/data/skills.json"

# Skill Name to ID Mapping
SKILL_MAP = {
    "流云刺": 1,
    "裂空斩": 2,
    "灵剑决": 3,
    "剑气护体": 4,
    "万剑归宗": 5,
    "剑阵·诛仙": 6,
    "心剑·无影": 7,
    "御剑·回旋": 8,
    "绝影闪": 9
}

# Branch ID offsets
BRANCH_OFFSETS = {
    "Base": 0,
    "A": 10,
    "B": 30,  # A has 20 slots (10-29)
    "C": 50,  # B has 20 slots (30-49)
    "D": 70   # C has 20 slots (50-69)
}

def parse_design_doc(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    skills_data = {}
    current_skill_name = None
    current_branch = None
    
    # Split by lines
    lines = content.split('\n')
    
    for line in lines:
        line = line.strip()
        
        # Match Skill Header: ### 3.1 流云刺 (Flowing Thrust)
        skill_match = re.match(r'^### \d+\.\d+\s+(.*?)\s+\(.*?\)', line)
        if skill_match:
            full_name = skill_match.group(1).strip()
            # Extract Chinese name if possible (assuming it's the first part or known map)
            # Simplification: check if any key in SKILL_MAP is in the name
            for key in SKILL_MAP:
                if key in full_name:
                    current_skill_name = key
                    skills_data[current_skill_name] = []
                    break
            continue

        if not current_skill_name:
            continue

        # Match Branch Header: **[基础核心 (Base Tier)]** or **[分支 A：... (Branch A)]**
        # Adapting to format in doc: **[基础核心 (Base Tier)]**, **[分支 A：极速流 (Velocity & Reset)]**
        branch_match = re.match(r'^\*\*\[(?:基础核心|分支 ([A-D]))', line)
        if branch_match:
            branch_group = branch_match.group(1)
            if branch_group:
                current_branch = branch_group
            else:
                current_branch = "Base"
            continue
            
        # Match Node: - **[迅捷之刃 (Swift Blade)] (0/5)**: ...
        # Or: - **<Keystone> [风行者 (Windwalker)] (0/1)**: ...
        # Regex explanation:
        # ^- \*\*             : Start with bullet and bold
        # (?:<(\w+)> )?       : Optional Type tag <Keystone/Transmuter/etc>
        # \[([^\]]+)\]        : Node Name [Name (English)]
        # \s*                 : spaces
        # \((\d+)/(\d+)\)\*\* : Points (0/5)**
        # :?                  : Optional colon
        # \s*                 : spaces
        # (?:(?:\*\{需求: (.*?)\}\*)?)\s* : Optional Prerequisite *{需求: ...}*
        # (.*)                : Description
        
        node_match = re.match(r'^- \*\*(?:<(\w+)> )?\[([^\]]+)\]\s*\((\d+)/(\d+)\)\*\*:?\s*(?:(?:\*\{需求:\s*(.*?)\}\*)?\s*)?(.*)', line)
        
        if node_match:
            node_type = node_match.group(1) # Keystone, etc
            full_node_name = node_match.group(2) # 迅捷之刃 (Swift Blade)
            points_min = int(node_match.group(3))
            max_points = int(node_match.group(4))
            req_str = node_match.group(5) # 迅捷之刃 1/5
            desc = node_match.group(6)
            
            # Parse Name and Key
            # Format: Chinese (English)
            name_parts = re.match(r'(.*?)\s*\((.*?)\)', full_node_name)
            if name_parts:
                name_cn = name_parts.group(1)
                name_en = name_parts.group(2) # simplified logic, use as key for now or just name
            else:
                name_cn = full_node_name
                name_en = ""

            node_data = {
                "name": name_cn,
                "name_en": name_en,
                "max_points": max_points,
                "description": desc,
                "branch": current_branch,
                "req_str": req_str,
                "type": node_type,
                "raw_line": line
            }
            skills_data[current_skill_name].append(node_data)

    return skills_data

def process_nodes(skill_id, raw_nodes):
    processed_nodes = []
    
    # Track node indices per branch to assign stable IDs
    # IDs: SkillID * 100 + BranchOffset + Index
    branch_counters = {
        "Base": 0,
        "A": 0,
        "B": 0,
        "C": 0,
        "D": 0
    }
    
    # Store name -> id mapping for prerequisite resolution
    name_to_id = {}
    
    for node in raw_nodes:
        branch = node['branch']
        idx = branch_counters[branch]
        branch_counters[branch] += 1
        
        node_id = skill_id * 100 + BRANCH_OFFSETS[branch] + idx
        name_to_id[node['name']] = node_id
        
        # Calculate Layout X/Y (Simple autolayout based on branch and index)
        # Base: straight line down? Or center.
        # Branches: A(UL), B(UR), C(LL), D(LR) or varied.
        # This is a placeholder layout logic.
        x, y = 0.0, 0.0
        if branch == "Base":
            y = idx * 1.5
        elif branch == "A":
            x = -1.5 - (idx % 2) * 1.5
            y = -1.5 - (idx // 2) * 1.5
        elif branch == "B":
            x = 1.5 + (idx % 2) * 1.5
            y = -1.5 - (idx // 2) * 1.5
        elif branch == "C":
            x = -1.5 - (idx % 2) * 1.5
            y = 1.5 + (idx // 2) * 1.5
        elif branch == "D":
            x = 1.5 + (idx % 2) * 1.5
            y = 1.5 + (idx // 2) * 1.5

        new_node = {
            "id": node_id,
            "name_key": node['name'],
            "desc_key": node['description'],
            "max_points": node['max_points'],
            "x": x,
            "y": y,
            "prerequisites": [],
            "_req_str": node['req_str'], # Temp for post-processing
            "_type": node['type']
        }
        
        if node['description']:
             # Simple checking for stat modifiers in description could go here
             # For now, we rely on manual tuning or advanced parsing later.
             pass

        processed_nodes.append(new_node)
        
    # Resolve Prerequisites
    for node in processed_nodes:
        req_str = node.pop('_req_str')
        node_type = node.pop('_type')
        
        # Handle Node Type Behavior Injection (Placeholder mapping)
        if node_type:
            # e.g., <Keystone> -> maybe some special hidden stat or just visual
            # This logic needs to be expanded if we want to map type to specific logic
            pass
            
        if req_str:
            # e.g., "迅捷之刃 3/5" or "连环" or "连环 3/3"
            # Extract just the name part
            # Regex: match name before space-digit or end of string
            # Be careful with names containing spaces? Chinese names usually don't.
            
            # Split by space to check for "Name Level/Max"
            parts = req_str.split(' ')
            req_name = parts[0]
            
            # Special case: "任意 Base 3点" (Any Base 3 points)
            if "任意" in req_name and "Base" in req_str:
                # Cannot handle complex logic in simple JSON yet, skip or add dummy
                print(f"Skipping complex requirement: {req_str}")
                continue
                
            if req_name in name_to_id:
                node['prerequisites'].append(name_to_id[req_name])
            else:
                print(f"Warning: Prerequisite '{req_name}' not found for node '{node['name_key']}'")

    return processed_nodes

def sync_skills():
    print(f"Parsing design doc: {DESIGN_DOC_PATH}")
    design_data = parse_design_doc(DESIGN_DOC_PATH)
    
    print(f"Loading skills JSON: {SKILLS_JSON_PATH}")
    if not os.path.exists(SKILLS_JSON_PATH):
        print(f"Error: {SKILLS_JSON_PATH} not found.")
        return

    with open(SKILLS_JSON_PATH, 'r', encoding='utf-8') as f:
        json_data = json.load(f)
        
    for skill_entry in json_data['skills']:
        skill_id = skill_entry['id']
        skill_name_key = skill_entry['name_key']
        
        # Check if we have design data for this skill
        target_design_name = None
        for name, sid in SKILL_MAP.items():
            if sid == skill_id:
                target_design_name = name
                break
        
        if target_design_name and target_design_name in design_data:
            print(f"Syncing skill: {target_design_name} (ID: {skill_id})")
            raw_nodes = design_data[target_design_name]
            new_nodes = process_nodes(skill_id, raw_nodes)
            
            # Preserve existing stat_modifiers if possible? 
            # Currently the plan says "largely overwrite".
            # We will overwrite 'talent_tree' completely for these skills.
            
            # OPTIONAL: Try to preserve icon_id if node names match strictly?
            # Existing nodes might have manual work.
            if 'talent_tree' in skill_entry:
                old_nodes_map = {n['name_key']: n for n in skill_entry['talent_tree']}
                for new_node in new_nodes:
                    if new_node['name_key'] in old_nodes_map:
                        old_node = old_nodes_map[new_node['name_key']]
                        if 'icon_id' in old_node:
                            new_node['icon_id'] = old_node['icon_id']
                        if 'stat_modifiers' in old_node:
                            new_node['stat_modifiers'] = old_node['stat_modifiers']
                        if 'damage_modifiers' in old_node:
                            new_node['damage_modifiers'] = old_node['damage_modifiers']
            
            skill_entry['talent_tree'] = new_nodes
            
    # Write back
    with open(SKILLS_JSON_PATH, 'w', encoding='utf-8') as f:
        json.dump(json_data, f, indent=2, ensure_ascii=False)
    print("Sync complete.")

if __name__ == "__main__":
    sync_skills()
