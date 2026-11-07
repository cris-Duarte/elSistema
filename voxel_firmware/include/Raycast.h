#pragma once
#include "World.h"
namespace voxel {
// maxT is a parameter along direction, not necessarily Euclidean distance.
RaycastHit trace(const World& world,Vec3 origin,Vec3 direction,float maxT);
}
