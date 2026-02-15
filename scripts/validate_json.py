import json
import os
import sys

def validate_json_files(data_dir):
    failed = False
    print(f"[JSON Validator] Scanning directory: {data_dir}")
    
    for root, dirs, files in os.walk(data_dir):
        for file in files:
            if file.endswith('.json'):
                file_path = os.path.join(root, file)
                try:
                    with open(file_path, 'r', encoding='utf-8-sig') as f:
                        json.load(f)
                except json.JSONDecodeError as e:
                    print(f"FAILED: {file_path}")
                    print(f"  Error: {e}")
                    failed = True
                except Exception as e:
                    print(f"ERROR reading {file_path}: {e}")
                    failed = True

    return not failed

if __name__ == "__main__":
    target_dir = "assets/data"
    if len(sys.argv) > 1:
        target_dir = sys.argv[1]
        
    if validate_json_files(target_dir):
        print("[JSON Validator] All JSON files are valid.")
        sys.exit(0)
    else:
        print("[JSON Validator] Some JSON files failed validation.")
        sys.exit(1)
