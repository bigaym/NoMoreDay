#include "TestCommon.hpp"
#include "game/components/Common.hpp"
#include "game/systems/input/InputSystem.hpp"
#include "game/systems/ui/UIAstrolabe.hpp"
#include "game/systems/ui/UISystem.hpp"

namespace {

class InputUiGateStateGuard {
public:
    InputUiGateStateGuard()
        : m_typing(UISystem::State.isTyping),
          m_skillTree(UISystem::State.showSkillTree),
          m_quantityPopup(UISystem::State.showQuantityPopup),
          m_astrolabeState(NoMoreDay::UIAstrolabe::CaptureVisibilityState()) {
        UISystem::State.isTyping = false;
        UISystem::State.showSkillTree = false;
        UISystem::State.showQuantityPopup = false;
        NoMoreDay::UIAstrolabe::RestoreVisibilityState({false, 0.0f});
    }

    ~InputUiGateStateGuard() {
        UISystem::State.isTyping = m_typing;
        UISystem::State.showSkillTree = m_skillTree;
        UISystem::State.showQuantityPopup = m_quantityPopup;
        NoMoreDay::UIAstrolabe::RestoreVisibilityState(m_astrolabeState);
    }

private:
    bool m_typing;
    bool m_skillTree;
    bool m_quantityPopup;
    NoMoreDay::UIAstrolabe::VisibilityState m_astrolabeState;
};

} // namespace

TEST_CASE("[Unit] InputSystem - Typing clears gameplay actions") {
    entt::registry registry;
    const auto entity = registry.create();
    registry.emplace<PlayerTag>(entity);
    auto& input = registry.emplace<InputComponent>(entity);
    input.moveX = 1.0f;
    input.moveY = -1.0f;
    input.attack = true;
    input.dash = true;

    InputUiGateStateGuard restoreUiGates;
    REQUIRE_FALSE(NoMoreDay::UIAstrolabe::IsVisible(registry, entity));
    REQUIRE_FALSE(UISystem::IsSkillTreeVisible(registry, entity));
    REQUIRE_FALSE(UISystem::IsModalInputCaptured());

    UISystem::State.isTyping = true;

    const Camera2D camera{};
    NoMoreDay::InputSystem::update(registry, camera);

    CHECK(input.moveX == 0.0f);
    CHECK(input.moveY == 0.0f);
    CHECK_FALSE(input.attack);
    CHECK_FALSE(input.dash);
}
