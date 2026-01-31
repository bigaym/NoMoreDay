import subprocess
import os
import time
import sys

"""
Batch Asset Generator for NoMoreDay
Based on Asset_Regeneration_List.md
Optimized for FLUX.1 / Qwen3-8B Text Encoder
Style: Dark Ink-Wash Fantasy (Bronze & Jade)
"""

# Common style suffix for Flux to ensure consistency. 
# FLUX prefers natural language and descriptive style definitions over comma-separated tags.
# Common style suffix for Flux to ensure consistency. 
# FLUX preferred style: Natural language, descriptive, focus on texture and material.
STYLE_SUFFIX = ". high quality, 8k resolution, unituitive game asset, isolated on white background. Clean composition, sharp focus. NO TEXT, NO CHARACTERS, NO LETTERS, Clean Background."

# Asset Definitions
# Format: category, filename (no ext), prompt, target_w, target_h, remove_bg
ASSETS = [
    # --- Style Set 1: Frost Crystal (Clean, Cold, Magic) ---
    {
        "category": "UI_Buttons_New",
        "name": "btn_frost_square",
        "prompt": "A square game UI button made of translucent blue frost crystal. The surface is smooth and icy with subtle internal fractures. Glowing soft cyan light from within. A simple, clean, magical aesthetic.",
        "w": 128, "h": 128, "remove_bg": True
    },
    {
        "category": "UI_Buttons_New",
        "name": "btn_frost_wide_2x1",
        "prompt": "A wide rectangular game UI button made of translucent blue frost crystal. The edges are sharp and frosted. The surface is smooth ice. Glowing soft cyan light. Aspect ratio 2:1.",
        "w": 128, "h": 128, "remove_bg": True
    },
    {
        "category": "UI_Buttons_New",
        "name": "btn_frost_wide_3x1",
        "prompt": "A very wide rectangular game UI button made of translucent blue frost crystal. Sleek and elongated. The texture is pure ice with a glowing core. Aspect ratio 3:1.",
        "w": 128, "h": 128, "remove_bg": True
    },

    # --- Style Set 2: Hitech Obsidian (Dark, Sleek, Premium) ---
    {
        "category": "UI_Buttons_New",
        "name": "btn_obsidian_square",
        "prompt": "A square game UI button made of polished black obsidian volcanic glass. It has a thin, elegant gold rim. The surface is extremely glossy and reflective. Premium luxury look.",
        "w": 128, "h": 128, "remove_bg": True
    },
    {
        "category": "UI_Buttons_New",
        "name": "btn_obsidian_wide_2x1",
        "prompt": "A wide rectangular game UI button made of polished black obsidian. Gold trim along the edges. The black surface is deep and glossy. Minimalist and elegant. Aspect ratio 2:1.",
        "w": 128, "h": 128, "remove_bg": True
    },
    {
        "category": "UI_Buttons_New",
        "name": "btn_obsidian_wide_3x1",
        "prompt": "A very wide rectangular game UI button made of polished black obsidian with gold accents. Sleek, dark, and reflective. Aspect ratio 3:1.",
        "w": 128, "h": 128, "remove_bg": True
    },

    # --- Style Set 3: White Porcelain (Clean, Bright, Artistic) ---
    {
        "category": "UI_Buttons_New",
        "name": "btn_porcelain_square",
        "prompt": "A square game UI button made of smooth white porcelain or ceramic. High gloss white glaze. Subtle embossed cloud patterns in white (white-on-white). Very clean and pristine.",
        "w": 128, "h": 128, "remove_bg": True
    },
    {
        "category": "UI_Buttons_New",
        "name": "btn_porcelain_wide_2x1",
        "prompt": "A wide rectangular game UI button made of smooth white porcelain. The surface is perfect white glaze. Soft curved edges. Minimalist ceramic aesthetic. Aspect ratio 2:1.",
        "w": 128, "h": 128, "remove_bg": True
    },
    {
        "category": "UI_Buttons_New",
        "name": "btn_porcelain_wide_3x1",
        "prompt": "A very wide rectangular game UI button made of smooth white porcelain. Elongated and elegant. Clean white surface with very subtle shadows. Aspect ratio 3:1.",
        "w": 128, "h": 128, "remove_bg": True
    },
]

def run_generation():
    print(f"Starting Batch Generation for {len(ASSETS)} assets...")
    print(f"Global Style: {STYLE_SUFFIX}")
    
    for i, asset in enumerate(ASSETS):
        print(f"\n[{i+1}/{len(ASSETS)}] Generatng: {asset['name']} ({asset['category']})")
        
        # Construct full sentence prompt
        full_prompt = asset['prompt'] + STYLE_SUFFIX
        output_dir = os.path.join("assets", "generated", asset['category'])
        
        cmd = [
            sys.executable, "scripts/asset_gen.py",
            "--prompt", full_prompt,
            "--name", asset['name'],
            "--target-width", str(asset['w']),
            "--target-height", str(asset['h']),
            "--output-dir", output_dir
        ]
        
        if not asset['remove_bg']:
            cmd.append("--no-remove-bg")
            
        # Execute
        try:
            # Set NUMBA_CACHE_DIR to local directory to avoid permission issues in conda env
            env = os.environ.copy()
            env["NUMBA_CACHE_DIR"] = os.path.join(os.getcwd(), ".numba_cache")
            if not os.path.exists(env["NUMBA_CACHE_DIR"]):
                os.makedirs(env["NUMBA_CACHE_DIR"], exist_ok=True)
                
            subprocess.run(cmd, check=True, env=env)
            # Small cooldown to be safe with ComfyUI queue
            time.sleep(1)
        except subprocess.CalledProcessError as e:
            print(f"!!! Error generating {asset['name']}: {e}")
            
    print("\nBatch Generation Complete!")

if __name__ == "__main__":
    run_generation()
