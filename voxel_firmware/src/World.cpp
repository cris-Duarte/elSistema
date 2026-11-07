#include "World.h"
#include "Memory.h"
#include <cstring>

namespace voxel {
World::~World() { freeExternal(data_); }
bool World::init() {
    if(data_) return true;
    data_=static_cast<Storage*>(allocateExternal(sizeof(Storage)));
    if(!data_) return false;
    clear(); return true;
}
void World::clear() { if(data_) std::memset(data_,0,sizeof(Storage)); }
Block World::get(int x,int y,int z) const {
    return data_ && contains(x,y,z) ? unchecked(x,y,z) : Block::Air;
}
bool World::set(int x,int y,int z,Block block) {
    if(!data_ || !contains(x,y,z) || static_cast<uint8_t>(block)>6) return false;
    auto& value=data_->blocks[index(x,y,z)];
    if(value!=static_cast<uint8_t>(block)) {
        value=static_cast<uint8_t>(block);
        ++data_->revisions[(x/cfg::ChunkSide)+4*(y/cfg::ChunkSide)];
    }
    return true;
}
uint32_t World::revision(int chunk) const {
    return data_ && chunk>=0 && chunk<cfg::ChunkCount ? data_->revisions[chunk] : 0;
}
uint32_t World::checksum() const {
    uint32_t hash=2166136261u;
    if(data_) for(uint8_t v:data_->blocks) hash=(hash^v)*16777619u;
    return hash;
}
}
