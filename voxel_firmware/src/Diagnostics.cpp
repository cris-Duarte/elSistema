#include "Diagnostics.h"
#include "esp_heap_caps.h"
#include <algorithm>
namespace voxel {
void Diagnostics::record(uint64_t now,uint32_t done,uint32_t renderUs,uint32_t presentUs,const RenderStats& s) {
    renderMs=renderUs*.001f; presentMs=presentUs*.001f;
    averageSteps=s.rays?static_cast<float>(s.steps)/s.rays:0;
    if(done) {
        if(lastComplete_) {
            intervals_[cursor_]=static_cast<uint32_t>((now-lastComplete_)/done);
            cursor_=(cursor_+1)%128; size_=std::min(128u,size_+1);
        }
        lastComplete_=now;
    }
    frames+=done; windowFrames_+=done;
    if(!windowStart_) windowStart_=now;
    if(now-windowStart_<1000000) return;
    fps=windowFrames_*1000000.f/static_cast<float>(now-windowStart_);
    windowStart_=now; windowFrames_=0;
    uint32_t sorted[128]; std::copy_n(intervals_,size_,sorted); std::sort(sorted,sorted+size_);
    frameP95=size_?sorted[(size_-1)*95/100]*.001f:0;
    freePsram=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    freeInternal=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    minInternal=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    largestInternal=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
}
}
