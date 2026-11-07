#include "Raycast.h"
#include <algorithm>
#include <limits>
namespace voxel {
RaycastHit trace(const World& world,Vec3 o,Vec3 d,float maxT) {
    RaycastHit hit;
    const float origin[]={o.x,o.y,o.z},dir[]={d.x,d.y,d.z};
    const int limits[]={cfg::WorldX,cfg::WorldY,cfg::WorldZ};
    float nearPlanes[3]={-std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()};
    float entry=0,exit=maxT;
    int entryAxis=-1,entrySign=0;
    if(!(maxT>=0) || !std::isfinite(maxT)) return hit;
    if(d.x==0 && d.y==0 && d.z==0) return hit;
    for(int a=0;a<3;++a) {
        if(!std::isfinite(origin[a]) || !std::isfinite(dir[a])) return hit;
        if(dir[a]==0) { if(origin[a]<0 || origin[a]>=limits[a]) return hit; continue; }
        float near=(0.f-origin[a])/dir[a],far=(limits[a]-origin[a])/dir[a];
        const int sign=dir[a]>0?-1:1;
        if(near>far) std::swap(near,far);
        nearPlanes[a]=near;
        if(near>entry) { entry=near; entryAxis=a; entrySign=sign; }
        exit=std::min(exit,far);
        if(entry>exit) return hit;
    }
    int cell[3],step[3],normal[3]={0,0,0};
    float next[3],delta[3];
    if(entryAxis>=0) normal[entryAxis]=entrySign;
    for(int a=0;a<3;++a) {
        float p=origin[a]+dir[a]*entry;
        // Reconstruct slab-entry coordinates exactly. o+d*t can round to a
        // tiny negative value at x=0 (or beyond the maximum face), making an
        // inward ray start outside. Snap every tied entry plane, not just the
        // chosen normal axis, so corner entries obey the same rule.
        if(entryAxis>=0 && nearPlanes[a]==entry) p=dir[a]>0?0.f:static_cast<float>(limits[a]);
        cell[a]=static_cast<int>(std::floor(p));
        if(dir[a]<0 && p==std::floor(p)) {
            --cell[a];
            if(entry==0 && entryAxis<0) { normal[a]=1; entryAxis=a; }
        }
        step[a]=dir[a]>0?1:(dir[a]<0?-1:0);
        delta[a]=step[a] ? std::abs(1.f/dir[a]) : std::numeric_limits<float>::infinity();
        next[a]=step[a] ? ((cell[a]+(step[a]>0?1:0))-origin[a])/dir[a] : delta[a];
    }
    float t=entry;
    for(int count=0;count<cfg::MaxRaySteps;++count) {
        if(t>exit || !World::contains(cell[0],cell[1],cell[2])) break;
        ++hit.steps;
        // Bounds are proven here. One byte fetched from flat PSRAM per cell.
        const Block b=world.unchecked(cell[0],cell[1],cell[2]);
        if(solid(b)) {
            hit.valid=true; hit.cell={cell[0],cell[1],cell[2]};
            hit.normal={normal[0],normal[1],normal[2]}; hit.t=t; hit.block=b; return hit;
        }
        t=std::min(next[0],std::min(next[1],next[2]));
        normal[0]=normal[1]=normal[2]=0;
        bool chosen=false;
        // Exact edge/corner ties advance together: zero-volume cells are skipped.
        // X, then Y, then Z is the deterministic face normal for a tie.
        for(int a=0;a<3;++a) if(next[a]==t) {
            cell[a]+=step[a]; next[a]+=delta[a];
            if(!chosen) { normal[a]=-step[a]; chosen=true; }
        }
    }
    return hit;
}
}
