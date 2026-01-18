"""
NoMoreDay VFX Asset Name Trimmer
Purpose: Trims everything starting from '_NoMoreDay' in filenames while preserving extension.
Usage: python scripts/trim_vfx_names.py <folder_path>
"""
import os
import sys

def main():
    if len(sys.argv) < 2:
        print("VFX 资源命名精简脚本")
        print("=" * 20)
        print("用法: python scripts/trim_vfx_names.py <目录路径>")
        print("示例: python scripts/trim_vfx_names.py assets/textures/vfx")
        print("说明: 脚本将删除文件名中 '_NoMoreDay' 及其之后的所有内容，并保留扩展名。如果目标文件已存在则覆盖。")
        return

    folder_path = sys.argv[1]
    if not os.path.isdir(folder_path):
        print(f"错误: 路径 '{folder_path}' 不是一个有效的文件夹。")
        return

    target_marker = "_NoMoreDay"
    try:
        files = [f for f in os.listdir(folder_path) if os.path.isfile(os.path.join(folder_path, f))]
    except Exception as e:
        print(f"无法读取目录: {e}")
        return

    if not files:
        print("文件夹内未找到文件。")
        return

    print(f"正在处理目录: {folder_path}...")
    count = 0
    
    for filename in files:
        if target_marker in filename:
            name_part, extension = os.path.splitext(filename)
            marker_pos = name_part.find(target_marker)
            new_name = name_part[:marker_pos] + extension
            
            if not new_name or new_name == extension:
                print(f"  [跳过] {filename}: 截断后文件名为空")
                continue
            
            if new_name == filename:
                continue
            
            old_path = os.path.join(folder_path, filename)
            new_path = os.path.join(folder_path, new_name)
            
            try:
                os.replace(old_path, new_path)
                print(f"  [重命名] {filename} -> {new_name}")
                count += 1
            except Exception as e:
                print(f"  [错误] 无法处理 {filename}: {e}")

    print(f"\n处理完成！共重构并覆盖了 {count} 个文件。")

if __name__ == "__main__":
    main()
