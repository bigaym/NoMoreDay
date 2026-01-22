import os
import sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

# Configuration
ATLAS_WIDTH = 1024
ATLAS_HEIGHT = 128
CELL_WIDTH = 64
CELL_HEIGHT = 64
OUTPUT_PATH = os.path.join("assets", "textures", "icons", "fastfont", "popup_glyphs.png")
FONT_SIZE = 48  # Large enough for high-res
STROKE_WIDTH = 3
SHADOW_OFFSET = (3, 3)

# Character Set
# Row 0: Numbers and Symbols
ROW_0 = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '-', '.', '%', '!', '?']
# Row 1: Chinese Characters (Critical, Dodge, Block, etc.)
ROW_1 = ['暴', '击', '闪', '避', '格', '挡', '免', '疫', '吸', '收', ' ', ' ', ' ', ' ', ' ', ' ']

GRID = [ROW_0, ROW_1]

def find_font():
    # Priority list of fonts
    font_names = [
        "simhei.ttf",   # Windows Chinese Black
        "msyh.ttc",     # Microsoft YaHei
        "arial.ttf",    # Fallback
        "segoeui.ttf"   # Windows UI
    ]
    
    system_font_dirs = [
        r"C:\Windows\Fonts",
        r"C:\Users\123\AppData\Local\Microsoft\Windows\Fonts"
    ]
    
    for name in font_names:
        for folder in system_font_dirs:
            path = os.path.join(folder, name)
            if os.path.exists(path):
                print(f"Using font: {path}")
                return path
    
    print("Warning: No preferred fonts found. Using default.")
    return None

def create_atlas():
    # Create transparent image
    atlas = Image.new('RGBA', (ATLAS_WIDTH, ATLAS_HEIGHT), (0, 0, 0, 0))
    draw = ImageDraw.Draw(atlas)
    
    font_path = find_font()
    try:
        if font_path:
            font = ImageFont.truetype(font_path, FONT_SIZE)
        else:
            font = ImageFont.load_default()
    except Exception as e:
        print(f"Error loading font: {e}")
        font = ImageFont.load_default()

    for row_idx, row_chars in enumerate(GRID):
        for col_idx, char in enumerate(row_chars):
            if not char or char == ' ':
                continue
                
            x = col_idx * CELL_WIDTH
            y = row_idx * CELL_HEIGHT
            
            # Center text in cell
            bbox = draw.textbbox((0, 0), char, font=font)
            text_w = bbox[2] - bbox[0]
            text_h = bbox[3] - bbox[1]
            
            # Offset to center
            txt_x = x + (CELL_WIDTH - text_w) / 2
            txt_y = y + (CELL_HEIGHT - text_h) / 2 - 4 # Adjusted offset
            
            # Create a temporary layer for shadow to apply blur
            shadow_layer = Image.new('RGBA', (CELL_WIDTH, CELL_HEIGHT), (0, 0, 0, 0))
            shadow_draw = ImageDraw.Draw(shadow_layer)
            # Draw shadow text shifted
            shadow_draw.text((txt_x - x + SHADOW_OFFSET[0], txt_y - y + SHADOW_OFFSET[1]), 
                             char, font=font, fill=(0, 0, 0, 160))
            # Blur the shadow slightly for better transition
            shadow_layer = shadow_layer.filter(ImageFilter.GaussianBlur(radius=1.0))
            atlas.alpha_composite(shadow_layer, (x, y))

            # Draw Main Text with native stroke for high quality antialiasing
            # stroke_width=1 provides a very sharp outline
            draw.text((txt_x, txt_y), char, font=font, fill=(255, 255, 255, 255),
                      stroke_width=1, stroke_fill=(0, 0, 0, 255))

    # Ensure output directory exists
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    
    atlas.save(OUTPUT_PATH)
    print(f"Atlas generated at: {OUTPUT_PATH}")

if __name__ == "__main__":
    create_atlas()
