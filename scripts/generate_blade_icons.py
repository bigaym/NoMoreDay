import os
import subprocess
import sys

# 指向现有的 AI 资产生成脚本
ASSET_GEN_SCRIPT = os.path.join(os.path.dirname(__file__), "asset_gen.py")

# 全局风格后缀：遵循《VFX 设计文档》的灵动水墨风格
STYLE_SUFFIX = (
    "2d game skill icon, ink wash cultivation style, ethereal ink, "
    "high contrast, sharp edges, white background, pale cyan energy (#C3F8F5) highlights, "
    "minimalist composition, masterpiece, best quality"
)

NEGATIVE_PROMPT = (
    "3d render, photo, realistic, blur, noise, text, watermark, messy, "
    "colorful rainbow, low quality, distorted anatomy, background"
)

# 根据《职业设计草案_剑修.md》修正后的机械准确提示词
SKILLS = {
    # 3.1 流云刺：位移+单体爆发。提示词侧重：极速突刺的线条
    "skill_liuyunci": "A sharp pale cyan silhouette of a swordsman performing a powerful linear sword thrust, extreme speed lines, ink wash splashes, sharp focus",
    # 3.2 裂空斩：半月剑气。提示词侧重：弯曲的剑气弧线
    "skill_liekongzhan": "A wide glowing golden-cyan crescent sword wave cutting through space, sharp energy edges, fluid ink trail, high contrast",
    # 3.3 灵剑决：悬浮灵剑。提示词侧重：环绕身侧的飞剑集群
    "skill_wanjianjue": "Multiple ethereal spirit swords orbiting in a defensive yet aggressive formation, glowing pale cyan light, ink wash brush texture",
    # 3.4 剑气护体：3柄旋转剑影。提示词侧重：旋转的剑盾
    "skill_jianqihuti": "Three sharp spirit swords rotating rapidly to form a protective circular shield, kinetic motion lines, pale cyan and black ink wash",
    # 3.5 万剑归宗：全屏引导。提示词侧重：密集下落的剑雨
    "skill_wanjianguizong": "An epic rain of countless spirit swords falling from the sky, dense linear streaks, glowing pale cyan energy, dramatic ink wash atmosphere",
    # 3.6 剑阵·诛仙：持续区域伤害。提示词侧重：地面法阵与插地的巨剑
    "skill_zhuxianjianzhen": "A circular Daoist array on the ground with four ethereal giant swords standing at cardinal points, glowing runes, ink splatter texture",
    # 3.7 心剑·无影：引导射线。提示词侧重：笔直的高能光束
    "skill_xinjianwuying": "A focused, ultra-thin beam of pure white and pale cyan energy piercing through an ink-wash cloud, sniper-like precision, sharp edges",
    # 3.8 御剑·回旋：折返飞剑。提示词侧重：旋转的曲线轨迹
    "skill_yujianhuixuan": "A glowing jade-green sword in a curved boomerang-like flying path, circular motion blur, ink trails representing momentum",
    # 3.9 绝影闪：后跳反击。提示词侧重：侧身闪避后的水墨爆发
    "skill_jueyingshan": "A swordsman's silhouette vanishing into ink during a backstep, a single sharp cyan slash counter-attacking from the shadow, ethereal vibe"
}

TALENTS = {
    "talent_mortal": "A handcrafted wooden practice sword entwined with fresh green vines, nature growth theme, simple ink wash outline",
    "talent_dao_source": "A glowing blue Taiji Yin-Yang symbol with a sharp spirit sword core, mystical energy flow, symmetrical ink wash",
    "talent_ascension": "A radiant golden celestial sword pointing upwards towards the heavens, stylized golden clouds, divine ink wash light"
}

def run_gen(name, prompt, category):
    """调用 asset_gen.py 生成图标并移动到对应文件夹"""
    print(f"\n>>> [FLUX2] 正在生成 {category} 图标: {name}...")
    
    full_prompt = f"{prompt}, {STYLE_SUFFIX}"
    
    cmd = [
        sys.executable, ASSET_GEN_SCRIPT,
        "--prompt", full_prompt,
        "--negative", NEGATIVE_PROMPT,
        "--name", name,
        "--width", "512",
        "--height", "512"
    ]
    
    try:
        subprocess.run(cmd, check=True)
        
        # 归档处理
        base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
        src_dir = os.path.join(base_dir, "assets", "textures")
        dst_dir = os.path.join(src_dir, "icons", category)
        
        if not os.path.exists(dst_dir):
            os.makedirs(dst_dir)
            
        # 匹配生成的文件
        found = False
        for filename in os.listdir(src_dir):
            if filename.startswith(name) and filename.endswith(".png"):
                src_path = os.path.join(src_dir, filename)
                dst_path = os.path.join(dst_dir, f"{name}.png")
                
                if os.path.exists(dst_path):
                    os.remove(dst_path)
                
                os.rename(src_path, dst_path)
                print(f"✅ 已存入: {dst_path}")
                found = True
                break
        if not found:
            print(f"❌ 警告: 未找到 {name} 的生成产物")
            
    except subprocess.CalledProcessError as e:
        print(f"🔥 生成 {name} 时出错: {e}")

def main():
    print("======================================================")
    print("   NoMoreDay - 剑修技能图标重构 (基于职业草案 v2)    ")
    print("   风格: 灵动水墨 | 模型: FLUX2                     ")
    print("======================================================")
    
    # 1. 生成技能图标
    for name, prompt in SKILLS.items():
        run_gen(name, prompt, "skills")
        
    # 2. 生成天赋图标
    for name, prompt in TALENTS.items():
        run_gen(name, prompt, "talents")
        
    print("\n>>> 美术资源重构任务已按最新设计文档更新并完成。")

if __name__ == "__main__":
    main()