#pragma once
#include "Camera.h"
#include "World.h"
namespace voxel {
class Player {
public:
    Vec3 feet{};
    float verticalSpeed=0;
    bool grounded=false;
    Camera camera{};
    bool spawn(const World& world);
    bool overlaps(Cell cell) const;
    bool collides(const World& world,Vec3 position) const;
    void step(const World& world,const InputState& input,float dt);
    void syncCamera();
private:
    void moveAxis(const World& world,int axis,float amount);
};
}
