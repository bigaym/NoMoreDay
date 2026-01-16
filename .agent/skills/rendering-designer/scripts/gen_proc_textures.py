import argparse
import numpy as np
from PIL import Image
import os
import math

def generate_gradient(width, height, type='radial', center=(0.5, 0.5), radius=0.5):
    y, x = np.ogrid[:height, :width]
    center_y, center_x = height * center[1], width * center[0]
    
    if type == 'radial':
        # Calculate distance from center
        dist_from_center = np.sqrt((x - center_x)**2 + (y - center_y)**2)
        # Normalize
        max_dist = min(width, height) * radius
        gradient = 1 - (dist_from_center / max_dist)
        gradient = np.clip(gradient, 0, 1)
    elif type == 'linear_h':
        gradient = x / width
    elif type == 'linear_v':
        gradient = y / height
    
    return (gradient * 255).astype(np.uint8)

def generate_noise(width, height, scale=10.0, seed=None):
    if seed:
        np.random.seed(seed)
    
    # Simple Value Noise implementation for standalone usage
    # (In a real project, we might use a library like 'noise' if available, but numpy is safer)
    def smoothstep(t):
        return t * t * (3. - 2. * t)

    def lerp(t, a, b):
        return a + t * (b - a)

    # Generate grid
    grid_w = int(math.ceil(width / scale)) + 1
    grid_h = int(math.ceil(height / scale)) + 1
    
    gradients = np.random.rand(grid_h, grid_w)
    
    # Pixel coordinates
    y, x = np.ogrid[:height, :width]
    
    # Grid coordinates
    gx = x / scale
    gy = y / scale
    
    # Integer and fractional parts
    gxi = gx.astype(int)
    gyi = gy.astype(int)
    gxf = gx - gxi
    gyf = gy - gyi
    
    # Smooth interpolation
    u = smoothstep(gxf)
    v = smoothstep(gyf)
    
    # Noise values at corners
    n00 = gradients[gyi, gxi]
    n10 = gradients[gyi, gxi + 1]
    n01 = gradients[gyi + 1, gxi]
    n11 = gradients[gyi + 1, gxi + 1]
    
    # Interpolate
    nx0 = lerp(u, n00, n10)
    nx1 = lerp(u, n01, n11)
    val = lerp(v, nx0, nx1)
    
    return (val * 255).astype(np.uint8)

def main():
    parser = argparse.ArgumentParser(description="Generate procedural textures for VFX.")
    parser.add_argument('output', help="Output file path (e.g., assets/textures/vfx/mask_01.png)")
    parser.add_argument('--type', choices=['radial', 'linear_h', 'linear_v', 'noise'], default='radial')
    parser.add_argument('--width', type=int, default=512)
    parser.add_argument('--height', type=int, default=512)
    parser.add_argument('--scale', type=float, default=50.0, help="Scale for noise")
    
    args = parser.parse_args()
    
    # Ensure directory exists
    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    
    if args.type == 'noise':
        data = generate_noise(args.width, args.height, args.scale)
    else:
        data = generate_gradient(args.width, args.height, args.type)
        
    img = Image.fromarray(data, mode='L')
    img.save(args.output)
    print(f"Generated {args.type} texture at {args.output}")

if __name__ == "__main__":
    main()
