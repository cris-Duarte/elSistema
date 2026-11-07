#include "Terrain.h"
#include <algorithm>
#include <cstdlib>
namespace voxel {
namespace {
uint32_t hash(int x,int y,uint32_t seed) {
    uint32_t h=seed ^ (static_cast<uint32_t>(x)*0x9e3779b9u) ^ (static_cast<uint32_t>(y)*0x85ebca6bu);
    h^=h>>16; h*=0x7feb352du; h^=h>>15; h*=0x846ca68bu; return h^(h>>16);
}
// Q12 gradient Perlin; all multiplies are widened before division.
int32_t mul(int32_t a,int32_t b) { return static_cast<int32_t>((static_cast<int64_t>(a)*b)/4096); }
int32_t fade(int32_t t) { return mul(mul(t,t),3*4096-2*t); }
int32_t lerp(int32_t a,int32_t b,int32_t t) { return a+mul(b-a,t); }
int32_t grad(uint32_t h,int32_t x,int32_t y) {
    switch(h&7u) {
    case 0:return x+y; case 1:return x-y; case 2:return -x+y; case 3:return -x-y;
    case 4:return x; case 5:return -x; case 6:return y; default:return -y;
    }
}
}
int32_t Terrain::noise(int x,int y,int scale,uint32_t seed) {
    const int gx=x/scale,gy=y/scale;
    const int32_t fx=(x%scale)*4096/scale,fy=(y%scale)*4096/scale;
    return lerp(lerp(grad(hash(gx,gy,seed),fx,fy),grad(hash(gx+1,gy,seed),fx-4096,fy),fade(fx)),
                lerp(grad(hash(gx,gy+1,seed),fx,fy-4096),grad(hash(gx+1,gy+1,seed),fx-4096,fy-4096),fade(fx)),fade(fy));
}
int Terrain::height(int x,int y,uint32_t seed) {
    const int value=static_cast<int>(11+(noise(x,y,20,seed)*7+noise(x,y,9,seed^0xabc123u)*3)/4096);
    return std::clamp(value,5,20);
}
void Terrain::generate(World& w,uint32_t seed) {
    w.clear();
    for(int y=0;y<cfg::WorldY;++y) for(int x=0;x<cfg::WorldX;++x) {
        const int h=height(x,y,seed);
        for(int z=0;z<=h;++z) w.set(x,y,z,z==h ? (h<=8?Block::Sand:Block::Grass) : (z>=h-2?Block::Dirt:Block::Stone));
    }
    for(int y=3;y<cfg::WorldY-3;y+=4) for(int x=3;x<cfg::WorldX-3;x+=4) {
        if(hash(x,y,seed)%7!=0 || (std::abs(x-32)<4 && std::abs(y-32)<4)) continue;
        const int h=height(x,y,seed);
        if(w.get(x,y,h)!=Block::Grass) continue;
        for(int z=h+3;z<=h+5;++z) for(int dy=-2;dy<=2;++dy) for(int dx=-2;dx<=2;++dx) {
            if(std::abs(dx)+std::abs(dy)>3 || (z==h+5 && std::abs(dx)+std::abs(dy)>1)) continue;
            if(w.get(x+dx,y+dy,z)==Block::Air) w.set(x+dx,y+dy,z,Block::Leaves);
        }
        for(int z=h+1;z<=h+4;++z) w.set(x,y,z,Block::Wood);
    }
}
}
