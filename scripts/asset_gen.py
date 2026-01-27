"""
NoMoreDay Asset Generator
Purpose: Generates high-quality 2D game assets using ComfyUI (Flux.1 model).
         Supports automatic background removal, custom output directories, and resizing.

Usage: 
  Basic:
    python scripts/asset_gen.py --prompt "golden sword" --name "weapon_gold_sword"
  
  Advanced (Custom Size & Directory):
    python scripts/asset_gen.py --prompt "potion" --target-width 64 --target-height 64 --output-dir "assets/icons"
  
  Background Retention:
    python scripts/asset_gen.py --prompt "forest background" --no-remove-bg --target-width 512 --target-height 512
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

SERVER_ADDRESS = "127.0.0.1:8188"
CLIENT_ID = str(uuid.uuid4())
COMFYUI_PATH = r"D:\\Program Files\\ComfyUI_windows_portable\\run_nvidia_gpu.bat"

# Force no proxy for localhost to avoid port 7890 issues
os.environ["NO_PROXY"] = "127.0.0.1,localhost"

def is_server_running():
    try:
        requests.get(f"http://{SERVER_ADDRESS}", timeout=1)
        return True
    except requests.RequestException:
        # Catches ConnectionError, Timeout, etc.
        return False

def start_comfyui(use_lowvram=True):
    print(f"ComfyUI not found at {SERVER_ADDRESS}. Starting it now...")
    if not os.path.exists(COMFYUI_PATH):
        print(f"Error: Could not find ComfyUI start script at: {COMFYUI_PATH}")
        print("Tip: If you are using a 12GB card like 4070S, ensure your .bat file includes --lowvram")
        sys.exit(1)
    
    # Extract the directory of the batch file to set as CWD
    comfyui_dir = os.path.dirname(COMFYUI_PATH)
    
    # Start ComfyUI with potential memory optimization flags
    lowvram_flag = "--lowvram" if use_lowvram else ""
    cmd = f'start "ComfyUI" /D "{comfyui_dir}" "{COMFYUI_PATH}" {lowvram_flag}'
    print(f"Executing: {cmd}")
    subprocess.Popen(cmd, shell=True)
    
    print("Waiting for ComfyUI to initialize (this may take 10-20 seconds)...")
    if use_lowvram:
        print("Note: Flux2 requires massive VRAM. On 12GB cards, it MUST run in --lowvram mode.")
    for i in range(30):
        if is_server_running():
            print("ComfyUI is Online!")
            return
        time.sleep(1)
        print(".", end="", flush=True)
    
    print("\nError: Timed out waiting for ComfyUI to start.")
    sys.exit(1)

# Standard Text-to-Image Workflow for ComfyUI
# Node IDs: 
# 3: KSampler, 4: Checkpoint, 6: Positive Prompt, 7: Negative Prompt, 
# 8: VAE Decode, 9: Save Image, 5: Empty Latent
def get_default_workflow(unet_name="flux-2-klein-9b.safetensors"):
    return {
        "10": {
            "inputs": {
                "unet_name": unet_name,
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
                "text": "masterpiece, best quality, 2d game sprite, high detail, white background",
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
                "steps": 7,
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
                "filename_prefix": "NoMoreDay_Asset"
            },
            "class_type": "SaveImage"
        }
    }

def get_available_models():
    try:
        response = requests.get(f"http://{SERVER_ADDRESS}/object_info/CheckpointLoaderSimple")
        if response.status_code == 200:
            data = response.json()
            # print("Debug - Model Info:", json.dumps(data, indent=2)) 
            models = data['CheckpointLoaderSimple']['input']['required']['ckpt_name'][0]
            if not models:
                 print("Warning: ComfyUI reports no Checkpoints available!")
            return models
    except Exception as e:
        print(f"Error fetching models: {e}")
    return []

def queue_prompt(prompt_workflow):
    p = {"prompt": prompt_workflow, "client_id": CLIENT_ID}
    data = json.dumps(p).encode('utf-8')
    req = urllib.request.Request(f"http://{SERVER_ADDRESS}/prompt", data=data)
    try:
        return json.loads(urllib.request.urlopen(req).read())
    except urllib.error.HTTPError as e:
        print(f"HTTP Error {e.code}: {e.reason}")
        print("Server Response:", e.read().decode('utf-8'))
        raise e

def get_image(filename, subfolder, folder_type):
    data = {"filename": filename, "subfolder": subfolder, "type": folder_type}
    url_values = urllib.parse.urlencode(data)
    with urllib.request.urlopen(f"http://{SERVER_ADDRESS}/view?{url_values}") as response:
        return response.read()

def get_history(prompt_id):
    with urllib.request.urlopen(f"http://{SERVER_ADDRESS}/history/{prompt_id}") as response:
        return json.loads(response.read())

import io
from PIL import Image
import rembg

...

def get_images(ws, prompt, output_dir, file_prefix, remove_bg=True):
    prompt_id = queue_prompt(prompt)['prompt_id']
    print(f"Step 1: Prompt Queued (ID: {prompt_id})... waiting for generation.")
    
    current_images = []
    
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
                
                # Post-processing: Background Removal
                if remove_bg:
                    print(f"Step 2: Removing background for {image['filename']}...")
                    input_image = Image.open(io.BytesIO(img_data))
                    output_image = rembg.remove(input_image)
                    
                    filename = f"{file_prefix}_{image['filename']}"
                    if not filename.lower().endswith(".png"):
                        filename += ".png"
                    
                    file_path = os.path.join(output_dir, filename)
                    output_image.save(file_path, "PNG")
                else:
                    filename = f"{file_prefix}_{image['filename']}"
                    file_path = os.path.join(output_dir, filename)
                    with open(file_path, "wb") as f:
                        f.write(img_data)
                
                print(f"Step 3: Asset Saved -> {file_path}")
                current_images.append(file_path)

    return current_images

def main():
    parser = argparse.ArgumentParser(description="NoMoreDay Asset Generator")
    parser.add_argument("--prompt", type=str, required=True, help="Positive prompt for the asset")
    parser.add_argument("--negative", type=str, default="blur, low quality, 3d render, text, background", help="Negative prompt")
    parser.add_argument("--width", type=int, default=1024, help="Width of the sprite")
    parser.add_argument("--height", type=int, default=1024, help="Height of the sprite")
    parser.add_argument("--name", type=str, default="asset", help="Output filename prefix")
    parser.add_argument("--no-remove-bg", action="store_true", help="Disable automatic background removal")
    parser.add_argument("--target-width", type=int, default=128, help="Final asset width after downscaling")
    parser.add_argument("--target-height", type=int, default=128, help="Final asset height after downscaling")
    parser.add_argument("--output-dir", type=str, default=os.path.join("assets", "textures"), help="Directory to save generated assets")
    parser.add_argument("--model", type=str, default="flux-2-klein-9b.safetensors", help="Model to use (e.g., flux-2-klein-base-4b.safetensors)")
    
    args = parser.parse_args()

    # Determine if we need lowvram (Assume 4B models don't need it as much, per user instruction)
    use_lowvram = True
    if "4b" in args.model.lower():
        use_lowvram = False
        print(f"4B Model detected ('{args.model}'), disabling --lowvram.")

    # 1. Connect (Auto-start if needed)
    if not is_server_running():
        start_comfyui(use_lowvram=use_lowvram)

    print(f"Connecting to ComfyUI at {SERVER_ADDRESS}...")
    try:
        ws = websocket.WebSocket()
        ws.connect(f"ws://{SERVER_ADDRESS}/ws?clientId={CLIENT_ID}")
    except Exception as e:
        print(f"Error connecting to ComfyUI: {e}")
        print("Please ensure ComfyUI is running and the address is correct.")
        return

    # 2. Setup Workflow
    workflow = get_default_workflow(unet_name=args.model)
    
    # Ensure dimensions are multiples of 8
    gen_width = (args.width // 8) * 8
    gen_height = (args.height // 8) * 8
    
    print(f"Workflow Configuration: Generation Res={gen_width}x{gen_height}, Prompt='{args.prompt}'")
    
    # Update Prompts and Dimensions
    workflow["6"]["inputs"]["text"] = f"{args.prompt}, white background, isolated, 2d game sprite"
    workflow["7"]["inputs"]["text"] = args.negative
    workflow["5"]["inputs"]["width"] = gen_width
    workflow["5"]["inputs"]["height"] = gen_height

    # 3. Generate
    output_path = os.path.abspath(args.output_dir)
    if not os.path.exists(output_path):
        os.makedirs(output_path)

    image_paths = get_images(ws, workflow, output_path, args.name, not args.no_remove_bg)
    
    # Downscale for game use (Standard 128x128 for NoMoreDay assets)
    for img_path in image_paths:
        try:
            with Image.open(img_path) as img:
                print(f"Standardizing {os.path.basename(img_path)} to {args.target_width}x{args.target_height}...")
                # Maintain aspect ratio by padding or just force square for icons
                img_res = img.resize((args.target_width, args.target_height), Image.Resampling.LANCZOS)
                img_res.save(img_path, "PNG")
        except Exception as e:
            print(f"Downscaling failed: {e}")
    ws.close()
    print("Generation Complete.")

if __name__ == "__main__":
    main()
