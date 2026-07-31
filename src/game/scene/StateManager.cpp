#include "game/scene/StateManager.hpp"

namespace NoMoreDay {

    StateManager::StateManager(SharedContext& context)
        : m_context(context) {
    }

    StateManager::~StateManager() {
        // Clear all states on destruction
        while (!m_stateStack.empty()) {
            m_stateStack.back()->OnExit();
            m_stateStack.pop_back();
        }
    }

    void StateManager::Update(float dt) {
        ProcessPendingChanges();

        if (m_stateStack.empty()) return;

        // Update from top to bottom
        // We stop as soon as a state returns false (consuming the update)
        for (auto it = m_stateStack.rbegin(); it != m_stateStack.rend(); ++it) {
            if (!(*it)->OnUpdate(dt)) {
                break; 
            }
        }
    }

    void StateManager::Render() {
        if (m_stateStack.empty()) return;

        // Find the lowest state that needs to be rendered.
        // We start from the top and go down. If a state is NOT transparent, 
        // it means it covers everything below it, so that becomes our start point.
        
        auto startIt = m_stateStack.begin(); 
        
        for (auto it = m_stateStack.rbegin(); it != m_stateStack.rend(); ++it) {
            if (!(*it)->IsTransparent()) {
                // Found an opaque state. The iterator is reverse, so 'base' is this one.
                // Convert reverse iterator to forward iterator.
                // standard: base() returns iterator to element *after* the one rbegin points to.
                // so if it points to element N, base points to N+1. 
                // We want to render N, N+1...
                startIt = it.base() - 1; // Points to current element (*it)
                break;
            }
        }

        // Render from the found start point up to the top
        for (auto it = startIt; it != m_stateStack.end(); ++it) {
            (*it)->OnRender();
        }
    }

    void StateManager::PopState() {
        m_pendingList.push_back({PendingChange::Action::Pop, nullptr});
    }

    void StateManager::ClearStates() {
        m_pendingList.push_back({PendingChange::Action::Clear, nullptr});
    }

    void StateManager::ProcessPendingChanges() {
        for (auto& change : m_pendingList) {
            switch (change.action) {
                case PendingChange::Action::Push:
                    if (!m_stateStack.empty()) {
                        m_stateStack.back()->OnSuspend();
                    }
                    m_stateStack.push_back(std::move(change.state));
                    m_stateStack.back()->OnEnter();
                    break;

                case PendingChange::Action::Pop:
                    if (!m_stateStack.empty()) {
                        m_stateStack.back()->OnExit();
                        m_stateStack.pop_back();
                    }
                    if (!m_stateStack.empty()) {
                        m_stateStack.back()->OnWakeup();
                    }
                    break;

                case PendingChange::Action::Clear:
                     while (!m_stateStack.empty()) {
                        m_stateStack.back()->OnExit();
                        m_stateStack.pop_back();
                    }
                    break;
            }
        }
        m_pendingList.clear();
    }

}