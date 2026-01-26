#include "doctest.h"
#include "core/utils/Branchless.hpp"
#include <cmath>

using namespace NoMoreDay::utils;

TEST_CASE("[Unit] Branchless - BoolToMask") {
    CHECK(BoolToMask(true) == -1);
    CHECK(BoolToMask(false) == 0);
    CHECK(static_cast<uint32_t>(BoolToMask(true)) == 0xFFFFFFFF);
    CHECK(static_cast<uint32_t>(BoolToMask(false)) == 0x00000000);
}

TEST_CASE("[Unit] Branchless - Select Integer") {
    CHECK(Select(true, 10, 20) == 10);
    CHECK(Select(false, 10, 20) == 20);
    
    CHECK(Select(true, -5, 5) == -5);
    CHECK(Select(false, -5, 5) == 5);
    
    CHECK(Select(true, 0, 100) == 0);
    CHECK(Select(false, 0, 100) == 100);
}

TEST_CASE("[Unit] Branchless - Select Float") {
    const float epsilon = 1e-5f;
    
    CHECK(std::abs(SelectF(true, 10.0f, 20.0f) - 10.0f) < epsilon);
    CHECK(std::abs(SelectF(false, 10.0f, 20.0f) - 20.0f) < epsilon);
    
    CHECK(std::abs(SelectF(true, -5.5f, 5.5f) - -5.5f) < epsilon);
    CHECK(std::abs(SelectF(false, -5.5f, 5.5f) - 5.5f) < epsilon);
}

TEST_CASE("[Unit] Branchless - MultFactor") {
    const float epsilon = 1e-5f;
    
    CHECK(std::abs(MultFactor(true, 2.5f) - 2.5f) < epsilon);
    CHECK(std::abs(MultFactor(false, 2.5f) - 1.0f) < epsilon);
    
    CHECK(std::abs(MultFactor(true, 0.0f) - 0.0f) < epsilon); // Clear damage
    CHECK(std::abs(MultFactor(false, 0.0f) - 1.0f) < epsilon); // Keep damage
}

TEST_CASE("[Unit] Branchless - AddFactor") {
    const float epsilon = 1e-5f;
    
    CHECK(std::abs(AddFactor(true, 10.0f) - 10.0f) < epsilon);
    CHECK(std::abs(AddFactor(false, 10.0f) - 0.0f) < epsilon);
}

TEST_CASE("[Unit] Branchless - ClampF") {
    const float epsilon = 1e-5f;
    
    CHECK(std::abs(ClampF(5.0f, 0.0f, 10.0f) - 5.0f) < epsilon);
    CHECK(std::abs(ClampF(-5.0f, 0.0f, 10.0f) - 0.0f) < epsilon);
    CHECK(std::abs(ClampF(15.0f, 0.0f, 10.0f) - 10.0f) < epsilon);
}
