#pragma once
#include "Camera.h"
#include "Types.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
namespace voxel {
struct BenchmarkReport {
    int scene=0;
    bool hasWindow=false;
    uint32_t frames=0,samples=0;
    uint64_t elapsedUs=0;
    float fps=0,minWindowFps=0,renderMs=0,presentMs=0;
    std::size_t minPsram=0,minInternal=0;
};

// Autonomous, deterministic hardware workload. Clock and frame acknowledgments
// are supplied by the HAL; host tests validate transitions and aggregation.
class Benchmark {
public:
    static constexpr int SceneCount=4;
    static constexpr uint64_t WarmupUs=3000000,MeasureUs=60000000,WindowUs=5000000;
    enum class Event { None,SceneStarted,Measuring,SceneFinished,Finished };
    bool begin(uint64_t now,World& world,Camera& camera);
    Event update(uint64_t now,World& world,Camera& camera);
    void record(uint64_t now,uint32_t completed,uint32_t renderUs,uint32_t presentUs,
                std::size_t freePsram,std::size_t freeInternal);
    int scene() const { return scene_; }
    bool measuring() const { return phase_==Phase::Measure; }
    bool finished() const { return phase_==Phase::Done; }
    const BenchmarkReport& report() const { return report_; }
    static const char* name(int scene);
    static void prepare(int scene,World& world,Camera& camera);
private:
    enum class Phase { Warmup,Measure,Next,Done };
    Phase phase_=Phase::Done;
    int scene_=0;
    uint64_t phaseStart_=0,windowStart_=0,renderSum_=0,presentSum_=0;
    uint32_t windowFrames_=0;
    BenchmarkReport report_{};
};
}
