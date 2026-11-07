#include "Camera.h"
#include <algorithm>
namespace voxel {
void Camera::updateBasis() {
    pitch=std::clamp(pitch,-cfg::PitchLimit,cfg::PitchLimit);
    yaw=std::remainder(yaw,2.f*cfg::Pi);
    const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
    forward={cy*cp,sy*cp,sp}; right={sy,-cy,0}; up={-cy*sp,-sy*sp,cp};
}
Vec3 Camera::ray(float x,float y) const { return forward+right*x+up*y; }
}
