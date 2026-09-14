// 本文件由 scripts/gen_skill_contracts.py --gen-specstate 生成，请勿手动编辑。
// 技能 10 效果层 SpecState（POD + 绑定表），由命名 descriptor 驱动。
// 输入：assets/data/skill_specstate/skill_10.json + talent_tree 结构。
#pragma once

#include "game/systems/skill/SpecStateTable.hpp"

#include <array>
#include <cstdint>

namespace NoMoreDay::skills {

// 技能 10 节点常量（talent_tree 全集，节点 id 升序）。
namespace SevenStarSlashNodesGen {
constexpr uint32_t TargetLock = 1000;
constexpr uint32_t CritChance = 1001;
constexpr uint32_t FinalSlash = 1002;
constexpr uint32_t QuickStar = 1003;
constexpr uint32_t ExposedWeakness = 1004;
constexpr uint32_t PoJun = 1005;
constexpr uint32_t ZhanJiang = 1006;
constexpr uint32_t SevenFocus = 1007;
constexpr uint32_t SolitaryStar = 1008;
constexpr uint32_t FlowReturn = 1009;
constexpr uint32_t RevolvingEdge = 1010;
constexpr uint32_t StarScarFollow = 1011;
constexpr uint32_t ChaseStep = 1012;
constexpr uint32_t EndlessSeven = 1013;
constexpr uint32_t DipperHandleReturn = 1014;
constexpr uint32_t VoidTread = 1015;
constexpr uint32_t FallingStarSwitch = 1016;
constexpr uint32_t SwordStepMirage = 1017;
constexpr uint32_t StarVeil = 1018;
constexpr uint32_t GateOfLife = 1019;
constexpr uint32_t LingeringScar = 1020;
constexpr uint32_t PoleStarOrbit = 1021;
constexpr uint32_t Starfall = 1022;
constexpr uint32_t ShatteredConstellation = 1023;
constexpr uint32_t ScarRuin = 1024;
constexpr uint32_t ReturningStep = 1025;
}  // namespace SevenStarSlashNodesGen

// 技能 10 效果层 SpecState（POD，A-01 D-A1）。
struct SevenStarSlashSpecStateGen {
  using PointBinding = SpecPointBinding<SevenStarSlashSpecStateGen>;
  using FlagBinding = SpecFlagBinding<SevenStarSlashSpecStateGen>;

  int targetLockPoints = 0;
  int critChancePoints = 0;
  int finalSlashPoints = 0;
  int quickStarPoints = 0;
  int exposedWeaknessPoints = 0;
  int poJunPoints = 0;
  int zhanJiangPoints = 0;
  int solitaryStarPoints = 0;
  int flowReturnPoints = 0;
  int revolvingEdgePoints = 0;
  int chaseStepPoints = 0;
  int voidTreadPoints = 0;
  int starVeilPoints = 0;
  int gateOfLifePoints = 0;
  int lingeringScarPoints = 0;
  int shatteredConstellationPoints = 0;
  int scarRuinPoints = 0;
  bool sevenFocus = false;
  bool starScarFollow = false;
  bool endlessSeven = false;
  bool fallingStarSwitch = false;
  bool swordStepMirage = false;
  bool returningStep = false;
};

inline constexpr std::array<SevenStarSlashSpecStateGen::PointBinding, 17> kSevenStarSlashPointBindingsGen{{
    {SevenStarSlashNodesGen::TargetLock, &SevenStarSlashSpecStateGen::targetLockPoints},
    {SevenStarSlashNodesGen::CritChance, &SevenStarSlashSpecStateGen::critChancePoints},
    {SevenStarSlashNodesGen::FinalSlash, &SevenStarSlashSpecStateGen::finalSlashPoints},
    {SevenStarSlashNodesGen::QuickStar, &SevenStarSlashSpecStateGen::quickStarPoints},
    {SevenStarSlashNodesGen::ExposedWeakness, &SevenStarSlashSpecStateGen::exposedWeaknessPoints},
    {SevenStarSlashNodesGen::PoJun, &SevenStarSlashSpecStateGen::poJunPoints},
    {SevenStarSlashNodesGen::ZhanJiang, &SevenStarSlashSpecStateGen::zhanJiangPoints},
    {SevenStarSlashNodesGen::SolitaryStar, &SevenStarSlashSpecStateGen::solitaryStarPoints},
    {SevenStarSlashNodesGen::FlowReturn, &SevenStarSlashSpecStateGen::flowReturnPoints},
    {SevenStarSlashNodesGen::RevolvingEdge, &SevenStarSlashSpecStateGen::revolvingEdgePoints},
    {SevenStarSlashNodesGen::ChaseStep, &SevenStarSlashSpecStateGen::chaseStepPoints},
    {SevenStarSlashNodesGen::VoidTread, &SevenStarSlashSpecStateGen::voidTreadPoints},
    {SevenStarSlashNodesGen::StarVeil, &SevenStarSlashSpecStateGen::starVeilPoints},
    {SevenStarSlashNodesGen::GateOfLife, &SevenStarSlashSpecStateGen::gateOfLifePoints},
    {SevenStarSlashNodesGen::LingeringScar, &SevenStarSlashSpecStateGen::lingeringScarPoints},
    {SevenStarSlashNodesGen::ShatteredConstellation, &SevenStarSlashSpecStateGen::shatteredConstellationPoints},
    {SevenStarSlashNodesGen::ScarRuin, &SevenStarSlashSpecStateGen::scarRuinPoints},
}};

inline constexpr std::array<SevenStarSlashSpecStateGen::FlagBinding, 6> kSevenStarSlashFlagBindingsGen{{
    {SevenStarSlashNodesGen::SevenFocus, &SevenStarSlashSpecStateGen::sevenFocus},
    {SevenStarSlashNodesGen::StarScarFollow, &SevenStarSlashSpecStateGen::starScarFollow},
    {SevenStarSlashNodesGen::EndlessSeven, &SevenStarSlashSpecStateGen::endlessSeven},
    {SevenStarSlashNodesGen::FallingStarSwitch, &SevenStarSlashSpecStateGen::fallingStarSwitch},
    {SevenStarSlashNodesGen::SwordStepMirage, &SevenStarSlashSpecStateGen::swordStepMirage},
    {SevenStarSlashNodesGen::ReturningStep, &SevenStarSlashSpecStateGen::returningStep},
}};

inline constexpr SpecStateTable<SevenStarSlashSpecStateGen> kSevenStarSlashTableGen{
    kSevenStarSlashPointBindingsGen, kSevenStarSlashFlagBindingsGen};

}  // namespace NoMoreDay::skills
