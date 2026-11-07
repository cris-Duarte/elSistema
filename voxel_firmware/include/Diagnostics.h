#pragma once
#include "Types.h"
#include <cstddef>
namespace voxel {
struct Diagnostics {
    float fps=0,renderMs=0,presentMs=0,frameP95=0,averageSteps=0;
    std::size_t freePsram=0,freeInternal=0,minInternal=0,largestInternal=0;
    uint32_t frames=0;
    uint8_t contacts=0;
    void record(uint64_t now,uint32_t completed,uint32_t renderUs,uint32_t presentUs,const RenderStats& stats);
private:
    uint64_t windowStart_=0,lastComplete_=0;
    uint32_t windowFrames_=0,intervals_[128]{};
    unsigned cursor_=0,size_=0;
};
}
