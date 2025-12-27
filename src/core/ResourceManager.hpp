#pragma once
#include <string>
#include <unordered_map>
#include <entt/entt.hpp>
#include "raylib.h"

class ResourceManager {
public:
    ResourceManager() = default;
    ~ResourceManager();

    // Loads a texture from disk if not already loaded. 
    // Usage: loadTexture("sword"_hs, "path/...")
    Texture2D loadTexture(entt::id_type id, const std::string& path);
    
    // Gets an already loaded texture. Returns empty texture if not found.
    Texture2D getTexture(entt::id_type id);

    void unloadAll();

private:
    std::unordered_map<entt::id_type, Texture2D> m_textures;
};
