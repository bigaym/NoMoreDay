#pragma once
#include <entt/entity/registry.hpp>

namespace NoMoreDay {

class XPAwardingSystem {
public:
    /*
     * @brief 处理被击杀的实体并向玩家奖励经验。
     * @param registry EnTT 注册表。
     */
    static void update(entt::registry& registry);

    /*
     * @brief 重置系统状态（清空待销毁队列）。
     * 应在游戏状态退出或进入时调用，防止跨 Session 指针残留。
     */
    static void Reset();
};

} // namespace NoMoreDay
