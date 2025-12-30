import os
import sys
import shutil

def organize_images(target_dir):
    # 1. 校验目录是否存在
    if not os.path.exists(target_dir):
        print(f"错误: 目录 '{target_dir}' 不存在。")
        return

    # 获取所有png文件 (不包含子文件夹中的)
    files = [f for f in os.listdir(target_dir) 
             if os.path.isfile(os.path.join(target_dir, f)) and f.lower().endswith('.png')]
    
    if not files:
        print("指定目录下没有找到 PNG 文件。")
        return

    print(f"找到 {len(files)} 个 PNG 文件，开始处理...")

    # 用于记录当前运行中，各类别已经处理到的序号
    # 格式: {'amulet': 5, 'ring': 0}
    category_counters = {}

    for filename in files:
        # 2. 解析类别
        # 假设文件名格式为: category_rest_of_name.png
        if '_' not in filename:
            print(f"[跳过] 文件名格式不符 (无下划线): {filename}")
            continue
        
        category = filename.split('_')[0]
        
        # 3. 准备目标子文件夹
        category_dir = os.path.join(target_dir, category)
        
        # 如果是脚本运行期间第一次遇到这个类别
        if category not in category_counters:
            if not os.path.exists(category_dir):
                os.makedirs(category_dir)
                category_counters[category] = 0
            else:
                # 如果文件夹已存在，计算里面已有的 png 数量，作为起始序号
                # 这样可以避免覆盖之前运行脚本生成的文件
                existing_files = [f for f in os.listdir(category_dir) if f.lower().endswith('.png')]
                category_counters[category] = len(existing_files)

        # 4. 生成新文件名
        # 获取当前序号
        current_index = category_counters[category]
        new_filename = f"{category}_{current_index}.png"
        
        src_path = os.path.join(target_dir, filename)
        dst_path = os.path.join(category_dir, new_filename)

        # 5. 移动并重命名
        try:
            # 防止目标文件已存在（极端情况）
            if os.path.exists(dst_path):
                print(f"[警告] 目标文件已存在，跳过: {dst_path}")
            else:
                shutil.move(src_path, dst_path)
                print(f"[移动] {filename} -> {category}/{new_filename}")
                # 成功移动后，计数器加1
                category_counters[category] += 1
        except Exception as e:
            print(f"[错误] 处理 {filename} 时出错: {e}")

    print("\n所有文件整理完成。")

if __name__ == "__main__":
    # 检查命令行参数
    if len(sys.argv) < 2:
        print("使用方法: python organize_images.py <目标目录路径>")
    else:
        target_path = sys.argv[1]
        organize_images(target_path)
