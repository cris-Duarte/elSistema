#pragma once
#include "Camera.h"
#include "World.h"
namespace voxel {
class Renderer {
public:
    ~Renderer();
    Renderer()=default;
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;
    bool init();
    RenderStats render(const World& world,const Camera& camera,uint16_t* pixels,float distance=cfg::ViewDistance);
    static uint16_t color(Block block,int face=0);
private:
    float* inverseLengths_=nullptr;
    float xs_[cfg::Width]{},ys_[cfg::Height]{};
};
}
