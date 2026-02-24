
import json
import re

def clean_name(name):
    # Remove everything in parentheses and strip
    return re.sub(r'\(.*?\)', '', name).strip()

def main():
    md_path = r'f:/NoMoreDay/设计文档/职业设计草案_剑修.md'
    json_path = r'f:/NoMoreDay/assets/data/skills.json'

    with open(md_path, 'r', encoding='utf-8') as f:
        content = f.read()

    skills_md = {}
    sections = re.split(r'\n### \d+\.\d+ ', content)[1:]
    for s in sections:
        header = s.split('\n')[0]
        skill_name = clean_name(header)
        
        lines = s.split('\n')
        nodes = set()
        for line in lines:
            line = line.strip()
            # Only look for nodes in bullet points
            if line.startswith('-'):
                # This regex captures the name inside [ ] which is inside bold markers ** **
                # It accounts for optional tags like <Keystone> inside the bold markers
                matches = re.findall(r'\*\*.*?\[([^\]]+)\].*?\*\*', line)
                for m in matches:
                    name = clean_name(m)
                    # Exclude non-node terms
                    if name and name not in ['关键节点清单', '基础核心'] and not name.startswith('分支'):
                        nodes.add(name)
        skills_md[skill_name] = nodes

    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    for skill in data['skills']:
        name = skill['name_key']
        if name in skills_md:
            json_nodes = set(n['name_key'] for n in skill.get('talent_tree', []))
            md_nodes = skills_md[name]
            
            missing = sorted(list(md_nodes - json_nodes))
            extra = sorted(list(json_nodes - md_nodes))
            
            print(f'Skill: {name}')
            if not missing and not extra:
                print('  [OK] FULLY ALIGNED')
            else:
                if missing:
                    print(f'  [MISSING IN JSON]: {missing}')
                if extra:
                    print(f'  [EXTRA IN JSON]: {extra}')
            print("-" * 30)

if __name__ == "__main__":
    main()
