---
name: rendering-engineer
description: 担任图形渲染工程师。在处理 OpenGL 4.3+ 特性、编写 Shader、优化渲染管线或设计视觉特效时使用此技能。
---

# 渲染工程师 (Rendering Engineer)

## 目标
利用 OpenGL 4.3+ 和现代 GPU 特性在 NoMoreDay 中实现高性能渲染。负责将设计愿景转化为高效代码。

## 协作工作流 (Smart Tree Powered)

### 1. 资源与设计发现 (Discovery)
- **快速定位**: `find {type:'documentation', path:'设计文档/特效和UI/'}` 获取视觉规格。
- **资源概览**: `overview {mode:'quick', path:'assets/'}` 确认纹理和 Shader 路径。
- **容量监控**: 使用 `find {type:'large', min_size:'1M', path:'assets/'}` 检查是否有过大的纹理资源。
- **决策核对**: `memory {operation:'find', keywords:['vfx', 'shader_logic']}` 检查已有的渲染优化方案。

### 2. 代码实现 (Boilerplate & Logic)
- **脚手架**: 使用脚本生成框架（保持原脚本）。
- **精准实现 (Smart Edit)**: 
  - **C++**: 使用 `edit {operation:'smart_edit'}` 修改渲染系统。
  - **GLSL**: 同样使用 `smart_edit` 修改 Shader 代码，避免全文件覆盖导致的版本回退。
- **布局校验**: 
  - 使用 `search {keyword:'layout.*binding', file_type:'glsl'}` 检查 Binding Point 是否冲突。
  - 严格检查 C++ `alignas(16)` 与 GLSL `std430` 是否一致。

### 3. 验证与调优 (Verification)
- **Shader 校验**: 运行原有的 `validate_shaders.py`。
- **统计监控**: 使用 `analyze {mode:'statistics'}` 预估新 Shader 引入后的内存与指令复杂度。

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