#pragma once
#include <cstdint>
#include <random>
#include <string_view>
#include <limits>

namespace NoMoreDay::Utils {

class UUID {
public:
    // [Runtime] 生成随机 UUID (64位)
    // 使用 mt19937_64 算法，适用于动态生成的物品、怪物等
    static uint64_t generate() {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis(1, std::numeric_limits<uint64_t>::max()); // 0 保留为无效
        
        uint64_t id = dis(gen);
        return (id == 0) ? generate() : id; // 确保不返回 0
    }

    // [Constexpr] 编译期生成确定性 UUID (基于 FNV-1a 哈希算法)
    // 适用于静态定义的实体 ID，例如: UUID::from("Player")
    static constexpr uint64_t from(std::string_view str) {
        uint64_t hash = 14695981039346656037ULL;
        for (char c : str) {
            hash ^= static_cast<uint64_t>(c);
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

} // namespace NoMoreDay::Utils