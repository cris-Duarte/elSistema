#include "Benchmark.h"
#include "Terrain.h"
#include <algorithm>
#include <limits>
namespace voxel {
const char* Benchmark::name(int scene) {
    constexpr const char* names[]={"vacio","plano","bosque","construccion"};
    return scene>=0 && scene<SceneCount?names[scene]:"fin";
}
void Benchmark::prepare(int scene,World& w,Camera& camera) {
    w.clear();
    camera.position={32.5f,32.5f,10.62f}; camera.yaw=.65f; camera.pitch=-.12f;
    if(scene==0) { camera.position.z=16; camera.pitch=0; }
    else if(scene==2) {
        Terrain::generate(w,cfg::Seed);
        camera.position.z=Terrain::height(32,32,cfg::Seed)+1+cfg::EyeHeight;
    } else {
        for(int y=0;y<64;++y) for(int x=0;x<64;++x)
            for(int z=0;z<=8;++z) w.set(x,y,z,z==8?Block::Grass:(z>5?Block::Dirt:Block::Stone));
        if(scene==3) {
            for(int y=20;y<=44;++y) for(int x=20;x<=44;++x) for(int z=9;z<=15;++z) {
                const bool wall=x==20 || x==44 || y==20 || y==44;
                const bool hole=(x+y)%5<2 && z>=10 && z<=12;
                const bool pillar=(x==26 || x==38) && (y==26 || y==38);
                if((wall && !hole) || pillar) w.set(x,y,z,pillar?Block::Wood:Block::Stone);
            }
        }
    }
    camera.updateBasis();
}
bool Benchmark::begin(uint64_t now,World& world,Camera& camera) {
    if(!world.init()) return false;
    scene_=0; phase_=Phase::Warmup; phaseStart_=now; report_={};
    prepare(scene_,world,camera); return true;
}
Benchmark::Event Benchmark::update(uint64_t now,World& world,Camera& camera) {
    if(phase_==Phase::Done) return Event::None;
    if(phase_==Phase::Next) {
        if(++scene_>=SceneCount) { phase_=Phase::Done; return Event::Finished; }
        prepare(scene_,world,camera); phase_=Phase::Warmup; phaseStart_=now;
        return Event::SceneStarted;
    }
    if(phase_==Phase::Warmup && now-phaseStart_>=WarmupUs) {
        phase_=Phase::Measure; phaseStart_=windowStart_=now;
        report_={}; report_.scene=scene_;
        report_.minPsram=report_.minInternal=std::numeric_limits<std::size_t>::max();
        windowFrames_=0; renderSum_=presentSum_=0;
        return Event::Measuring;
    }
    if(phase_==Phase::Measure && now-phaseStart_>=MeasureUs) {
        report_.elapsedUs=now-phaseStart_;
        report_.fps=report_.frames*1000000.f/static_cast<float>(report_.elapsedUs);
        report_.renderMs=report_.samples?renderSum_*.001f/report_.samples:0;
        report_.presentMs=report_.samples?presentSum_*.001f/report_.samples:0;
        if(!report_.samples) report_.minPsram=report_.minInternal=0;
        phase_=Phase::Next; return Event::SceneFinished;
    }
    return Event::None;
}
void Benchmark::record(uint64_t now,uint32_t done,uint32_t renderUs,uint32_t presentUs,
                       std::size_t psram,std::size_t internal) {
    if(phase_!=Phase::Measure) return;
    report_.frames+=done; ++report_.samples;
    renderSum_+=renderUs; presentSum_+=presentUs;
    report_.minPsram=std::min(report_.minPsram,psram);
    report_.minInternal=std::min(report_.minInternal,internal);
    windowFrames_+=done;
    const auto elapsed=now-windowStart_;
    if(elapsed>=WindowUs) {
        const float fps=windowFrames_*1000000.f/static_cast<float>(elapsed);
        report_.minWindowFps=report_.hasWindow?std::min(report_.minWindowFps,fps):fps;
        report_.hasWindow=true; windowStart_=now; windowFrames_=0;
    }
}
}
