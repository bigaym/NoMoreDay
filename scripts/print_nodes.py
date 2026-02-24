import json
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--skill-id', type=int)
args = parser.parse_args()

with open('f:/NoMoreDay/assets/data/skills.json', 'r', encoding='utf-8') as f:
    data = json.load(f)

for s in data.get('skills', []):
    if args.skill_id is None or s.get('id') == args.skill_id:
        print(f"=============================")
        print(f"Skill {s['id']}: {s.get('name_key')}")
        print(f"=============================")
        for n in s.get('talent_tree', []):
            print(f"Node {n['id']}: {n.get('name_key')} - {n.get('desc_key')}")
