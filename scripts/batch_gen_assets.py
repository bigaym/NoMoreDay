"""
NoMoreDay Batch Asset Generator
Purpose: Parses 'Asset_Regeneration_List.md' and generates assets in bulk.
         Supports both small (icons) and large (UI panels, environment) assets with dynamic sizing.
Usage: conda run -n ai python scripts/batch_gen_assets.py
"""
import websocket # type: ignore
import uuid
import json
import urllib.request
import urllib.parse
import random
import sys
import os
import argparse
import requests # type: ignore
import subprocess
import time
import re
from PIL import Image
import io
import rembg

# --- Configuration ---
SERVER_ADDRESS = "127.0.0.1:8188"
CLIENT_ID = str(uuid.uuid4())
COMFYUI_PATH = r"D:\\Program Files\\ComfyUI_windows_portable\\run_nvidia_gpu.bat"
MD_FILE_PATH = r"设计文档/特效和UI/Asset_Regeneration_List.md"
PROJECT_ROOT = os.getcwd()

# Force no proxy for localhost
os.environ["NO_PROXY"] = "127.0.0.1,localhost"

# --- ComfyUI Helper Functions ---

def is_server_running():
    try:
        requests.get(f"http://{SERVER_ADDRESS}", timeout=1)
        return True
    except requests.RequestException:
        return False

def start_comfyui():
    print(f"ComfyUI not found at {SERVER_ADDRESS}. Starting it now...")
    if not os.path.exists(COMFYUI_PATH):
        print(f"Error: Could not find ComfyUI start script at: {COMFYUI_PATH}")
        sys.exit(1)
    
    comfyui_dir = os.path.dirname(COMFYUI_PATH)
    cmd = f'start "ComfyUI" /D "{comfyui_dir}" "{COMFYUI_PATH}" --lowvram'
    subprocess.Popen(cmd, shell=True)
    
    print("Waiting for ComfyUI to initialize...")
    for i in range(30):
        if is_server_running():
            print("ComfyUI is Online!")
            return
        time.sleep(1)
        print(".", end="", flush=True)
    
    print("\nError: Timed out waiting for ComfyUI to start.")
    sys.exit(1)

def get_default_workflow():
    return {
        "10": {
            "inputs": {
                "unet_name": "flux-2-klein-9b.safetensors",
                "weight_dtype": "fp8_e4m3fn"
            },
            "class_type": "UNETLoader"
        },
        "11": {
            "inputs": {
                "clip_name": "qwen_3_8b_fp8mixed.safetensors",
                "type": "flux2"
            },
            "class_type": "CLIPLoader"
        },
        "12": {
            "inputs": {
                "vae_name": "flux2-vae.safetensors"
            },
            "class_type": "VAELoader"
        },
        "13": {
            "inputs": {
                "max_shift": 1.15,
                "base_shift": 0.5,
                "width": 1024,
                "height": 1024,
                "model": ["10", 0]
            },
            "class_type": "ModelSamplingFlux"
        },
        "5": {
            "inputs": {
                "width": 1024,
                "height": 1024,
                "batch_size": 1
            },
            "class_type": "EmptyLatentImage"
        },
        "6": {
            "inputs": {
                "text": "", # To be filled
                "clip": ["11", 0]
            },
            "class_type": "CLIPTextEncode"
        },
        "7": {
            "inputs": {
                "text": "blur, noise, photo, realistic, 3d, text, watermark, background",
                "clip": ["11", 0]
            },
            "class_type": "CLIPTextEncode"
        },
        "3": {
            "inputs": {
                "seed": random.randint(1, 1000000000),
                "steps": 15,
                "cfg": 1.0,
                "sampler_name": "euler",
                "scheduler": "simple",
                "denoise": 1,
                "model": ["13", 0],
                "positive": ["6", 0],
                "negative": ["7", 0],
                "latent_image": ["5", 0]
            },
            "class_type": "KSampler"
        },
        "8": {
            "inputs": {
                "samples": ["3", 0],
                "vae": ["12", 0]
            },
            "class_type": "VAEDecode"
        },
        "9": {
            "inputs": {
                "images": ["8", 0],
                "filename_prefix": "NoMoreDay_Batch"
            },
            "class_type": "SaveImage"
        }
    }

