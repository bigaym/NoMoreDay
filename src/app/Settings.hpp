#pragma once
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <filesystem>

namespace NoMoreDay {

    struct GameSettings {
        float cameraZoom = 1.5f;
        int targetFPS = 0;  // 0 for Unlimited FPS

        void Save(const std::string& filePath = "settings.json") {
            nlohmann::json j;
            j["cameraZoom"] = cameraZoom;
            j["targetFPS"] = targetFPS;
            
            std::ofstream file(filePath);
            if (file.is_open()) {
                file << j.dump(4);
            }
        }

        void Load(const std::string& filePath = "settings.json") {
            if (!std::filesystem::exists(filePath)) {
                Save(filePath);
                return;
            }

            try {
                std::ifstream file(filePath);
                nlohmann::json j;
                file >> j;

                if (j.contains("cameraZoom")) cameraZoom = j["cameraZoom"].get<float>();
                if (j.contains("targetFPS")) targetFPS = j["targetFPS"].get<int>();
            } catch (...) {
                // Fallback to defaults on error
            }
        }
    };

}
