"""
NoMoreDay Icon Standardization Utility
Purpose: Resizes all PNG icons in a target directory to exactly 128x128 pixels.
Usage: python scripts/resize_icons.py <directory_path>
"""
import os
import sys
from PIL import Image

def resize_icons_in_directory(target_dir):
    """
    将指定目录下的所有 PNG 文件调整为 128x128 大小并覆盖保存。
    """
    if not os.path.isdir(target_dir):
        print(f"错误: 路径 '{target_dir}' 不是一个有效的目录。")
        return

    # 获取所有 PNG 文件
    files = [f for f in os.listdir(target_dir) if f.lower().endswith('.png')]
    
    if not files:
        print(f"在目录 '{target_dir}' 中未找到 PNG 文件。")
        return

    print(f"开始处理目录: {target_dir}")
    print(f"找到 {len(files)} 个文件，准备调整尺寸为 128x128...")

    count = 0
    for filename in files:
        file_path = os.path.join(target_dir, filename)
        try:
            with Image.open(file_path) as img:
                # 使用 Resampling.LANCZOS 获得最佳的缩放质量
                # 如果你的 Pillow 版本较低，可以使用 Image.LANCZOS
                resized_img = img.resize((128, 128), Image.Resampling.LANCZOS)
                resized_img.save(file_path)
                count += 1
                print(f"[{count}/{len(files)}] 已重置大小: {filename}")
        except Exception as e:
            print(f"处理文件 {filename} 时出错: {e}")

    print(f"\n处理完成！共成功转换 {count} 个图标。")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("使用方法: python resize_icons.py <目标目录路径>")
        print("示例: python f:/NoMoreDay/scripts/resize_icons.py f:/NoMoreDay/assets/textures/icons/skills")
    else:
        target_path = sys.argv[1]
        resize_icons_in_directory(target_path)