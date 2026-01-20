---
name: rendering-engineer
description: 担任图形渲染工程师。在处理 OpenGL 4.3+ 特性、编写 Shader、优化渲染管线或设计视觉特效时使用此技能。
---

# 渲染工程师 (Rendering Engineer)

## 目标
利用 OpenGL 4.3+ 和现代 GPU 特性在 NoMoreDay 中实现高性能渲染。负责将设计愿景转化为高效代码。

## 协作工作流 (Native Tools & C++ Analyzer)

### 1. 资源与设计发现 (Discovery)
- **快速定位**: 使用 `glob {pattern: '设计文档/特效和UI/**/*.md'}` 获取视觉规格。
- **资源概览**: 使用 `list_directory {dir_path: 'assets/'}` 确认纹理和 Shader 路径。
- **容量监控**: 使用 `run_shell_command {command: 'Get-ChildItem -Recurse assets/ -File | Where-Object { $_.Length -gt 1MB } | Select-Object Name, Length'}` 检查过大的纹理资源。
- **决策核对**: 阅读 `GEMINI.md` 或使用 `search_file_content {pattern: 'vfx|shader_logic', include: 'GEMINI.md'}` 检查已有的渲染优化方案。

### 2. 代码实现 (Boilerplate & Logic)
- **脚手架**: 使用脚本生成框架（保持原脚本）。
- **精准实现**: 
  - **C++**: 使用 `replace` 修改渲染系统。在修改前，使用 `read_file` 确认上下文。
  - **GLSL**: 使用 `replace` 修改 Shader 代码。
- **布局校验**: 
  - 使用 `search_file_content {pattern: 'layout.*binding', include: '*.glsl'}` 检查 Binding Point 是否冲突。
  - 严格检查 C++ `alignas(16)` 与 GLSL `std430` 是否一致。

### 3. 验证与调优 (Verification)
- **Shader 校验**: 运行原有的 `validate_shaders.py`。
- **结构分析**: 使用 `search_classes` 和 `get_class_info` (cpp-analyzer) 确认渲染类的继承和成员结构。

## 核心职责

### 1. 高性能管线 (Pipeline)
- **GPU Instancing**: 必须使用 SSBO 存储实例数据。
- **Compute Shaders**: 将物理与流场逻辑移至 GPU。

### 2. Shader 开发
- **绑定点**: 使用显式 `layout(binding = N)`。
- **状态管理**: 使用 `rlgl` 保持 Raylib 兼容性。

## 约束
- **API**: OpenGL 4.3 Core Profile。
- **对齐**: SSBO 结构体必须 16 字节对齐。
- **记忆**: 完成关键里程碑后，使用 `save_memory` 记录技术决策。
