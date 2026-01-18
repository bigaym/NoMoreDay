"""
NoMoreDay Placeholder Image Creator
Purpose: Generates solid-color RGBA PNGs to act as temporary UI/icon assets.
Usage: python scripts/create_placeholder_png.py
"""
from PIL import Image

def create_solid_color_png(filepath, size=(64, 64), color=(128, 128, 128, 255)):
    """Creates a solid color PNG image.

    Args:
        filepath (str): The path to save the PNG file.
        size (tuple): The size of the image (width, height).
        color (tuple): The RGBA color tuple (0-255).
    """
    img = Image.new('RGBA', size, color)
    img.save(filepath)
    print(f"Created placeholder PNG: {filepath}")

if __name__ == "__main__":
    # Main UI textures
    create_solid_color_png("assets/textures/ui/slot_background.png", size=(64, 64), color=(60, 60, 60, 255))
    create_solid_color_png("assets/textures/ui/equip_slot_background.png", size=(64, 64), color=(80, 80, 80, 255))
    create_solid_color_png("assets/textures/ui/panel_background.png", size=(100, 100), color=(40, 40, 40, 255))
    create_solid_color_png("assets/textures/ui/context_menu_bg.png", size=(50, 50), color=(70, 70, 70, 255))

    # Skill icons
    create_solid_color_png("assets/textures/ui/icons/skill_icon_1.png", size=(48, 48), color=(255, 0, 0, 255)) # Red for skill 1
    create_solid_color_png("assets/textures/ui/icons/skill_icon_2.png", size=(48, 48), color=(0, 255, 0, 255)) # Green for skill 2
