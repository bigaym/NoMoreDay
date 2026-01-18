
"""
NoMoreDay VFX Texture Utility
Purpose: Generates common shader masks (noise, trail masks, normal maps) 
         used for energy and distortion effects.
Usage: python scripts/gen_vfx_textures.py
"""
import numpy as np
from PIL import Image
import os

def create_dir(path):
    if not os.path.exists(path):
        os.makedirs(path)

def gen_noise_texture(size=(256, 256), filename="noise.png"):
    # Generate random noise as a base for energy flow
    noise = np.random.randint(0, 256, size, dtype=np.uint8)
    img = Image.fromarray(noise, mode='L')
    img.save(filename)
    print(f"Generated {filename}")

def gen_trail_mask(size=(256, 64), filename="trail_mask.png"):
    # Linear gradient from opaque to transparent
    # x direction: opaque at right (1.0), transparent at left (0.0)
    # y direction: opaque at center, transparent at edges
    
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    for x in range(size[0]):
        alpha_x = x / size[0] # 0 to 1
        for y in range(size[1]):
            # y alpha: bell curve
            y_norm = (y / size[1]) * 2 - 1 # -1 to 1
            alpha_y = 1.0 - abs(y_norm) # triangle
            alpha_y = alpha_y * alpha_y # curve
            
            val = int(255 * alpha_x * alpha_y)
            data[y, x] = [255, 255, 255, val]
            
    img = Image.fromarray(data, mode='RGBA')
    img.save(filename)
    print(f"Generated {filename}")

def gen_distortion_normal(size=(256, 256), filename="distortion_normal.png"):
    # Procedural wave normal map
    data = np.zeros((size[1], size[0], 3), dtype=np.uint8)
    for y in range(size[1]):
        for x in range(size[0]):
            nx = np.sin(x * 0.1) * 127 + 128
            ny = np.cos(y * 0.1) * 127 + 128
            data[y, x] = [int(nx), int(ny), 255]
            
    img = Image.fromarray(data, mode='RGB')
    img.save(filename)
    print(f"Generated {filename}")

if __name__ == "__main__":
    vfx_dir = "assets/textures/vfx/"
    create_dir(vfx_dir)
    
    gen_noise_texture((256, 256), vfx_dir + "energy_noise.png")
    gen_trail_mask((256, 64), vfx_dir + "trail_mask.png")
    gen_distortion_normal((256, 256), vfx_dir + "distortion_normal.png")
