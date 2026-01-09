# Implementation Plan - GPU Skill VFX (水墨修仙特效优化)

本计划旨在分阶段实现 9 个技能的水墨风格视觉表现，并确保在高负载下的性能表现。

## Phase 1: 基础建设与水墨逻辑 (Foundation)
- [x] **Task: 粒子着色器扩展** [commit: <commit_hash>]
    - 分析 `assets/shaders/particle.compute`，增加对“水墨感”（边缘羽化、淡入淡出、非线性缩放）的支持。
- [~] **Task: 特效辅助工具类实现**
    - 在 `src/systems/GPUParticleSystem` 中实现 `InkEffectHelper` 静态方法，用于生成淡雅墨迹、墨迹溅射和金色流光的粒子参数包。
- [ ] Task: Conductor - User Manual Verification 'Phase 1: Foundation' (Protocol in workflow.md)

## Phase 2: 核心攻击与位移特效 (Offensive & Movement - IDs 1, 2, 9)
- [x] **Task: 实现流云刺 (ID 1) 视觉效果** [commit: <commit_hash>]
    - 增加突刺路径的淡墨残影和强化版的金芒爆发。
- [x] **Task: 实现裂空斩 (ID 2) 视觉效果** [commit: <commit_hash>]
    - 为扇面剑气添加墨痕质感，并实现穿透时的水墨溅射。
- [x] **Task: 实现绝影闪 (ID 9) 视觉效果** [commit: <commit_hash>]
    - 实现浓墨残影和原地留下的金色剑痕。
- [x] Task: Conductor - User Manual Verification 'Phase 2: Core Offensive' (Protocol in workflow.md)

## Phase 3: 战术与范围技能特效 (Tactical & Area - IDs 3, 4, 6, 8)
- [ ] **Task: 实现御剑·回旋 (ID 8) 视觉效果**
    - 为飞剑增加回旋墨环拖尾。
- [ ] **Task: 实现万剑诀 (ID 3) 视觉效果**
    - 为浮游灵剑添加细长墨纹。
- [ ] **Task: 实现剑气护体 (ID 4) 视觉效果**
    - 实现环绕的半透明墨色护罩及格挡时的墨渍反馈。
- [ ] **Task: 实现剑阵·诛仙 (ID 6) 视觉效果**
    - 绘制阵法墨韵边界及内部升腾的墨烟粒子。
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Tactical and Area' (Protocol in workflow.md)

## Phase 4: 引导与终极技能特效 (Channeling & Ultimates - IDs 5, 7)
- [ ] **Task: 实现万剑归宗 (ID 5) 视觉效果**
    - 设计全屏乱序的草书式墨痕特效。
- [ ] **Task: 实现心剑·无影 (ID 7) 视觉效果**
    - 实现近乎透明的墨丝线条特效。
- [ ] **Task: 强化状态全局优化**
    - 统一校验所有技能在 `is_empowered` 状态下的“金韵墨痕”表现。
- [ ] Task: Conductor - User Manual Verification 'Phase 4: Channeling and Ultimate' (Protocol in workflow.md)

## Phase 5: 性能优化与最终调优 (Performance & Polish)
- [ ] **Task: 压力测试**
    - 在 10,000 实体同屏且频繁释放技能的情况下，验证 GPU 粒子系统的稳定性。
- [ ] **Task: 视觉一致性微调**
    - 调整粒子透明度衰减曲线，确保整体观感符合“淡雅”的风格定位。
- [ ] Task: Conductor - User Manual Verification 'Phase 5: Final Polish' (Protocol in workflow.md)
