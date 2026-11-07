#include "Renderer.h"
#include "Memory.h"
#include "Raycast.h"
#include <algorithm>
namespace voxel {
namespace {
constexpr uint8_t Palette[7][3]={{0,0,0},{121,79,48},{95,166,61},{140,146,153},{131,94,52},{54,122,60},{218,201,138}};
}
Renderer::~Renderer() { freeExternal(inverseLengths_); }
uint16_t Renderer::color(Block block,int face) {
    const auto b=static_cast<unsigned>(block);
    if(b>=7) return 0xf81f;
    const unsigned shade=face==0?256:(face==1?205:158);
    return rgb565(Palette[b][0]*shade/256,Palette[b][1]*shade/256,Palette[b][2]*shade/256);
}
bool Renderer::init() {
    if(inverseLengths_) return true;
    inverseLengths_=static_cast<float*>(allocateExternal(cfg::Pixels*sizeof(float)));
    if(!inverseLengths_) return false;
    const float half=std::tan(cfg::Fov*.5f);
    for(int x=0;x<cfg::Width;++x) xs_[x]=(2.f*(x+.5f)/cfg::Width-1.f)*half;
    for(int y=0;y<cfg::Height;++y) ys_[y]=(1.f-2.f*(y+.5f)/cfg::Height)*half*cfg::Height/cfg::Width;
    for(int y=0;y<cfg::Height;++y) for(int x=0;x<cfg::Width;++x)
        inverseLengths_[y*cfg::Width+x]=1.f/std::sqrt(1+xs_[x]*xs_[x]+ys_[y]*ys_[y]);
    return true;
}
RenderStats Renderer::render(const World& world,const Camera& cam,uint16_t* pixels,float distance) {
    RenderStats stats;
    if(!pixels || !inverseLengths_) return stats;
    distance=std::clamp(distance,12.f,32.f);
    uint16_t colors[7][3];
    for(int b=0;b<7;++b) for(int f=0;f<3;++f) colors[b][f]=color(static_cast<Block>(b),f);
    const Vec3 increment=cam.right*(2.f*std::tan(cfg::Fov*.5f)/cfg::Width);
    for(int y=0;y<cfg::Height;++y) {
        const uint16_t sky=rgb565(92+y*40/cfg::Height,156+y*35/cfg::Height,220);
        Vec3 direction=cam.ray(xs_[0],ys_[y]);
        for(int x=0;x<cfg::Width;++x) {
            const int index=y*cfg::Width+x;
            // Precomputed norm turns the Euclidean range into a DDA parameter.
            const auto hit=trace(world,cam.position,direction,distance*inverseLengths_[index]);
            direction=direction+increment;
            ++stats.rays; stats.steps+=hit.steps; stats.maxSteps=std::max(stats.maxSteps,hit.steps);
            pixels[index]=hit.valid ? colors[static_cast<unsigned>(hit.block)][hit.normal.z?0:(hit.normal.x?1:2)] : sky;
        }
    }
    return stats;
}
}
