"""
NoMoreDay 33 Heavens Rune Generator
Purpose: Generates 33 unique jade rune stones based on Chinese tiers (Yu, Se, Sheng).
         Uses stylized seal script characters and progressive material quality.
Usage: python scripts/gen_runes_33.py
"""
import subprocess
import os

def generate_rune_assets():
    script_path = os.path.join(os.path.dirname(__file__), "asset_gen.py")
    output_width = 1024
    output_height = 1024

    # 33重天代表字 (按阶级排列)
    # Tier 1: 欲界 (曾-飘)
    # Tier 2: 色界 (竺-慧)
    # Tier 3: 圣境 (迦-宝)
    runes = [
        "曾", "完", "童", "育", "举", "夷", "衡", "翳", "阳", "华", "飘", # Tier 1
        "竺", "禁", "崖", "答", "霄", "翁", "极", "晖", "密", "耀", "慧", # Tier 2
        "迦", "昙", "虚", "演", "梵", "腾", "显", "秀", "清", "灵", "宝"  # Tier 3
    ]

    tier_configs = [
        {
            "range": range(0, 11),
            "material": "weathered deep cyan nephrite stone, semi-opaque, with natural mineral veins and rough stony edges",
            "glow": "soft pale green",
            "quality": "lower tier"
        },
        {
            "range": range(11, 22),
            "material": "polished translucent emerald jade, ice-like texture, vibrant green color, smooth rounded edges",
            "glow": "bright spiritual emerald",
            "quality": "middle tier"
        },
        {
            "range": range(22, 33),
            "material": "radiant white mutton-fat jade, ethereal milky texture, glowing with inner divinity, pristine sharp edges",
            "glow": "golden divine light",
            "quality": "high tier"
        }
    ]

    print(f"--- Starting Rune Generation: 33 Heavens Series ---")

    for i, char in enumerate(runes):
        # 确定当前符文所属的 Tier
        config = next(t for t in tier_configs if i in t["range"])
        
        asset_name = f"rune_{i:02d}_{char}"
        
        # 构建提示词: 强调倾斜角度和篆书刻字
        full_prompt = (
            f"A single mystical jade rune stone, {config['material']}. "
            f"The stone is tilted at a 45-degree angle. "
            f"Deeply engraved with the ancient Chinese seal script character '{char}' on its surface, "
            f"the character is {config['glow']} and glowing. "
            f"Isolated on a plain white background, 2D game icon style, professional digital art, masterpiece."
        )
        
        # 负面提示词
        negative_prompt = "human, hand, flat, top-down view, blurry, low quality, 3d render, photo, multiple items"

        print(f"Generating Rune {i+1}/33: {asset_name} ({config['quality']})")
        
        cmd = [
            "python", script_path,
            "--prompt", full_prompt,
            "--negative", negative_prompt,
            "--name", asset_name,
            "--width", str(output_width),
            "--height", str(output_height)
        ]

        try:
            # 执行生成
            subprocess.run(cmd, check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error generating {asset_name}: {e}")

    print("\n--- Rune Generation Complete! ---")

if __name__ == "__main__":
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    generate_rune_assets()
