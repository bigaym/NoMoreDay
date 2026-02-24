import json
import os
import subprocess
import re
import argparse

def sanitize_filename(name):
    """Sanitize string for Windows filename."""
    return re.sub(r'[\\/*?:"<>|]', "", name)

def get_custom_prompts(project_root):
    prompts_file = os.path.join(project_root, "assets", "data", "skill_node_prompts.json")
    if os.path.exists(prompts_file):
        with open(prompts_file, "r", encoding="utf-8") as f:
            return json.load(f)
    return {}

def generate_icons(target_skill_id=None, target_node_id=None):
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    json_path = os.path.join(project_root, "assets", "data", "skills.json")
    output_dir = os.path.join(project_root, "assets", "textures", "skill_nodes")
    asset_gen_script = os.path.join(project_root, "scripts", "asset_gen.py")
    
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    custom_prompts = get_custom_prompts(project_root)

    for skill in data.get("skills", []):
        skill_id = skill.get("id")
        
        if target_skill_id is not None and skill_id != target_skill_id:
            continue
            
        talent_tree = skill.get("talent_tree", [])
        for node in talent_tree:
            node_id = node.get("id")
            
            if target_node_id is not None and node_id != target_node_id:
                continue
                
            name_key = node.get("name_key", "")
            
            if not name_key:
                continue
            
            safe_name = sanitize_filename(name_key)
            final_name = f"skill_nodes_{node_id}_{safe_name}.png"
            final_path = os.path.join(output_dir, final_name)
            
            if os.path.exists(final_path):
                print(f"Skipping {final_name} (already exists)")
                continue

            node_id_str = str(node_id)
            if node_id_str in custom_prompts:
                prompt = custom_prompts[node_id_str]
            else:
                print(f"Warning: No custom prompt found for node {node_id} ({name_key}). Skipping.")
                continue
                
            expected_prefix = f"tmp_{node_id}"
            
            cmd = [
                "python", asset_gen_script,
                "--prompt", prompt,
                "--name", expected_prefix,
                "--output-dir", output_dir,
                "--width", "512",
                "--height", "512",
                "--target-width", "128",
                "--target-height", "128",
                "--negative", ""
            ]
            
            print(f"Generating for node {node_id}: {name_key}...")
            print(f"Prompt: {prompt}")
            try:
                subprocess.run(cmd, check=True)
            except subprocess.CalledProcessError as e:
                print(f"Error generating {final_name}: {e}")
                continue
            
            # Find and rename the output file
            renamed = False
            for file in os.listdir(output_dir):
                if file.startswith(expected_prefix) and file.endswith(".png"):
                    old_path = os.path.join(output_dir, file)
                    if os.path.exists(final_path):
                        os.remove(final_path)
                    os.rename(old_path, final_path)
                    print(f"Successfully generated and saved: {final_name}")
                    renamed = True
                    break
                    
            if not renamed:
                print(f"Warning: Output file for {final_name} not found!")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Batch Generator for Skill Icons")
    parser.add_argument("--skill-id", type=int, help="Generate icons for all nodes in a specific skill ID")
    parser.add_argument("--node-id", type=int, help="Generate an icon for a specific node ID")
    
    args = parser.parse_args()
    
    generate_icons(target_skill_id=args.skill_id, target_node_id=args.node_id)
