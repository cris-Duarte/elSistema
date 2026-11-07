#pragma once
#include "Config.h"
#include "Types.h"

namespace voxel {
class World {
public:
    World()=default;
    ~World();
    World(const World&)=delete;
    World& operator=(const World&)=delete;
    bool init();
    void clear();
    static bool contains(int x,int y,int z) {
        return static_cast<unsigned>(x)<cfg::WorldX &&
               static_cast<unsigned>(y)<cfg::WorldY && static_cast<unsigned>(z)<cfg::WorldZ;
    }
    Block get(int x,int y,int z) const;
    bool set(int x,int y,int z,Block block);
    // Only call after contains(). Flat storage avoids pointer chasing in PSRAM.
    Block unchecked(int x,int y,int z) const { return static_cast<Block>(data_->blocks[index(x,y,z)]); }
    uint32_t revision(int chunk) const;
    uint32_t checksum() const;
private:
    static std::size_t index(int x,int y,int z) { return x+cfg::WorldX*(y+cfg::WorldY*z); }
    struct Storage { uint8_t blocks[cfg::WorldBytes]; uint32_t revisions[cfg::ChunkCount]; };
    Storage* data_=nullptr;
};
}
