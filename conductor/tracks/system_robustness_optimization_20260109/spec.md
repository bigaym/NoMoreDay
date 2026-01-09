# Specification: System Robustness & Performance Optimization

## 1. Overview
This track focuses on three critical pillars for the long-term health of NoMoreDay: 
1. **User Feedback:** Visualizing stat caps (Soft Caps) to prevent player confusion.
2. **Balance Tuning:** Reducing high-impact 'More' multipliers in the Astrolabe (Keystones) to normalize damage scaling.
3. **Performance Scalability:** Optimizing the `StatsSystem` to support high-refresh-rate environments (up to 240 FPS) and large entity counts.

## 2. Functional Requirements

### 2.1 Stat Aggregation & UI Visualization
- **Target Stats:** Implement "Cap Awareness" for:
    - Resistances (Elemental/Physical)
    - Cooldown Reduction (CDR)
    - Attack/Cast Speed
    - Movement Speed
- **UI Feedback:** Update stat tooltips/display to indicate when a stat is capped. 
    - *Example:* "Fire Resistance: 75% (Capped from 95%)".

### 2.2 Astrolabe Keystone Tuning
- **Keystone Adjustment:** Directly reduce the 'More' damage multipliers for dominant nodes.
    - **Target:** `心剑合一` (ID: 301) reduced from 50% More damage to 15% More damage.
    - **Audit:** Review other high-impact Keystones (e.g., `剑心通明`) for similar numerical adjustments.

### 2.3 StatsSystem Performance Optimization
- **Frequency Reduction:** Audit and enforce strict `StatsDirty` flag usage. Ensure recalculation only occurs during discrete events (Equipment change, Buff addition/removal).
- **Hot-path Optimization:** 
    - Minimize string-based lookups during `Recalculate`.
    - Investigate caching mechanisms for `GetStatWithTags` results if the underlying modifiers haven't changed.
    - Ensure the system can handle updates efficiently enough to support 240 FPS targets.

## 3. Acceptance Criteria
- [ ] UI correctly displays the difference between "Raw Value" and "Effective Capped Value" for targeted stats.
- [ ] `心剑合一` damage multiplier is verified as 1.15x in the `DamagePipeline`.
- [ ] Performance profiling shows a reduction in total frame time spent in `StatsSystem::Recalculate` under heavy load (1000+ active entities with shifting buffs).
- [ ] No regression in stat calculation accuracy.

## 4. Out of Scope
- Implementing new Keystones or Skill Talents.
- Replacing the `entt` ECS architecture.
- Full UI redesign (only incremental feedback updates for stats).
