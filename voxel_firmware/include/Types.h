#pragma once
#include <cmath>
#include <cstdint>

namespace voxel {
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator+(Vec3 v) const { return {x+v.x,y+v.y,z+v.z}; }
    Vec3 operator-(Vec3 v) const { return {x-v.x,y-v.y,z-v.z}; }
    Vec3 operator*(float s) const { return {x*s,y*s,z*s}; }
};
inline float length(Vec3 v) { return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
inline Vec3 normalized(Vec3 v) { const float n=length(v); return n>0 ? v*(1.f/n) : Vec3{}; }
struct Cell { int x=0,y=0,z=0; };
enum class Block : uint8_t { Air, Dirt, Grass, Stone, Wood, Leaves, Sand };
static_assert(sizeof(Block)==1);
inline bool solid(Block b) { return b!=Block::Air; }
inline constexpr uint16_t rgb565(unsigned r,unsigned g,unsigned b) {
    return static_cast<uint16_t>(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
struct RaycastHit {
    bool valid=false;
    Cell cell{},normal{};
    float t=0;
    Block block=Block::Air;
    uint16_t steps=0;
};
struct InputState {
    float strafe=0, forward=0, lookX=0, lookY=0;
    bool jump=false, mine=false, place=false, pause=false, diagnostic=false;
    int slot=-1;
};
struct RenderStats { uint32_t rays=0,steps=0; uint16_t maxSteps=0; };
struct TouchPoint { int id=0,x=0,y=0; };
}
