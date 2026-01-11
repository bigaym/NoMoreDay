#pragma once

#include <vector>
#include <memory>
#include <functional>
#include "engine/scene/State.hpp"
#include "app/SharedContext.hpp"

namespace NoMoreDay {

    class StateManager {
    public:
        explicit StateManager(SharedContext& context);
        ~StateManager();

        void Update(float dt);
        void Render();

        // State Management
        template<typename T, typename... Args>
        void PushState(Args&&... args);

        void PopState();
        
        template<typename T, typename... Args>
        void ChangeState(Args&&... args); // Pop current and push new

        void ClearStates();

        bool IsEmpty() const { return m_stateStack.empty(); }
        SharedContext& GetContext() { return m_context; }

    private:
        struct PendingChange {
            enum class Action { Push, Pop, Clear };
            Action action;
            std::unique_ptr<IState> state;
        };

        void ProcessPendingChanges();

        SharedContext& m_context;
        std::vector<std::unique_ptr<IState>> m_stateStack;
        std::vector<PendingChange> m_pendingList;
    };

    // Template implementation
    template<typename T, typename... Args>
    void StateManager::PushState(Args&&... args) {
        static_assert(std::is_base_of<IState, T>::value, "T must derive from IState");
        auto newState = std::make_unique<T>(*this, m_context, std::forward<Args>(args)...);
        m_pendingList.push_back({PendingChange::Action::Push, std::move(newState)});
    }

    template<typename T, typename... Args>
    void StateManager::ChangeState(Args&&... args) {
        static_assert(std::is_base_of<IState, T>::value, "T must derive from IState");
        // For ChangeState, we can just Pop then Push. 
        // Note: Logic order in ProcessPendingChanges matters.
        // Or specific Action::Change. Let's use Pop then Push logic manually or add Action.
        // Simple approach: Add a Clear (if we want to replace all) or just Pop + Push.
        // For "Change" (Replace top), let's just queue Pop then Push.
        m_pendingList.push_back({PendingChange::Action::Pop, nullptr});
        
        auto newState = std::make_unique<T>(*this, m_context, std::forward<Args>(args)...);
        m_pendingList.push_back({PendingChange::Action::Push, std::move(newState)});
    }
}
