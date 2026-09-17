#pragma once

// 技能 10（七星连斩）在数据烘焙层与行为层之间共享的字面量。
// 刻意保持零依赖：烘焙层只需要这里的常量，不应为此拖入行为层的重型头
// （SkillSystem.hpp / SevenStarSlashSpecState.gen.hpp 等）。
namespace NoMoreDay::skills::seven_star_shared {

// 技能 10 判定半径基准：技能级 params 默认值与交付档案回退共用的单源常量，
// 防止 Baker case 10 与行为层回退各写一份字面量而漂移。
inline constexpr float kSevenStarSlashBaseRadius = 96.0f;

} // namespace NoMoreDay::skills::seven_star_shared
