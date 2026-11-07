#pragma once
#include "Config.h"
#include "Diagnostics.h"
#include "Game.h"
namespace voxel {
class Hud {
public:
    explicit Hud(uint16_t* pixels):pixels_(pixels) {}
    void draw(const Game& game,const Diagnostics& stats);
    void pattern(uint32_t frame);
    void text(int x,int y,const char* value,uint16_t color=0xffff);
    void rect(int x,int y,int width,int height,uint16_t color,bool filled=true);
private:
    uint16_t* pixels_;
};
}
