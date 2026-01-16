
import numpy as np
from PIL import Image, ImageDraw
import os
import math
import random

def create_dir(path):
    if not os.path.exists(path):
        os.makedirs(path)

def save_image(data, path):
    img = Image.fromarray(data, mode='RGBA')
    img.save(path)
    print(f"Generated {path}")

def gen_trail_smooth(size=(256, 64), filename="vfx_trail_smooth.png"):
    # Smooth trail: Fade out at tail (left), soft edges (top/bottom)
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    for x in range(size[0]):
        # X fade: 0 (left) to 1 (right)
        alpha_x = x / size[0]
        # Sharper fade at the very tip
        alpha_x = pow(alpha_x, 0.5) 
        
        for y in range(size[1]):
            # Y soft edges: Sine wave or bell curve
            # 0 -> 1 -> 0
            ny = y / size[1]
            alpha_y = math.sin(ny * math.pi)
            alpha_y = pow(alpha_y, 0.5) # Plump it up a bit
            
            val = int(255 * alpha_x * alpha_y)
            # Cyan tint base: R=0, G=255, B=255
            # But usually trails are white and tinted in shader. Let's make it white.
            data[y, x] = [255, 255, 255, val]
    
    save_image(data, filename)

def gen_noise_cloud(size=(256, 256), filename="vfx_noise_cloud.png"):
    # Simple Value Noise
    # For a real project, we'd use Perlin, but simple noise + blur works for placeholders
    noise = np.random.randint(0, 256, size, dtype=np.uint8)
    img = Image.fromarray(noise, mode='L')
    # Simple blur to make it "cloudy"
    # We can't use filters without more imports, so let's stick to raw noise or simple downscale/upscale
    img_small = img.resize((32, 32), Image.Resampling.BILINEAR)
    img_cloud = img_small.resize(size, Image.Resampling.BICUBIC)
    
    # Convert to RGBA
    cloud_data = np.array(img_cloud)
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    data[..., 0] = cloud_data
    data[..., 1] = cloud_data
    data[..., 2] = cloud_data
    data[..., 3] = cloud_data
    
    save_image(data, filename)

def gen_circle_shockwave(size=(256, 256), filename="vfx_circle_shockwave.png"):
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    cx, cy = size[0] / 2, size[1] / 2
    max_r = min(cx, cy)
    
    for y in range(size[1]):
        for x in range(size[0]):
            dx = x - cx
            dy = y - cy
            dist = math.sqrt(dx*dx + dy*dy)
            norm_dist = dist / max_r
            
            # Ring shape: Gaussian at r=0.8
            # exp(-((x-mu)^2 / (2sigma^2)))
            mu = 0.8
            sigma = 0.1
            if norm_dist > 1.0:
                val = 0
            else:
                val = math.exp(-((norm_dist - mu)**2) / (2 * sigma**2))
                val = int(val * 255)
            
            data[y, x] = [255, 255, 255, val]
            
    save_image(data, filename)

def gen_scratch_mask(size=(256, 256), filename="vfx_scratch_mask.png"):
    # Directional streaks
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    # Generate random horizontal streaks
    for _ in range(50):
        y = random.randint(0, size[1]-1)
        length = random.randint(20, 100)
        x_start = random.randint(0, size[0] - length)
        brightness = random.randint(100, 255)
        
        for i in range(length):
            if x_start + i < size[0]:
                data[y, x_start + i] = [255, 255, 255, brightness]
                
    # Blur it horizontally (fake)
    # Just save raw streaks for "sharp" scratches
    save_image(data, filename)

def gen_rune_array(size=(256, 256), filename="vfx_rune_array.png"):
    # Geometric circles
    img = Image.new('RGBA', size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    cx, cy = size[0] / 2, size[1] / 2
    
    # Outer ring
    draw.ellipse([10, 10, size[0]-10, size[1]-10], outline=(0, 255, 255, 255), width=3)
    # Inner ring
    draw.ellipse([50, 50, size[0]-50, size[1]-50], outline=(0, 255, 255, 128), width=2)
    # Triangle
    r = size[0] / 2 - 20
    points = []
    for i in range(3):
        angle = -math.pi/2 + i * (2 * math.pi / 3)
        px = cx + r * math.cos(angle)
        py = cy + r * math.sin(angle)
        points.append((px, py))
    
    draw.polygon(points, outline=(0, 255, 255, 200), width=2)
    
    img.save(filename)
    print(f"Generated {filename}")

if __name__ == "__main__":
    vfx_dir = "assets/textures/vfx/"
    create_dir(vfx_dir)
    
    gen_trail_smooth(filename=vfx_dir + "vfx_trail_smooth.png")
    gen_noise_cloud(filename=vfx_dir + "vfx_noise_cloud.png")
    gen_circle_shockwave(filename=vfx_dir + "vfx_circle_shockwave.png")
    gen_scratch_mask(filename=vfx_dir + "vfx_scratch_mask.png")
    gen_rune_array(filename=vfx_dir + "vfx_rune_array.png")
