#include "Game.h"
#include "Terrain.h"
#include <algorithm>
namespace voxel {
bool Game::init(uint32_t seed) {
    if(!world.init()) return false;
    Terrain::generate(world,seed);
    return player.spawn(world);
}
bool Game::mine() {
    const auto hit=trace(world,player.camera.position,player.camera.forward,cfg::Reach);
    return hit.valid && hit.cell.z>0 && world.set(hit.cell.x,hit.cell.y,hit.cell.z,Block::Air);
}
bool Game::place() {
    const auto hit=trace(world,player.camera.position,player.camera.forward,cfg::Reach);
    if(!hit.valid || (!hit.normal.x && !hit.normal.y && !hit.normal.z) || !solid(selected)) return false;
    const Cell c={hit.cell.x+hit.normal.x,hit.cell.y+hit.normal.y,hit.cell.z+hit.normal.z};
    return World::contains(c.x,c.y,c.z) && !solid(world.get(c.x,c.y,c.z)) &&
        !player.overlaps(c) && world.set(c.x,c.y,c.z,selected);
}
void Game::advance(float dt,const InputState& in) {
    if(!std::isfinite(dt) || dt<0) dt=0;
    if(in.pause) { paused=!paused; accumulator_=0; pendingJump_=false; }
    if(in.diagnostic) { diagnostic=!diagnostic; accumulator_=0; pendingJump_=false; }
    if(paused || diagnostic) return;
    const float elapsed=std::min(dt,.1f);
    player.camera.yaw-=in.lookX; player.camera.pitch+=in.lookY;
    player.syncCamera();
    if(in.slot>=0 && in.slot<6) selected=static_cast<Block>(in.slot+1);
    pendingJump_|=in.jump;
    accumulator_+=elapsed;
    int iterations=0;
    while(accumulator_>=cfg::PhysicsStep && iterations<4) {
        InputState physics=in; physics.jump=pendingJump_;
        player.step(world,physics,cfg::PhysicsStep); pendingJump_=false;
        accumulator_-=cfg::PhysicsStep; ++iterations; ++physicsSteps;
    }
    if(accumulator_>=cfg::PhysicsStep) {
        droppedSteps+=static_cast<uint32_t>(accumulator_/cfg::PhysicsStep);
        accumulator_=std::fmod(accumulator_,cfg::PhysicsStep);
    }
    cooldown_=std::max(0.f,cooldown_-elapsed);
    if(cooldown_==0 && in.slot<0) {
        if(in.mine) { mine(); cooldown_=.15f; }
        else if(in.place) { place(); cooldown_=.15f; }
    }
    target=trace(world,player.camera.position,player.camera.forward,cfg::Reach);
}
}
