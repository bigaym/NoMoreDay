"""
NoMoreDay General Image Resizer
Purpose: Batch resizes images in a folder to a specified square size.
Usage: python scripts/resize_images.py <folder_path> <target_size>
"""
import os
import sys
from PIL import Image

def main():
    # 检查参数数量，如果不足则打印帮助信息
    if len(sys.argv) < 3:
        print("图像批量缩放脚本")
        print("=" * 20)
        print("用法: python resize_images.py <图片文件夹路径> <目标尺寸>")
        print("示例: python resize_images.py ./pngs 512")
        print("说明: 该脚本会将指定文件夹下的所有图片缩放为 [目标尺寸 * 目标尺寸]，并保存在 'resized' 子目录中。")
        return

    folder_path = sys.argv[1]
    try:
        target_size = int(sys.argv[2])
    except ValueError:
        print("错误: 目标尺寸必须是一个整数。")
        return

    # 检查文件夹是否存在
    if not os.path.isdir(folder_path):
        print(f"错误: 路径 '{folder_path}' 不是一个有效的文件夹。")
        return

    # 创建输出目录
    output_dir = os.path.join(folder_path, "resized")
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # 支持的图像格式
    supported_formats = ('.png', '.jpg', '.jpeg', '.bmp', '.webp')
    files = [f for f in os.listdir(folder_path) if f.lower().endswith(supported_formats)]

    if not files:
        print("文件夹内未找到支持的图片格式。")
        return

    print(f"开始处理 {len(files)} 张图片，目标尺寸: {target_size}x{target_size}...")
    for filename in files:
        input_path = os.path.join(folder_path, filename)
        output_path = os.path.join(output_dir, filename)
        
        try:
            with Image.open(input_path) as img:
                # 使用 LANCZOS 滤镜保证缩放质量，并强制调整为正方形
                resized_img = img.resize((target_size, target_size), Image.Resampling.LANCZOS)
                resized_img.save(output_path)
                print(f"  [成功] {filename}")
        except Exception as e:
            print(f"  [失败] 处理 {filename} 时出错: {e}")

    print(f"\n处理完成！缩放后的图片保存在: {output_dir}")

if __name__ == "__main__":
    main()