#include "SkillBehaviorRegistry.hpp"
#include "core/logging/Logger.hpp"

namespace NoMoreDay {

std::unordered_map<uint32_t, SkillBehaviorRegistry::CastFunc>& SkillBehaviorRegistry::GetCastMap() {
    static std::unordered_map<uint32_t, CastFunc> s_cast_map;
    return s_cast_map;
}

std::unordered_map<uint32_t, SkillBehaviorRegistry::TickFunc>& SkillBehaviorRegistry::GetTickMap() {
    static std::unordered_map<uint32_t, TickFunc> s_tick_map;
    return s_tick_map;
}

std::unordered_map<uint32_t, SkillBehaviorRegistry::EndFunc>& SkillBehaviorRegistry::GetEndMap() {
    static std::unordered_map<uint32_t, EndFunc> s_end_map;
    return s_end_map;
}

std::unordered_map<uint32_t, SkillBehaviorRegistry::HitFunc>& SkillBehaviorRegistry::GetHitMap() {
    static std::unordered_map<uint32_t, HitFunc> s_hit_map;
    return s_hit_map;
}

void SkillBehaviorRegistry::RegisterCast(uint32_t skill_id, CastFunc func) {
    GetCastMap()[skill_id] = func;
    LOG_DEBUG("SkillBehaviorRegistry: Registered OnCast for skill {}", skill_id);
}

void SkillBehaviorRegistry::RegisterTick(uint32_t skill_id, TickFunc func) {
    GetTickMap()[skill_id] = func;
    LOG_DEBUG("SkillBehaviorRegistry: Registered OnTick for skill {}", skill_id);
}

void SkillBehaviorRegistry::RegisterEnd(uint32_t skill_id, EndFunc func) {
    GetEndMap()[skill_id] = func;
    LOG_DEBUG("SkillBehaviorRegistry: Registered OnEnd for skill {}", skill_id);
}

void SkillBehaviorRegistry::RegisterHit(uint32_t skill_id, HitFunc func) {
    GetHitMap()[skill_id] = func;
    LOG_DEBUG("SkillBehaviorRegistry: Registered OnHit for skill {}", skill_id);
}

SkillBehaviorRegistry::CastFunc SkillBehaviorRegistry::GetCast(uint32_t skill_id) {
    auto& map = GetCastMap();
    auto it = map.find(skill_id);
    return (it != map.end()) ? it->second : nullptr;
}

SkillBehaviorRegistry::TickFunc SkillBehaviorRegistry::GetTick(uint32_t skill_id) {
    auto& map = GetTickMap();
    auto it = map.find(skill_id);
    return (it != map.end()) ? it->second : nullptr;
}

SkillBehaviorRegistry::EndFunc SkillBehaviorRegistry::GetEnd(uint32_t skill_id) {
    auto& map = GetEndMap();
    auto it = map.find(skill_id);
    return (it != map.end()) ? it->second : nullptr;
}

SkillBehaviorRegistry::HitFunc SkillBehaviorRegistry::GetHit(uint32_t skill_id) {
    auto& map = GetHitMap();
    auto it = map.find(skill_id);
    return (it != map.end()) ? it->second : nullptr;
}

// External registration functions to force linkage
namespace skills {
    void RegisterFlowingThrust();
    void RegisterRendingWave();
    void RegisterBladeFormation();
    void RegisterBladeWard();
    void RegisterInfiniteBlades();
    void RegisterSwordArray();
    void RegisterMindBlade();
    void RegisterBladeBoomerang();
    void RegisterPhantomFlash();
    void RegisterSevenStarSlash();
    void RegisterHeavenlySwordDescent();
    void RegisterBloodSea();
}

void SkillBehaviorRegistry::Initialize() {
    LOG_INFO("SkillBehaviorRegistry: Initializing skill behavior system...");
    
    // Explicitly call a function from each behavior translation unit.
    // This forces the linker to include the object file, ensuring that
    // the static initializers (which call RegisterCast/RegisterHit) are executed.
    skills::RegisterFlowingThrust();
    skills::RegisterRendingWave();
    skills::RegisterBladeFormation();
    skills::RegisterBladeWard();
    skills::RegisterInfiniteBlades();
    skills::RegisterSwordArray();
    skills::RegisterMindBlade();
    skills::RegisterBladeBoomerang();
    skills::RegisterPhantomFlash();
    skills::RegisterSevenStarSlash();
    skills::RegisterHeavenlySwordDescent();
    skills::RegisterBloodSea();

    LOG_INFO("SkillBehaviorRegistry: Handshake completed for all skill behaviors.");
}

void SkillBehaviorRegistry::Clear() {
    GetCastMap().clear();
    GetTickMap().clear();
    GetEndMap().clear();
    GetHitMap().clear();
    LOG_INFO("SkillBehaviorRegistry: All skill behaviors cleared");
}

bool SkillBehaviorRegistry::HasBehavior(uint32_t skill_id) {
    return GetCastMap().count(skill_id) > 0 || 
           GetTickMap().count(skill_id) > 0 ||
           GetEndMap().count(skill_id) > 0 ||
           GetHitMap().count(skill_id) > 0;
}

} // namespace NoMoreDay
