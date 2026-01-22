import os
import sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

# Configuration
ATLAS_WIDTH = 512
ATLAS_HEIGHT = 128
CELL_WIDTH = 32
CELL_HEIGHT = 48
OUTPUT_PATH = os.path.join("assets", "textures", "popup_glyphs.png")
FONT_SIZE = 36  # Adjust based on cell size
STROKE_WIDTH = 2
SHADOW_OFFSET = (2, 2)

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
            # Get bounding box
            bbox = draw.textbbox((0, 0), char, font=font)
            text_w = bbox[2] - bbox[0]
            text_h = bbox[3] - bbox[1]
            
            # Offset to center
            # Note: fonts have baseline issues, manual tweak might be needed
            txt_x = x + (CELL_WIDTH - text_w) / 2
            txt_y = y + (CELL_HEIGHT - text_h) / 2 - 4 # Move up slightly
            
            # 1. Drop Shadow
            draw.text((txt_x + SHADOW_OFFSET[0], txt_y + SHADOW_OFFSET[1]), char, font=font, fill=(0, 0, 0, 128))
            
            # 2. Outline (Stroke) - simple 8-way simulation
            stroke_color = (0, 0, 0, 255)
            for ox in range(-STROKE_WIDTH, STROKE_WIDTH + 1):
                for oy in range(-STROKE_WIDTH, STROKE_WIDTH + 1):
                    if ox == 0 and oy == 0: continue
                    draw.text((txt_x + ox, txt_y + oy), char, font=font, fill=stroke_color)
            
            # 3. Main Text (Gradient Simulation - Top White, Bottom Light Gray)
            # For simplicity in PIL, just Solid White or slightly yellow for Crit
            # We can vary color based on row?
            # Row 0 (Numbers): White
            # Row 1 (Text): Yellow/Orange?
            
            # Actually, the spec says "Color is passed in Instance".
            # So the texture should be White (Grayscale) so it can be tinted!
            # If we make it red, we can't tint it blue easily.
            # So Main Text = Pure White.
            # Outline = Black.
            
            draw.text((txt_x, txt_y), char, font=font, fill=(255, 255, 255, 255))

    # Ensure output directory exists
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    
    atlas.save(OUTPUT_PATH)
    print(f"Atlas generated at: {OUTPUT_PATH}")

if __name__ == "__main__":
    create_atlas()
