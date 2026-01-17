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

# --- ComfyUI Helper Functions (Adapted from asset_gen.py) ---

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
    # Use the same workflow as asset_gen.py (Flux based)
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
    # Flux likes 1024x1024 generation
    gen_width = 1024
    gen_height = 1024
    
    workflow["6"]["inputs"]["text"] = f"{prompt_text}, white background, isolated, 2d game sprite"
    workflow["7"]["inputs"]["text"] = negative_prompt
    workflow["5"]["inputs"]["width"] = gen_width
    workflow["5"]["inputs"]["height"] = gen_height
    workflow["3"]["inputs"]["seed"] = random.randint(1, 1000000000)

    prompt_id = queue_prompt(workflow)['prompt_id']
    print(f" -> Generating: {filename} (ID: {prompt_id})")

    while True:
        out = ws.recv()
        if isinstance(out, str):
            message = json.loads(out)
            if message['type'] == 'executing':
                data = message['data']
                if data['node'] is None and data['prompt_id'] == prompt_id:
                    break # Execution is done
        else:
            continue

    history = get_history(prompt_id)[prompt_id]
    for node_id in history['outputs']:
        node_output = history['outputs'][node_id]
        if 'images' in node_output:
            for image in node_output['images']:
                img_data = get_image(image['filename'], image['subfolder'], image['type'])
                
                # Processing
                img = Image.open(io.BytesIO(img_data))
                
                if remove_bg:
                    img = rembg.remove(img)
                
                # Resize
                img = img.resize((target_size, target_size), Image.Resampling.LANCZOS)
                
                # Save
                full_path = os.path.join(output_path, filename)
                img.save(full_path, "PNG")
                print(f" -> Saved to {full_path}")
                return # Only one image per prompt expected

# --- Markdown Parsing & Logic ---

def is_small_asset(filename, section_title):
    # Rules for identifying assets <= 128x128
    
    filename = filename.lower()
    
    # 1. Explicitly small types
    small_keywords = ['button', 'checkbox', 'slider', 'icon', 'slot', 'tile', 'shard', 'rune']
    
    # 2. Exclude typically large types (unless overridden)
    large_keywords = ['panel', 'bg', 'background', 'border', 'frame', 'bar', 'fill', 'divider', 'wall', 'env', 'tree', 'rock', 'portal', 'character', 'monster', 'vfx', 'splatter', 'trail', 'distortion']
    
    # Exceptions (Slot backgrounds are small)
    if 'slot_background' in filename or 'equip_slot_background' in filename:
        return True
    
    # Check exclusion first
    for k in large_keywords:
        if k in filename:
            # Check if it's a tile (tiles are usually 128x128 in this project context)
            if 'tile' in filename:
                return True
            return False
            
    # Check inclusion
    for k in small_keywords:
        if k in filename:
            return True
            
    return False

def parse_markdown(file_path):
    tasks = []
    
    if not os.path.exists(file_path):
        print(f"File not found: {file_path}")
        return tasks

    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    current_section = ""
    section_completed = False
    
    # Regex to match table rows: | Col1 | Col2 | Col3 | ... | 
    table_row_re = re.compile(r"^|\s*(.*?)\s*|\s*(.*?)\s*|\s*(.*?)\s*|")

    for line in lines:
        line = line.strip()
        
        # Section Detection
        if line.startswith("## "):
            current_section = line
            section_completed = "--已完成" in line or "Completed" in line
            continue
            
        if section_completed:
            continue
            
        # Skip header separators
        if line.startswith("|--") or line.startswith("| :"):
            continue

        # Table Parsing
        match = table_row_re.match(line)
        if match:
            cols = [c.strip() for c in line.split('|') if c.strip()]
            
            # We expect at least 3 columns for valid asset rows
            # Format varies:
            # UI: | Name | Path | Filename | Prompt |
            # Map: | Category | Path | Filename | Prompt |
            
            if len(cols) >= 4:
                path = cols[1]
                filename = cols[2]
                prompt = cols[3]
                
                # Cleanup path (remove backticks if present)
                path = path.replace('`', '').strip()
                filename = filename.replace('`', '').strip()
                
                # Check validity
                if filename.endswith('.png'):
                     tasks.append({
                        "section": current_section,
                        "path": path,
                        "filename": filename,
                        "prompt": prompt
                    })

    return tasks

