
#include "doctest.h"
#include "engine/render/GPUData.hpp"

using namespace NoMoreDay::components;

TEST_CASE("GPUFlags Bit Operations") {
    SUBCASE("PackAIState correctly shifts bits") {
        uint8_t state = 1; // PATROL
        uint32_t packed = GPUFlags::PackAIState(state);
        CHECK(packed == (1 << 8));
        
        state = 255;
        packed = GPUFlags::PackAIState(state);
        CHECK(packed == (255 << 8));
    }

    SUBCASE("UnpackAIState correctly retrieves bits") {
        uint32_t flags = (123 << 8); // 123 packed at bit 8
        // Add some noise in other bits
        flags |= GPU_ENTITY_FLAG_CHASING; // bit 2
        flags |= (1 << 20); // bit 20
        
        uint8_t state = GPUFlags::UnpackAIState(flags);
        CHECK(state == 123);
    }
    
    SUBCASE("Pack and Unpack Round Trip") {
        for (uint16_t i = 0; i < 256; ++i) {
            uint8_t original = static_cast<uint8_t>(i);
            uint32_t packed = GPUFlags::PackAIState(original);
            uint8_t unpacked = GPUFlags::UnpackAIState(packed);
            CHECK(unpacked == original);
        }
    }
    
    SUBCASE("Integration with Existing Flags") {
        uint32_t flags = GPU_ENTITY_FLAG_KINEMATIC | GPU_ENTITY_FLAG_CHASING;
        uint8_t aiState = 5; // NEMESIS_HUNTER
        
        flags |= GPUFlags::PackAIState(aiState);
        
        // check individual flags
        CHECK((flags & GPU_ENTITY_FLAG_KINEMATIC) != 0);
        CHECK((flags & GPU_ENTITY_FLAG_CHASING) != 0);
        
        // check ai state
        CHECK(GPUFlags::UnpackAIState(flags) == 5);
        
        // Modify AI state
        flags = (flags & ~GPUFlags::AI_STATE_MASK) | GPUFlags::PackAIState(2); // CHASE
        CHECK(GPUFlags::UnpackAIState(flags) == 2);
        
        // Verify other flags untouched
        CHECK((flags & GPU_ENTITY_FLAG_KINEMATIC) != 0);
        CHECK((flags & GPU_ENTITY_FLAG_CHASING) != 0);
    }
}