def queue_prompt(prompt_workflow):
    p = {"prompt": prompt_workflow, "client_id": CLIENT_ID}
    data = json.dumps(p).encode('utf-8')
    req = urllib.request.Request(f"http://{SERVER_ADDRESS}/prompt", data=data)
    return json.loads(urllib.request.urlopen(req).read())

def get_image(filename, subfolder, folder_type):
    data = {"filename": filename, "subfolder": subfolder, "type": folder_type}
    url_values = urllib.parse.urlencode(data)
    with urllib.request.urlopen(f"http://{SERVER_ADDRESS}/view?{url_values}") as response:
        return response.read()

def get_history(prompt_id):
    with urllib.request.urlopen(f"http://{SERVER_ADDRESS}/history/{prompt_id}") as response:
        return json.loads(response.read())

def generate_image(ws, prompt_text, negative_prompt, output_path, filename, target_size=128, remove_bg=True):
    workflow = get_default_workflow()
    
    # Configure Workflow
    gen_width = 1024
    gen_height = 1024
    
    workflow["6"]["inputs"]["text"] = f"{prompt_text}, white background, isolated, 2d game sprite"
    workflow["7"]["inputs"]["text"] = negative_prompt
    workflow["5"]["inputs"]["width"] = gen_width
    workflow["5"]["inputs"]["height"] = gen_height
    workflow["3"]["inputs"]["seed"] = random.randint(1, 1000000000)

    prompt_id = queue_prompt(workflow)['prompt_id']
    print(f" -> Generating: {filename} ({target_size}px) (ID: {prompt_id})")

    while True:
        out = ws.recv()
        if isinstance(out, str):
            message = json.loads(out)
            if message['type'] == 'executing':
                data = message['data']
                if data['node'] is None and data['prompt_id'] == prompt_id:
                    break
        else:
            continue

    history = get_history(prompt_id)[prompt_id]
    for node_id in history['outputs']:
        node_output = history['outputs'][node_id]
        if 'images' in node_output:
            for image in node_output['images']:
                img_data = get_image(image['filename'], image['subfolder'], image['type'])
                
                img = Image.open(io.BytesIO(img_data))
                
                if remove_bg:
                    img = rembg.remove(img)
                
                # Resize
                img = img.resize((target_size, target_size), Image.Resampling.LANCZOS)
                
                full_path = os.path.join(output_path, filename)
                img.save(full_path, "PNG")
                print(f" -> Saved to {full_path}")
                return

# --- Logic ---

def get_asset_config(filename):
    filename = filename.lower()
    
    config = {
        "size": 128,
        "remove_bg": True
    }
    
    # UI Panels & Backgrounds
    if any(k in filename for k in ['panel', 'bg', 'background']):
        config["size"] = 512
        config["remove_bg"] = False # Backgrounds should keep their texture
    
    # Large Environment / Wall
    elif any(k in filename for k in ['wall', 'env_', 'tree', 'rock', 'portal']):
        config["size"] = 512
        config["remove_bg"] = True
    
    # Tile (Usually 128x128 but keep texture)
    elif 'tile' in filename:
        config["size"] = 128
        config["remove_bg"] = False
        
    # Standard UI / Icon
    else:
        config["size"] = 128
        config["remove_bg"] = True
        
    return config

