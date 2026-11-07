#include "Camera.h"
#include "Benchmark.h"
#include "Game.h"
#include "Hud.h"
#include "Input.h"
#include "Memory.h"
#include "Renderer.h"
#include "Scale.h"
#include "Terrain.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

namespace {
int checks=0,failures=0,allocations=0,liveAllocations=0;
bool failAllocation=false;
#define CHECK(condition) do { ++checks; if(!(condition)) { ++failures; std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#condition); } } while(0)
bool near(float a,float b,float epsilon=.001f) { return std::abs(a-b)<epsilon; }
void floor(voxel::World& w,int height=0) {
    w.clear();
    for(int y=0;y<64;++y) for(int x=0;x<64;++x) w.set(x,y,height,voxel::Block::Stone);
}
void testMemoryAndWorld() {
    using namespace voxel;
    failAllocation=true;
    World rejected; CHECK(!rejected.init());
    failAllocation=false;
    World w; CHECK(w.init()); CHECK(w.init());
    CHECK(w.get(-1,0,0)==Block::Air); CHECK(!w.set(-1,0,0,Block::Dirt));
    CHECK(!w.set(64,0,0,Block::Dirt)); CHECK(!w.set(0,64,0,Block::Dirt)); CHECK(!w.set(0,0,32,Block::Dirt));
    CHECK(!w.set(0,0,0,static_cast<Block>(255)));
    CHECK(w.set(63,63,31,Block::Sand)); CHECK(w.get(63,63,31)==Block::Sand);
    CHECK(w.revision(15)==1); CHECK(w.revision(0)==0);
    w.clear(); CHECK(w.get(63,63,31)==Block::Air); CHECK(w.revision(15)==0);
    Renderer r; failAllocation=true; CHECK(!r.init()); failAllocation=false; CHECK(r.init());
}
void testTerrain() {
    using namespace voxel;
    World w; CHECK(w.init()); Terrain::generate(w,cfg::Seed); const auto checksum=w.checksum();
    Terrain::generate(w,cfg::Seed); CHECK(checksum==w.checksum());
    int wood=0,leaves=0;
    for(int y=0;y<64;++y) for(int x=0;x<64;++x) {
        CHECK(w.get(x,y,0)==Block::Stone);
        for(int z=0;z<32;++z) {
            const auto b=w.get(x,y,z); CHECK(static_cast<unsigned>(b)<=6);
            wood+=b==Block::Wood; leaves+=b==Block::Leaves;
        }
    }
    CHECK(wood>0 && leaves>0);
    Player p; CHECK(p.spawn(w)); CHECK(!p.collides(w,p.feet));
    Terrain::generate(w,cfg::Seed+1); CHECK(checksum!=w.checksum());
}
void testRays() {
    using namespace voxel;
    World w; CHECK(w.init()); w.set(5,5,5,Block::Stone);
    auto h=trace(w,{2.5f,5.5f,5.5f},{1,0,0},5);
    CHECK(h.valid && h.cell.x==5 && h.normal.x==-1 && near(h.t,2.5f));
    CHECK(!trace(w,{2.5f,5.5f,5.5f},{1,0,0},2.49f).valid);
    CHECK(trace(w,{2.5f,5.5f,5.5f},{1,0,0},2.5f).valid);
    h=trace(w,{8.f,5.5f,5.5f},{-1,0,0},5); CHECK(h.valid && h.normal.x==1 && near(h.t,2));
    h=trace(w,{5.f,5.5f,5.5f},{1,0,0},5); CHECK(h.valid && near(h.t,0));
    CHECK(!trace(w,{5.f,5.5f,5.5f},{-1,0,0},5).valid);
    h=trace(w,{5.5f,5.5f,8},{0,0,-1},5); CHECK(h.valid && h.normal.z==1);
    CHECK(!trace(w,{0,0,0},{0,0,0},5).valid);
    CHECK(!trace(w,{-1,5,5},{0,1,0},5).valid);
    CHECK(!trace(w,{1,1,1},{std::numeric_limits<float>::quiet_NaN(),0,0},5).valid);
    w.set(0,5,5,Block::Grass);
    h=trace(w,{-2,5.5f,5.5f},{1,0,0},5); CHECK(h.valid && near(h.t,2) && h.normal.x==-1);
    w.clear(); w.set(2,2,2,Block::Wood); w.set(2,1,1,Block::Dirt);
    h=trace(w,{1.5f,1.5f,1.5f},{1,1,1},5);
    CHECK(h.valid && h.block==Block::Wood && h.normal.x==-1 && near(h.t,.5f));
    w.clear(); w.set(63,63,31,Block::Sand);
    h=trace(w,{64,63.5f,31.5f},{-1,0,0},5); CHECK(h.valid && h.cell.x==63 && h.normal.x==1);
}
uint32_t randomState=0x1234567u;
float randomUnit() { randomState=randomState*1664525u+1013904223u; return (randomState>>8)/16777216.f; }
void testRaysAgainstIndependentSlabs() {
    using namespace voxel;
    World w; CHECK(w.init());
    constexpr Cell cubes[]={{7,8,8},{9,8,8},{10,10,8},{8,9,10},{6,10,7},{11,7,9}};
    for(auto c:cubes) w.set(c.x,c.y,c.z,Block::Stone);
    for(int n=0;n<1500;++n) {
        const Vec3 o={4.f+randomUnit()*10,4.f+randomUnit()*10,4.f+randomUnit()*10};
        const Vec3 d=normalized({randomUnit()*2-1,randomUnit()*2-1,randomUnit()*2-1});
        float closest=15.f; bool found=false;
        for(auto c:cubes) {
            const float start[]={o.x,o.y,o.z},direction[]={d.x,d.y,d.z}; const int minimum[]={c.x,c.y,c.z};
            float enter=0,exit=15.f; bool valid=true;
            for(int a=0;a<3;++a) {
                if(direction[a]==0) { if(start[a]<minimum[a] || start[a]>=minimum[a]+1) valid=false; continue; }
                float a0=(minimum[a]-start[a])/direction[a],a1=(minimum[a]+1-start[a])/direction[a];
                if(a0>a1) std::swap(a0,a1);
                enter=std::max(enter,a0); exit=std::min(exit,a1);
            }
            if(valid && enter<=exit && enter<=closest) { closest=enter; found=true; }
        }
        const auto hit=trace(w,o,d,15.f);
        CHECK(hit.valid==found); if(found) CHECK(near(hit.t,closest,.0001f));
    }
}
void testOutsideRays() {
    using namespace voxel;
    World w; CHECK(w.init());
    // Full solid map makes every inward ray hit its world-entry face, exposing
    // rounding errors that rays starting inside the map cannot exercise.
    for(int z=0;z<32;++z) for(int y=0;y<64;++y) for(int x=0;x<64;++x) w.set(x,y,z,Block::Stone);
    for(int i=0;i<3000;++i) {
        const float gap=.01f+randomUnit()*30;
        const float speed=.01f+randomUnit();
        for(int axis=0;axis<3;++axis) for(int sign=-1;sign<=1;sign+=2) {
            float o[]={5+randomUnit()*50,5+randomUnit()*50,5+randomUnit()*20};
            float d[]={0,0,0}; const int limits[]={64,64,32};
            o[axis]=sign>0?-gap:limits[axis]+gap; d[axis]=speed*sign;
            const auto hit=trace(w,{o[0],o[1],o[2]},{d[0],d[1],d[2]},4000);
            const int cell[]={hit.cell.x,hit.cell.y,hit.cell.z};
            const int normal[]={hit.normal.x,hit.normal.y,hit.normal.z};
            CHECK(hit.valid && cell[axis]==(sign>0?0:limits[axis]-1) && normal[axis]==-sign);
        }
    }
    const auto corner=trace(w,{-1,-1,-1},{.3f,.3f,.3f},100);
    CHECK(corner.valid && corner.cell.x==0 && corner.cell.y==0 && corner.cell.z==0 && corner.normal.x==-1);
}
void testPhysics() {
    using namespace voxel;
    World w; CHECK(w.init()); floor(w);
    Player p; p.feet={10.5f,10.5f,10}; p.syncCamera();
    for(int i=0;i<300;++i) p.step(w,{},cfg::PhysicsStep);
    CHECK(near(p.feet.z,1) && p.grounded && p.verticalSpeed==0);
    InputState jump; jump.jump=true; p.step(w,jump,cfg::PhysicsStep); CHECK(p.feet.z>1 && !p.grounded);
    const float upward=p.verticalSpeed; p.step(w,jump,cfg::PhysicsStep); CHECK(p.verticalSpeed<upward);
    for(int i=0;i<300;++i) p.step(w,{},cfg::PhysicsStep); CHECK(near(p.feet.z,1));
    for(int z=1;z<=4;++z) for(int y=1;y<63;++y) w.set(13,y,z,Block::Stone);
    p.feet={10.5f,10.5f,1}; p.camera.yaw=0; InputState forward; forward.forward=1;
    for(int i=0;i<150;++i) p.step(w,forward,cfg::PhysicsStep);
    CHECK(near(p.feet.x,12.7f) && !p.collides(w,p.feet));
    p.feet={.4f,10.5f,1}; forward.forward=-1;
    for(int i=0;i<150;++i) p.step(w,forward,cfg::PhysicsStep); CHECK(near(p.feet.x,.3f));
    p.feet={20.5f,20.5f,1}; p.verticalSpeed=0; p.grounded=true;
    w.set(20,20,3,Block::Stone); p.step(w,jump,cfg::PhysicsStep);
    for(int i=0;i<20;++i) p.step(w,{},cfg::PhysicsStep);
    CHECK(p.feet.z<=1.2f && !p.collides(w,p.feet));
    p.feet={30.5f,30.5f,25}; p.verticalSpeed=-20;
    for(int i=0;i<100;++i) p.step(w,{},.08f); CHECK(near(p.feet.z,1));
    Player a,b; a.feet=b.feet={40.f,40.f,1}; a.camera.yaw=b.camera.yaw=0;
    InputState diagonal; diagonal.forward=1; diagonal.strafe=1; forward.forward=1;
    a.step(w,forward,cfg::PhysicsStep); b.step(w,diagonal,cfg::PhysicsStep);
    CHECK(near(length(a.feet-Vec3{40,40,1}),length(b.feet-Vec3{40,40,1})));
}
void testTouch() {
    using namespace voxel;
    Input input;
    TouchPoint points[]={{8,180,425},{3,700,300},{12,1100,450}};
    auto in=input.update(points,3,false); CHECK(in.forward>.9f && in.jump && in.lookX==0);
    TouchPoint reordered[]={{12,1100,450},{3,730,310},{8,180,425}};
    in=input.update(reordered,3,false); CHECK(!in.jump && near(in.lookX,.09f) && in.forward>.9f);
    reordered[1].x=1100; reordered[1].y=620;
    in=input.update(reordered,3,false); CHECK(!in.place && !in.mine && in.lookX>0);
    in=input.update(nullptr,0,false); CHECK(in.forward==0 && !in.jump);
    TouchPoint slot={0,550,650}; in=input.update(&slot,1,false); CHECK(in.slot==1 && !in.mine);
    input.reset(); TouchPoint menu={0,1190,30};
    in=input.update(&menu,1,false); CHECK(in.pause); input.cancelHeld();
    in=input.update(&menu,1,true); CHECK(!in.pause);
    input.update(nullptr,0,true);
    in=input.update(&menu,1,true); CHECK(in.pause);
}
void testPlacementAndTiming() {
    using namespace voxel;
    Game g; CHECK(g.init()); floor(g.world);
    g.player.feet={10.5f,10.5f,1}; g.player.camera.yaw=0; g.player.camera.pitch=0; g.player.syncCamera();
    g.world.set(13,10,2,Block::Stone); g.selected=Block::Wood;
    CHECK(g.place()); CHECK(g.world.get(12,10,2)==Block::Wood);
    CHECK(g.place()); CHECK(g.world.get(11,10,2)==Block::Wood);
    CHECK(!g.place()); CHECK(g.world.get(10,10,2)==Block::Air);
    CHECK(g.mine()); CHECK(g.world.get(11,10,2)==Block::Air);
    g.world.clear(); g.world.set(16,10,2,Block::Stone); CHECK(!g.mine());
    g.world.clear(); floor(g.world); g.player.camera.pitch=-1.4f; g.player.syncCamera(); CHECK(!g.mine());
    InputState resume; resume.pause=true; g.advance(0,resume); CHECK(!g.paused);
    const uint32_t before=g.physicsSteps; g.advance(5,{});
    CHECK(g.physicsSteps-before<=4 && g.droppedSteps>0);
    g.advance(0,resume); CHECK(g.paused); const Vec3 position=g.player.feet;
    InputState movement; movement.forward=1; g.advance(.5f,movement); CHECK(near(g.player.feet.x,position.x));
}
void testCameraAndScale() {
    using namespace voxel;
    Camera c; c.pitch=10; c.updateBasis(); CHECK(near(c.pitch,cfg::PitchLimit));
    CHECK(near(length(c.forward),1) && near(length(c.up),1));
    constexpr uint16_t source[]={1,2,3,4,5,6};
    for(unsigned r=0;r<8;++r) {
        const int pw=(r&1)?4:6,ph=(r&1)?6:4;
        uint16_t dest[24]{}; CHECK(scaleNearest(source,3,2,dest,pw,ph,r));
        // Independently map each logical pixel forward to native coordinates.
        for(int y=0;y<4;++y) for(int x=0;x<6;++x) {
            int px=x,py=y;
            if((1u<<r)&0b10010110u) py=3-py;
            if(r&2) px=5-px;
            if(r&1) std::swap(px,py);
            CHECK(dest[py*pw+px]==source[(y/2)*3+x/2]);
        }
    }
    uint16_t bad[25]; CHECK(!scaleNearest(source,3,2,bad,5,5,0));
}
void testBenchmark() {
    using namespace voxel;
    World w; Camera camera; Benchmark bench;
    CHECK(bench.begin(1000,w,camera));
    CHECK(bench.scene()==0 && !bench.measuring() && !bench.finished());
    CHECK(!trace(w,camera.position,camera.forward,24).valid);
    uint64_t now=1000;
    for(int scene=0;scene<Benchmark::SceneCount;++scene) {
        CHECK(bench.scene()==scene);
        CHECK(bench.update(now+Benchmark::WarmupUs-1,w,camera)==Benchmark::Event::None);
        now+=Benchmark::WarmupUs;
        CHECK(bench.update(now,w,camera)==Benchmark::Event::Measuring);
        const auto start=now;
        const int allocated=allocations;
        for(int frame=1;frame<=1200;++frame) {
            now=start+static_cast<uint64_t>(frame)*50000;
            bench.record(now,1,10000,2000,10000000-frame,100000-frame);
        }
        CHECK(allocations==allocated);
        CHECK(bench.update(now,w,camera)==Benchmark::Event::SceneFinished);
        const auto& report=bench.report();
        CHECK(report.frames==1200 && report.samples==1200 && near(report.fps,20));
        CHECK(report.hasWindow && near(report.minWindowFps,20));
        CHECK(near(report.renderMs,10) && near(report.presentMs,2));
        CHECK(report.minPsram==9998800 && report.minInternal==98800);
        ++now;
        CHECK(bench.update(now,w,camera)==(scene==3?Benchmark::Event::Finished:Benchmark::Event::SceneStarted));
        if(scene==1) {
            const auto checksum=w.checksum(); Camera second;
            Benchmark::prepare(2,w,second); CHECK(w.checksum()==checksum);
        }
    }
    CHECK(bench.finished());
    CHECK(bench.update(now+100,w,camera)==Benchmark::Event::None);
}
void testRendering(const char* path) {
    using namespace voxel;
    Game game; CHECK(game.init()); Renderer renderer; CHECK(renderer.init());
    std::vector<uint16_t> image(cfg::Pixels+2,0xdead); const int count=allocations;
    const auto stats=renderer.render(game.world,game.player.camera,image.data()+1);
    CHECK(stats.rays==cfg::Pixels && stats.steps>0 && stats.maxSteps<=cfg::MaxRaySteps);
    Hud hud(image.data()+1); hud.draw(game,{});
    CHECK(allocations==count); CHECK(image.front()==0xdead && image.back()==0xdead);
    if(path) {
        FILE* file=std::fopen(path,"wb"); CHECK(file!=nullptr);
        if(file) {
            std::fprintf(file,"P6\n%d %d\n255\n",cfg::Width,cfg::Height);
            // Capture the playable view rather than the menu for visual QA.
            renderer.render(game.world,game.player.camera,image.data()+1); game.paused=false; hud.draw(game,{});
            for(std::size_t i=1;i<=cfg::Pixels;++i) {
                const auto color=image[i]; const uint8_t rgb[]={static_cast<uint8_t>(((color>>11)&31)*255/31),
                    static_cast<uint8_t>(((color>>5)&63)*255/63),static_cast<uint8_t>((color&31)*255/31)};
                std::fwrite(rgb,1,3,file);
            }
            std::fclose(file);
        }
    }
}
}
namespace voxel {
void* testAllocate(std::size_t bytes) {
    ++allocations; if(failAllocation) return nullptr;
    void* p=std::malloc(bytes); if(p) ++liveAllocations; return p;
}
void testFree(void* p) { if(p) --liveAllocations; std::free(p); }
}
int main(int argc,char** argv) {
    testMemoryAndWorld(); testTerrain(); testRays(); testRaysAgainstIndependentSlabs(); testOutsideRays();
    testPhysics(); testTouch(); testPlacementAndTiming(); testCameraAndScale(); testBenchmark();
    testRendering(argc>1?argv[1]:nullptr);
    CHECK(liveAllocations==0);
    std::printf("%d comprobaciones, %d fallos; ASan/UBSan segun configuracion CMake\n",checks,failures);
    return failures?1:0;
}
