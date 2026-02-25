# Combat Endgame Linker — Specification

> Track ID: `combat_endgame_linker_20260225`  
> Series: CS-M3-02 | Priority: P2 | Milestone: M3

---

## 1. Overview

建立 Endgame 词缀到战斗合同的映射框架，确保新内容通过合同扩展而非临时特判接入。

## 2. Requirements

- 定义 `EndgameModifierContract` 接口。
- 每种词缀声明对伤害/防御/状态的影响参数。
- 映射通过配置文件驱动。
- 至少 5 种词缀验证：如 Extra Damage、Resistance Reduction、Ailment Amplification 等。

## 3. Acceptance Criteria

- [ ] 新词缀通过合同扩展接入。
- [ ] 1 轮回归内可定位异常源。
- [ ] 至少 5 种 Endgame 词缀验证通过。
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。
