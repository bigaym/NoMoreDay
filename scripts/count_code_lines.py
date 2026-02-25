import os
import re
import sys

def count_file_stats(file_path):
    """
    统计单个 C/C++ 文件的有效代码行数和注释行数。
    跳过纯空行。
    """
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        # 将 /* ... */ 块替换为相同数量的空格和换行，以保持行号一致
        # 这样我们可以通过对比原行和脱敏行来判断该行是否包含多行注释
        no_multi = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), content, flags=re.DOTALL)

        orig_lines = content.splitlines()
        no_multi_lines = no_multi.splitlines()
        
        effective_count = 0
        comment_count = 0

        for i in range(len(orig_lines)):
            line = orig_lines[i]
            if not line.strip():
                continue

            # 1. 统计有效代码行：移除 // 后，且在没有 /* */ 的版本中仍有内容
            code_only = re.sub(r'//.*', '', no_multi_lines[i])
            if code_only.strip():
                effective_count += 1

            # 2. 统计注释行：包含 // 或者该行被多行注释掩码修改过
            if '//' in line or line != no_multi_lines[i]:
                comment_count += 1

        return effective_count, comment_count
    except Exception as e:
        print(f"Error reading {file_path}: {e}")
        return 0, 0

def main():
    # 默认统计当前目录，或者从命令行获取路径
    target_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    
    if not os.path.isdir(target_dir):
        print(f"错误: 路径 '{target_dir}' 不是有效的目录。")
        return

    # 定义 C/C++ 相关扩展名
    valid_extensions = {'.c', '.cpp', '.h', '.hpp', '.cc', '.cxx', '.inl'}
    
    total_files = 0
    total_effective = 0
    total_comments = 0

    print(f"正在扫描目录: {os.path.abspath(target_dir)}")
    print("-" * 70)
    print(f"{'Code':>8} | {'Comm.':>8} | {'File Path'}")
    print("-" * 70)

    for root, _, files in os.walk(target_dir):
        for file in files:
            ext = os.path.splitext(file)[1].lower()
            if ext in valid_extensions:
                full_path = os.path.join(root, file)
                eff, comm = count_file_stats(full_path)
                
                total_files += 1
                total_effective += eff
                total_comments += comm
                
                # 打印相对路径以便阅读
                rel_path = os.path.relpath(full_path, target_dir)
                print(f"{eff:8} | {comm:8} | {rel_path}")

    print("-" * 70)
    print(f"统计结果:")
    print(f"  总文件数: {total_files}")
    print(f"  总有效代码行数: {total_effective}")
    print(f"  总注释行数: {total_comments}")

if __name__ == "__main__":
    main()