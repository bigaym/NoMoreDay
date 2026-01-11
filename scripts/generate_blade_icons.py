import os
import subprocess
import sys

# 指向现有的 AI 资产生成脚本
ASSET_GEN_SCRIPT = os.path.join(os.path.dirname(__file__), "asset_gen.py")

# 根据《职业设计草案_剑修.md》定义的图标提示词
# 这些提示词将传递给 asset_gen.py，利用 Stable Diffusion 生成具有仙侠风格的图标
SKILLS = {
    # "flowing_thrust": "Silhouette of a swordsman charging forward in a powerful single-handed sword thrust, sharp cyan wind energy streaks, clean black silhouette with glowing edges, 2d game icon style", # 流云刺：位移与单体爆发
    # "rending_wave": "A powerful golden crescent sword wave cutting the air, sharp energy",       # 裂空斩：群体清理/半月剑气
    # "blade_formation": "A circle of floating ethereal spirit swords, blue magical glow, spiritual energy", # 灵剑决：自动化浮游灵剑
    # "blade_ward": "A protective dome made of spinning sword phantoms, defensive aura",           # 剑气护体：防御与拦截投射物
    "infinite_blades": "Low-angle 2.5D perspective looking up at a glowing circular sword array in the sky, distinct sharp spirit blades in vibrant colors falling down in straight lines, sharp edges, clean composition, high contrast, 2d game icon style, sharp focus", # 万剑归宗：全屏清场终极技能
    # "sword_array_execution": "2.5D isometric view of a circular daoist sword array, four distinct glowing swords at cardinal points: deep blue sword at North, silver white sword at West, fiery red sword at South, cyan green sword at East, glowing runes on a circular platform, symmetrical, clean composition", # 剑阵·诛仙：区域持续伤害
    # "mind_blade_shadowless": "A flash of pure white light representing an invisible sword strike, sharp and fast", # 心剑·无影：高频远程狙击
    # "blade_boomerang": "A curved jade blade, sharp green crystalline texture, a clean white circular motion arc line, elegant and lethal",        # 御剑·回旋：中程折返牵引
    # "phantom_flash": "A dark purple shadow dash with sword afterimages, stealthy movement"       # 绝影闪：看破反击与隐身
}

TALENTS = {
    "talent_mortal": "A handcrafted wooden practice sword entwined with fresh green vines and leaves, nature growth theme, clean silhouette", # 凡木区：敏捷侧基础天赋
    "talent_dao_source": "A glowing blue Taiji Yin-Yang symbol with a sharp spirit sword core, mystical energy, symmetrical, clean lines", # 道源区：智力侧法术转换
    "talent_ascension": "A radiant golden celestial sword pointing upwards, stylized golden auspicious clouds at the base, divine light, symmetrical" # 飞升区：核心/混合高级天赋
}

def run_gen(name, prompt, category):
    """调用 asset_gen.py 生成图标并移动到对应文件夹"""
    print(f"\n>>> 正在生成 {category} 图标: {name}...")
    
    # 使用当前 Python 解释器调用 asset_gen.py
    cmd = [
        sys.executable, ASSET_GEN_SCRIPT,
        "--prompt", prompt,
        "--name", name,
        "--width", "512",
        "--height", "512"
    ]
    
    try:
        subprocess.run(cmd, check=True)
        
        # 后处理：将生成的资产从 assets/textures 移动到具体的图标分类目录
        base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
        src_dir = os.path.join(base_dir, "assets", "textures")
        dst_dir = os.path.join(src_dir, "icons", category)
        
        if not os.path.exists(dst_dir):
            os.makedirs(dst_dir)
            
        # asset_gen.py 会在文件名后添加 ComfyUI 的随机后缀，我们需要匹配开头
        for filename in os.listdir(src_dir):
            if filename.startswith(name) and filename.endswith(".png"):
                src_path = os.path.join(src_dir, filename)
                dst_path = os.path.join(dst_dir, f"{name}.png")
                
                # 如果目标已存在，先删除
                if os.path.exists(dst_path):
                    os.remove(dst_path)
                
                os.rename(src_path, dst_path)
                print(f"成功保存至: {dst_path}")
                break
    except subprocess.CalledProcessError as e:
        print(f"生成 {name} 时出错: {e}")

def main():
    print("=== 剑修 (Blade Ascendant) 图标生成任务开始 ===")
    
    # 生成技能图标
    for i in range(10):  # 重试机制，最多尝试3次
        for name, prompt in SKILLS.items():
            run_gen(name, prompt, "skills")
        
    # 生成天赋图标
    # for name, prompt in TALENTS.items():
    #     run_gen(name, prompt, "talents")
        
    print("\n=== 所有图标已生成并整理至 assets/textures/icons/ ===")

if __name__ == "__main__":
    main()