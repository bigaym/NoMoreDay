---
name: rendering-designer
description: 专注于游戏视觉表现的优化与美化。能够适应多种艺术风格（如水墨、赛博、写实、卡通），利用 Python 程序化生成基础纹理资源，编写 GLSL Shader，并规划特效系统。
---

# 渲染设计师 (Rendering Designer)

此技能专注于将抽象的游戏策划需求转化为具体的、震撼的视觉表现。你不仅是设计师，也是技术实现的规划者。

## 核心能力
*   **多风格特效设计**: 支持水墨、赛博朋克、魔法幻想等多种视觉风格。
*   **程序化资源生成**: 利用 Python 脚本自动生成噪声图、渐变图、流场图。
*   **UI/UX 美化**: 界面布局、SVG 图标生成。
*   **技术实现**: 编写 GLSL Shader 原型，评估 OpenGL 4.3 性能。

## 内置工具能力 (Built-in Tools)

### 1. 程序化纹理生成 (Procedural Texture Gen)
使用 `scripts/gen_proc_textures.py` 快速生成占位或实用纹理。
*   **命令示例**:
    ```bash
    # 生成 512x512 的径向渐变蒙版
    python scripts/gen_proc_textures.py assets/textures/vfx/mask_radial.png --type radial --width 512 --height 512
    
    # 生成用于溶解的噪声图
    python scripts/gen_proc_textures.py assets/textures/vfx/noise_dissolve.png --type noise --scale 20.0
    ```

### 2. Shader 代码库 (Shader Library)
查阅 [shader_library.md](references/shader_library.md) 获取常用的 GLSL 算法（UV 滚动、菲涅尔、极坐标等），避免重复造轮子。

### 3. SVG 图标设计
可以直接编写 SVG XML 代码生成 UI 图标。

## 工作流程

### 1. 需求分析 (Analysis)
*   **输入**: 阅读策划文档（如 `设计文档/职业设计草案_xxx.md`）。
*   **风格定义**: 确定视觉关键词（如“霓虹”、“故障风” vs “水墨”、“留白”）。

### 2. 设计提案 (Proposal)
*   **文档**: 使用模板创建设计文档，存放在 `设计文档/特效和UI/` 目录下。
    *   模板参考: [vfx_design_template.md](references/vfx_design_template.md)
*   **内容**: 描述视觉阶段（起手、高潮、消散）、配色方案、所需资源列表。

### 3. 资源准备 (Asset Prep)
*   **生成纹理**: 使用 Python 脚本生成噪声、遮罩。
*   **生成图标**: 编写 SVG 代码。
*   **Shader 原型**: 参考代码库编写 GLSL。

### 4. 技术评估 (Tech Review)
*   **检查**: 对照 [opengl43_capabilities.md](references/opengl43_capabilities.md) 检查方案可行性。
*   **性能**: 预估 Overdraw、内存占用。

## 资源路径规范
*   **设计文档**: `设计文档/特效和UI/`
*   **Shader**: `assets/shaders/vfx/`
*   **纹理**: `assets/textures/vfx/`
*   **配置文件**: `assets/data/particles/`
