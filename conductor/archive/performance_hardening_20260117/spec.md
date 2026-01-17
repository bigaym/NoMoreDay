# Performance Hardening Specification

## 1. Stats Cache Management
- **Problem**: `StatsSystem::s_tagStatCache` grew indefinitely because it didn't listen to entity destruction.
- **Solution**: 
  - Added `StatsSystem::Initialize(registry)` and `Shutdown(registry)`.
  - Connected `registry.on_destroy<CombatStats>()` to `ClearCache`.
  - Integrated into `Game::init` and `Game::cleanup`.

## 2. Zero-Allocation Loops
- **SkillSystem**:
  - Replaced local `std::vector<entt::entity>` with a static `s_entities_scratch`.
  - Reused buffer across `Update`, `UpdateStates`, `MindBlade`, and `PhantomFlash` logic.
- **RenderSystem**:
  - Replaced `flowSystem.DownloadFlowField()` (which returned a new vector) with `SyncToCPU()` and `GetFlowFieldCPU()` (const reference to internal vector).

## 3. String Optimization (DOD)
- **HashUtils**: Implemented `constexpr uint32_t Hash(std::string_view)` and user-defined literal `_hash`.
- **ItemComponent**: Added `setNameHash`. Populated automatically in `from_json` or factory.
- **StatsSystem**: 
  - Set Bonus counting now uses `unordered_map<uint32_t, int>` (hash-keyed).
  - Reduced complexity from string hashing/comparison to integer comparison.
- **Astrolabe**: Introduced `node->conversions` vector in `AstrolabeNode` to replace string-based effect parsing (e.g., "IntToCritMult:0.1").

## 4. Reactive State Updates
- **Sword Intent**:
  - Skill hits, empowered consumptions, and decay now trigger `registry.get_or_emplace<StatsDirty>(entity)`.
  - Ensures dynamic stats (e.g. "Attack Speed per 10 Stacks") update instantly.
