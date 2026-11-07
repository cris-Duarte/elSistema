#pragma once
#include "Config.h"
#include "Types.h"
namespace voxel {
struct Camera {
    Vec3 position{};
    float yaw=0,pitch=0;
    Vec3 forward{},right{},up{};
    void updateBasis();
    Vec3 ray(float screenX,float screenY) const;
};
}
