#pragma once
#include "Player.h"
#include "Raycast.h"
namespace voxel {
class Game {
public:
    bool init(uint32_t seed=cfg::Seed);
    void advance(float dt,const InputState& input);
    bool mine();
    bool place();
    World world;
    Player player;
    RaycastHit target{};
    Block selected=Block::Grass;
    bool paused=true,diagnostic=false;
    uint32_t physicsSteps=0,droppedSteps=0;
private:
    float accumulator_=0,cooldown_=0;
    bool pendingJump_=false;
};
}
