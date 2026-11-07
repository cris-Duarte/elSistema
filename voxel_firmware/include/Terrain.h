#pragma once
#include "World.h"
namespace voxel {
class Terrain {
public:
    static void generate(World& world,uint32_t seed);
    static int height(int x,int y,uint32_t seed);
private:
    static int32_t noise(int x,int y,int scale,uint32_t seed);
};
}
