#pragma once
#include <cstdint>
namespace voxel {
// Mirrors Panel_FrameBufferBase's rotation mapping; source/dest cannot alias.
bool scaleNearest(const uint16_t* source,int width,int height,uint16_t* destination,
                  int panelWidth,int panelHeight,unsigned rotation);
}
