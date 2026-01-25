#include "doctest.h"
#include "engine/resource/ResourceManager.hpp"
#include "core/logging/Logger.hpp"
#include <vector>
#include <string>

TEST_CASE("ResourceManager: Texture Array Loading (Headless)") {
    ResourceManager rm;
    rm.SetHeadless(true);

    std::vector<std::string> paths = {
        "test_sprite_0.png",
        "test_sprite_1.png",
        "test_sprite_2.png"
    };

    unsigned int texArray = rm.loadTextureArray(paths);
    
    // In headless mode, it returns 1
    CHECK(texArray == 1);
    CHECK(rm.getEntityTextureArray() == 1);

    // Verify mapping
    CHECK(rm.getTextureLayerIndex("test_sprite_0") == 0);
    CHECK(rm.getTextureLayerIndex("test_sprite_1") == 1);
    CHECK(rm.getTextureLayerIndex("test_sprite_2") == 2);
    CHECK(rm.getTextureLayerIndex("non_existent") == -1);

    rm.unloadAll();
    CHECK(rm.getEntityTextureArray() == 0);
}
