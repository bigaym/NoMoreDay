# Specification: Blade Ward Interception

## Core Logic

### Component
`BladeWardComponent`:
- `sword_count`: Current number of active swords.
- `interception_chance`: Probability (0.0-1.0) to intercept a projectile.
- `is_solidified`: If true, interception does not consume a sword.

### Damage Pipeline Integration
Located in `DamagePipeline::Calculate` and `CalculateBatch`.

**Flow**:
1. Detect `Tag::Projectile` in `hit_tags`.
2. Check if defender has `BladeWardComponent`.
3. Check `sword_count > 0`.
4. Roll RNG against `interception_chance`.
5. **On Success**:
   - Set `total_final_damage = 0`.
   - Clear `final_pool`.
   - If `!is_solidified`, `sword_count--`.
   - Log "Blade Ward: Projectile intercepted!".

## Test Verification
- **Functional Test**: Verify `sword_count` decreases after successful hits (if not solidified).
- **Integration Test**: Ensure multiple projectiles are handled correctly in batch processing.
