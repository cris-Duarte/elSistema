#include "Scale.h"
#include <algorithm>
#include <cstring>
namespace voxel {
bool scaleNearest(const uint16_t* src,int width,int height,uint16_t* dst,int pw,int ph,unsigned r) {
    if(!src || !dst || src==dst || width<=0 || height<=0 || pw<=0 || ph<=0 || r>7) return false;
    const int lw=(r&1)?ph:pw,lh=(r&1)?pw:ph;
    if(lw%width || lh%height || lw/width!=lh/height) return false;
    const int scale=lw/width;
    // Iterate native rows for contiguous PSRAM writes even when landscape is rotated.
    // Each source sample expands into a run; repeated native rows use memcpy.
    for(int py=0;py<ph;++py) {
        uint16_t* row=dst+py*pw;
        if(py%scale) { std::memcpy(row,row-pw,static_cast<std::size_t>(pw)*2); continue; }
        for(int px=0;px<pw;px+=scale) {
            int x=(r&1)?py:px,y=(r&1)?px:py;
            if(r&2) x=lw-1-x;
            if((1u<<r)&0b10010110u) y=lh-1-y;
            const uint16_t color=src[(y/scale)*width+x/scale];
            std::fill_n(row+px,scale,color);
        }
    }
    return true;
}
}