def parse_markdown(file_path):
    tasks = []
    if not os.path.exists(file_path):
        return tasks

    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    current_section = ""
    table_row_re = re.compile(r"^|\s*(.*?)\s*|\s*(.*?)\s*|\s*(.*?)\s*|")

    for line in lines:
        line = line.strip()
        if line.startswith("## "):
            current_section = line
            if "--已完成" in line:
                current_section = "COMPLETED"
            continue
            
        if current_section == "COMPLETED" or line.startswith("|--") or line.startswith("| :"):
            continue

        match = table_row_re.match(line)
        if match:
            cols = [c.strip() for c in line.split('|') if c.strip()]
            if len(cols) >= 4:
                path = cols[1].replace('`', '').strip()
                filename_raw = cols[2].replace('`', '').strip()
                prompt = cols[3]
                
                clean_filenames = filename_raw.replace('<br>', ',').split(',')
                for filename in clean_filenames:
                    filename = filename.strip()
                    if filename.endswith('.png'):
                         tasks.append({
                            "section": current_section,
                            "path": path,
                            "filename": filename,
                            "prompt": prompt
                        })
    return tasks

def main():
    print("--- NoMoreDay Advanced Batch Asset Generator ---")
    
    all_tasks = parse_markdown(MD_FILE_PATH)
    print(f"Found {len(all_tasks)} potential assets in Markdown.")
    
    generation_queue = []
    for task in all_tasks:
        full_dir = os.path.join(PROJECT_ROOT, task['path'])
        full_path = os.path.join(full_dir, task['filename'])
        
        if os.path.exists(full_path):
            continue
            
        generation_queue.append(task)
            
    print(f"\nQueued {len(generation_queue)} assets for generation.\n")
    if not generation_queue:
        print("Nothing to do.")
        return

    if not is_server_running():
        start_comfyui()

    try:
        ws = websocket.WebSocket()
        ws.connect(f"ws://{SERVER_ADDRESS}/ws?clientId={CLIENT_ID}")
    except Exception as e:
        print(f"Error connecting: {e}")
        return

    universal_keywords = "high-quality, masterpiece, professional game art, clean digital painting style, sharp edges, centered composition, best quality, 8k, highly detailed"
    style_keywords = "ink wash painting style, traditional chinese sumi-e, dark fantasy, ancient chinese aesthetics, mystical, high contrast, detailed texture, ethereal, wuxia atmosphere"
    
    for i, task in enumerate(generation_queue):
        print(f"[{i+1}/{len(generation_queue)}] Processing {task['filename']}...")
        
        filename = task['filename'].lower()
        config = get_asset_config(filename)
        
        specific_instruction = ""
        if "button" in filename:
            specific_instruction = "rectangular button UI element, horizontal layout, game interface asset, stone or jade texture"
        elif "slider" in filename or "bar" in filename:
            specific_instruction = "horizontal slider bar UI element, linear design"
        elif "tile" in filename:
            specific_instruction = "top-down view, seamless texture pattern, flat ground surface, no perspective, infinite tiling"
        elif "background" in filename or "panel" in filename:
            specific_instruction = "large game ui background panel, ornate borders, ancient paper or dark stone texture"
        elif "env_" in filename or "tree" in filename or "rock" in filename:
            specific_instruction = "solitary environment object, detailed silhouette, mystical atmosphere"

        full_prompt = f"{specific_instruction}, {task['prompt']}, {style_keywords}, {universal_keywords}."
        if config['remove_bg']:
            full_prompt += " Isolated on a plain white background."
        else:
            full_prompt += " Full frame coverage, no empty edges."
            
        negative_prompt = "blur, low quality, 3d render, text, background, photo, realistic, vector, flat, cartoon, rotated, tilted, perspective, distorted, messy, cluttered"

        full_dir = os.path.join(PROJECT_ROOT, task['path'])
        if not os.path.exists(full_dir):
            os.makedirs(full_dir)
            
        try:
            generate_image(ws, full_prompt, negative_prompt, full_dir, task['filename'], target_size=config['size'], remove_bg=config['remove_bg'])
        except Exception as e:
            print(f"Failed to generate {task['filename']}: {e}")
            
    ws.close()
    print("\nBatch Generation Complete.")

if __name__ == "__main__":
    main()
