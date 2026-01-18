
"""
NoMoreDay Sword Intent UI & VFX Generator
Purpose: Generates simple UI icons (sword) and ink-wash splatter masks for 
         the Sword Intent system using procedural PIL drawing.
Usage: python scripts/gen_sword_intent_assets.py
"""
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
import os
import random
import math

def create_dir(path):
    if not os.path.exists(path):
        os.makedirs(path)

def gen_sword_icon(size=(64, 64), filename="ui_sword_icon.png"):
    img = Image.new('RGBA', size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Simple sword shape
    cx, cy = size[0] // 2, size[1] // 2
    
    # Blade color (Cyan-ish white)
    blade_color = (200, 255, 255, 255)
    hilt_color = (100, 100, 100, 255)
    
    # Blade (triangle + rect)
    blade_w = 8
    blade_h = 32
    draw.rectangle([cx - blade_w//2, cy - blade_h//2, cx + blade_w//2, cy + blade_h//2], fill=blade_color)
    draw.polygon([(cx - blade_w//2, cy - blade_h//2), (cx + blade_w//2, cy - blade_h//2), (cx, cy - blade_h//2 - 10)], fill=blade_color)
    
    # Guard
    draw.rectangle([cx - 12, cy + blade_h//2, cx + 12, cy + blade_h//2 + 4], fill=hilt_color)
    
    # Handle
    draw.rectangle([cx - 3, cy + blade_h//2 + 4, cx + 3, cy + blade_h//2 + 14], fill=hilt_color)
    
    img.save(filename)
    print(f"Generated {filename}")

def gen_ink_splatter(size=(256, 256), filename="vfx_ink_splatter.png"):
    img = Image.new('L', size, 0)
    draw = ImageDraw.Draw(img)
    
    cx, cy = size[0] // 2, size[1] // 2
    max_radius = size[0] // 2 - 10
    
    # Base blob
    draw.ellipse([cx - 30, cy - 30, cx + 30, cy + 30], fill=255)
    
    # Random splats
    for _ in range(20):
        angle = random.uniform(0, 2 * math.pi)
        dist = random.uniform(20, max_radius)
        r = random.uniform(2, 10)
        
        px = cx + dist * math.cos(angle)
        py = cy + dist * math.sin(angle)
        
        # Connect to center with variable thickness line
        width = int(r * (1.0 - dist/max_radius) + 1)
        draw.line([cx, cy, px, py], fill=200, width=width)
        draw.ellipse([px-r, py-r, px+r, py+r], fill=255)

    # Blur to make it liquid-y
    img = img.filter(ImageFilter.GaussianBlur(2))
    
    # Threshold to sharpen edges again (ink look)
    # This part requires numpy
    arr = np.array(img)
    arr = np.where(arr > 100, 255, 0).astype(np.uint8)
    
    img = Image.fromarray(arr, mode='L')
    
    # Add alpha channel
    final_img = Image.new('RGBA', size)
    final_img.putalpha(img)
    
    # Fill with black ink color
    # Create a black image
    black = Image.new('RGB', size, (0, 0, 0))
    final_img.paste(black, (0,0), img) # Use mask
    
    final_img.save(filename)
    print(f"Generated {filename}")

def gen_aura_noise(size=(256, 256), filename="vfx_aura_noise.png"):
    # Generate seamless noise
    # Simple random noise for now, smoothed
    noise = np.random.randint(0, 255, size, dtype=np.uint8)
    img = Image.fromarray(noise, mode='L')
    img = img.filter(ImageFilter.GaussianBlur(5))
    img.save(filename)
    print(f"Generated {filename}")

if __name__ == "__main__":
    vfx_dir = "assets/textures/vfx/"
    ui_dir = "assets/textures/ui/"
    create_dir(vfx_dir)
    create_dir(ui_dir)
    
    gen_sword_icon((64, 64), ui_dir + "ui_sword_icon.png")
    gen_ink_splatter((256, 256), vfx_dir + "vfx_ink_splatter.png")
    gen_aura_noise((256, 256), vfx_dir + "vfx_aura_noise.png")
