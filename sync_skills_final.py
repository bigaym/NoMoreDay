
import json
import re
import os

def clean_name(name):
    # 移除括号及其内容，并去除首尾空格
    return re.sub(r'\(.*?\)', '', name).strip()

def sync_skills():
    md_path = r'f:/NoMoreDay/设计文档/职业设计草案_剑修.md'
    json_path = r'f:/NoMoreDay/assets/data/skills.json'

    if not os.path.exists(md_path) or not os.path.exists(json_path):
        print("Files not found.")
        return

    # 1. 从 MD 中解析所有技能及其节点
    with open(md_path, 'r', encoding='utf-8') as f:
        md_content = f.read()

    skills_md = {}
    # 分割技能章节
    sections = re.split(r'\n### \d+\.\d+ ', md_content)[1:]
    
    for s in sections:
        header = s.split('\n')[0]
        skill_name = clean_name(header)
        
        # 寻找形如 - **[节点名]** 或 - <类型> **[节点名]** 的行
        nodes = []
        lines = s.split('\n')
        for line in lines:
            if line.strip().startswith('-'):
                # 匹配 ** 内部的 [ ] 内容
                matches = re.findall(r'\*\*.*?\[([^\]]+)\].*?\*\*', line)
                for m in matches:
                    name = clean_name(m)
                    if name and name not in ['关键节点清单', '基础核心'] and not name.startswith('分支'):
                        # 还要检查描述
                        desc = ""
                        if ':' in line:
                            desc = line.split(':', 1)[1].strip()
                        nodes.append({'name': name, 'desc': desc})
        
        skills_md[skill_name] = nodes

    # 2. 读取 JSON 并同步
    with open(json_path, 'r', encoding='utf-8') as f:
        json_data = json.load(f)

    # 技能 ID 映射（仅针对剑修相关技能）
    skill_mapping = {
        '流云刺': 1,
        '裂空斩': 2,
        '灵剑决': 3,
        '剑气护体': 4,
        '万剑归宗': 5,
        '剑阵·诛仙': 6,
        '心剑·无影': 7,
        '御剑·回旋': 8,
        '绝影绝剑': 9
    }

    modified = False
    for skill in json_data['skills']:
        name = skill['name_key']
        if name in skills_md:
            md_nodes = skills_md[name]
            talent_tree = skill.get('talent_tree', [])
            
            existing_nodes = {n['name_key']: n for n in talent_tree}
            new_talent_tree = []
            
            # 基础 ID 范围（根据技能 ID 分配，如 100, 200...）
            base_id = skill['id'] * 100
            
            # 同步节点
            counter = 0
            for md_node in md_nodes:
                node_name = md_node['name']
                if node_name in existing_nodes:
                    # 更新描述
                    node = existing_nodes[node_name]
                    if md_node['desc']:
                        node['desc_key'] = md_node['desc']
                    new_talent_tree.append(node)
                else:
                    # 新增节点（尽量寻找未使用的 ID）
                    # 这里简化处理：如果在原树里没找到，分配一个新的 ID
                    new_id = base_id + 50 + counter # 临时分配一个偏移量，避免冲突
                    while any(n['id'] == new_id for n in talent_tree):
                        new_id += 1
                    
                    new_node = {
                        "id": new_id,
                        "name_key": node_name,
                        "desc_key": md_node['desc'],
                        "max_points": 5,
                        "x": 0.0,
                        "y": 0.0,
                        "prerequisites": [],
                        "stat_modifiers": []
                    }
                    new_talent_tree.append(new_node)
                    counter += 1
            
            skill['talent_tree'] = new_talent_tree
            modified = True
            print(f"Synced {name}: {len(new_talent_tree)} nodes.")

    if modified:
        with open(json_path, 'w', encoding='utf-8') as f:
            json.dump(json_data, f, ensure_ascii=False, indent=2)
        print("Successfully updated skills.json")

if __name__ == "__main__":
    sync_skills()
