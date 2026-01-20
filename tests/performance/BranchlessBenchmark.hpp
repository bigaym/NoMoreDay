#pragma once
#include "doctest.h"
#include "core/utils/Branchless.hpp"
#include <vector>
#include <random>
#include <chrono>
#include <cstdio>

using namespace NoMoreDay::utils;

TEST_CASE("Performance: Branchless Logic Benchmark") {
    // Setup random data
    constexpr size_t N = 100000;
    std::vector<int> conditions(N);
    std::vector<float> values(N);
    std::vector<float> multipliers(N);
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> bool_dist(0, 1);
    std::uniform_real_distribution<float> val_dist(10.0f, 100.0f);
    std::uniform_real_distribution<float> mult_dist(1.5f, 3.0f);
    
    for (size_t i = 0; i < N; ++i) {
        conditions[i] = bool_dist(rng);
        values[i] = val_dist(rng);
        multipliers[i] = mult_dist(rng);
    }
    
    // Warmup
    {
        float total = 0.0f;
        for (size_t i = 0; i < N; ++i) total += values[i];
        (void)total;
    }

    long long duration_branch = 0;
    float result_branch = 0.0f;
    {
        auto start = std::chrono::high_resolution_clock::now();
        float total = 0.0f;
        for (int iter = 0; iter < 100; ++iter) { // Repeated iterations
            for (size_t i = 0; i < N; ++i) {
                float dmg = values[i];
                if (conditions[i]) {
                    dmg *= multipliers[i];
                }
                total += dmg;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        duration_branch = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        result_branch = total;
    }
    
    long long duration_select = 0;
    float result_select = 0.0f;
    {
        auto start = std::chrono::high_resolution_clock::now();
        float total = 0.0f;
        for (int iter = 0; iter < 100; ++iter) {
            for (size_t i = 0; i < N; ++i) {
                float dmg = values[i];
                float factor = SelectF(conditions[i], multipliers[i], 1.0f);
                dmg *= factor;
                total += dmg;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        duration_select = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        result_select = total;
    }
    
    printf("\n[Benchmark] Branchless vs Branching (N=%zu, 100 iters)\n", N);
    printf("  Branching: %lld us\n", duration_branch);
    printf("  SelectF:   %lld us (%.2fx speedup)\n", duration_select, (float)duration_branch / duration_select);
}
