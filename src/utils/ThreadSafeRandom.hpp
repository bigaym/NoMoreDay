#pragma once

#include <random>
#include <type_traits>

namespace NoMoreDay::utils {

class ThreadSafeRandom {
public:
    // Returns a float between [min, max]
    static float GetFloat(float min, float max) {
        return std::uniform_real_distribution<float>(min, max)(GetEngine());
    }

    // Returns a float between [0, 1]
    static float GetFloat01() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(GetEngine());
    }

    // Returns an int between [min, max] (inclusive)
    static int GetInt(int min, int max) {
        return std::uniform_int_distribution<int>(min, max)(GetEngine());
    }

    // Helper to replace Raylib's GetRandomValue(min, max)
    // Raylib's implementation is inclusive [min, max]
    static int GetRandomValue(int min, int max) {
        return GetInt(min, max);
    }

private:
    static std::mt19937& GetEngine() {
        // Initialized once per thread
        thread_local std::mt19937 engine([]() {
            std::random_device rd;
            return rd();
        }());
        return engine;
    }
};

} // namespace NoMoreDay::utils
