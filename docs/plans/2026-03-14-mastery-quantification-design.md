# Mastery Skill Quantification Design (Skill 10, 11, 12)

Date: 2026-03-14
Status: Approved

## Overview
This design document outlines the quantification of qualitative descriptions in three specialization skill trees (Skill 10: Sword Saint, Skill 11: Heavenly Sword, Skill 12: Demon Blade). The goal is to replace vague terms like "increases damage" with concrete numerical values.

## Strategy
1.  **Static Padding:** All quantitative values will be hardcoded directly into the `desc_key` in `assets/data/mastery_skill_trees.json`.
2.  **Linear Growth:** All modifiers will follow a fixed increment per level (e.g., 10% → 20% → 30% → 40%).
3.  **Cooldown Speed:** Qualitative references to "cooldown reduction" will be quantified as "Cooldown Speed" modifiers.

## Numerical Standards
- **Standard Buff (General):** +10% / +20% / +30% / +40%
- **Core Damage (More):** +15% / +30% / +45% / +60%
- **Attack/Cast Speed:** +5% / +10% / +15% / +20%
- **Cooldown Speed:** +10% / +20% / +30% / +40%
- **Area/Range:** +10% / +20% / +30% / +40%
- **Functional Probability:** +5% / +10% / +15% / +20%
- **Lifesteal/Recovery Efficiency:** +2% / +4% / +6% / +8%

## Scope & Key Nodes

### Skill 10: Sword Saint
- Node 1001: Attack Speed +5%/10%/15%/20%
- Node 1003: Cooldown Speed +10%/20%/30%/40%, Cast Speed/Lock Reduction +15%/30%/45%/60%
- Node 1007: 7th Hit More Damage +20%/40%/60%/80%

### Skill 11: Heavenly Sword
- Node 1101: Sword Domain Radius +10%/20%/30%/40%
- Node 1105: Crit Chance per active sword +5%/10%/15%/20%
- Node 1110: Attack Frequency +10%/20%/30%/40%

### Skill 12: Demon Blade
- Node 1201: Blood Sea Damage Over Time +15%/30%/45%/60%
- Node 1204: Life Gain on Hit +2/4/6/8
- Node 1209: Lifesteal Efficiency per Thirst stack +2%/4%/6%/8%

## Implementation Workflow
1.  **Batch Scan:** Identify all nodes in Skills 10, 11, 12 in `assets/data/mastery_skill_trees.json`.
2.  **Update `desc_key`:** Replace qualitative strings with quantified strings using the linear templates.
3.  **Update `stat_modifiers`:** Populate the corresponding modifier arrays with the linear values.
4.  **Verification:** Validate JSON schema and run build to ensure no regression.