def main():
    print("--- NoMoreDay Batch Asset Generator ---")
    
    # 1. Parse Tasks
    all_tasks = parse_markdown(MD_FILE_PATH)
    print(f"Found {len(all_tasks)} potential assets in Markdown.")
    
    generation_queue = []
    
    for task in all_tasks:
        # Resolve full path
        # Path in MD might be 'assets/textures/ui/' or relative
        rel_path = task['path']
        full_dir = os.path.join(PROJECT_ROOT, rel_path)
        full_path = os.path.join(full_dir, task['filename'])
        
        # Check existence
        if os.path.exists(full_path):
            # print(f"Skipping [Exists]: {task['filename']}")
            continue
            
        # Check size classification
        if is_small_asset(task['filename'], task['section']):
            generation_queue.append(task)
        else:
            print(f"Skipping [Pending/Large]: {task['filename']}")
            
    print(f"\nQueued {len(generation_queue)} assets for generation.\n")
    
    if not generation_queue:
        print("Nothing to do.")
        return

    # 2. Init ComfyUI
    if not is_server_running():
        start_comfyui()

    print(f"Connecting to ComfyUI at {SERVER_ADDRESS}...")
    try:
        ws = websocket.WebSocket()
        ws.connect(f"ws://{SERVER_ADDRESS}/ws?clientId={CLIENT_ID}")
    except Exception as e:
        print(f"Error connecting: {e}")
        return

    # 3. Process Queue
    universal_keywords = "high-quality, masterpiece, professional game art, clean digital painting style, sharp edges, centered composition"
    style_keywords = "ink wash painting style, dark fantasy, ancient chinese aesthetics, mystical, high contrast, detailed texture"
    
    for i, task in enumerate(generation_queue):
        print(f"[{i+1}/{len(generation_queue)}] Processing {task['filename']}...")
        
        # Determine specific prompt enhancements based on asset type
        specific_instruction = ""
        filename = task['filename'].lower()
        
        # UI Button logic: Force aspect ratio descriptions in prompt (though generation is square, the content should fit)
        if "button" in filename:
            specific_instruction = "rectangular button UI element, horizontal layout, game interface asset, stone or jade texture"
        elif "slider" in filename or "bar" in filename:
            specific_instruction = "horizontal slider bar UI element, linear design, game interface asset"
        elif "icon" in filename or "slot" in filename:
             specific_instruction = "square icon, game interface asset"
        elif "tile" in filename:
            specific_instruction = "top-down view, seamless texture pattern, flat ground surface, no perspective, infinite tiling"

        # Combine prompts: [Specific Type Instruction] + [Markdown Description] + [Style] + [Universal Quality]
        full_prompt = (
            f"{specific_instruction}, {task['prompt']}, {style_keywords}, {universal_keywords}. "
            f"Isolated on a plain white background."
        )
        
        # Refined Negative Prompt
        negative_prompt = (
            "blur, low quality, 3d render, text, background, photo, realistic, vector, flat, cartoon, "
            "rotated, tilted, perspective, distorted, messy, cluttered, multiple objects, collage"
        )
        
        if "tile" in filename:
             # Tiles shouldn't be isolated or have background removed in the same way, but the prompt needs to ensure full coverage
             full_prompt = (
                f"{specific_instruction}, {task['prompt']}, {style_keywords}, {universal_keywords}. "
                f"Full frame texture, no borders, no empty space."
            )
             negative_prompt += ", borders, edges, frames, vignette, white background"

        # Prepare Output Dir
        rel_path = task['path']
        full_dir = os.path.join(PROJECT_ROOT, rel_path)
        if not os.path.exists(full_dir):
            os.makedirs(full_dir)
            
        # Generate
        # Determine background removal needs
        remove_bg = True
        if "tile" in filename or "background" in filename:
            remove_bg = False
            
        try:
            generate_image(
                ws, 
                full_prompt, 
                negative_prompt, 
                full_dir, 
                task['filename'], 
                target_size=128, 
                remove_bg=remove_bg
            )
        except Exception as e:
            print(f"Failed to generate {task['filename']}: {e}")
            
    ws.close()
    print("\nBatch Generation Complete.")

if __name__ == "__main__":
    main()
