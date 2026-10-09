#pragma once
#include <cstddef>
#include <cstdint>
namespace el_sistema {
// Original 8-bit melody and generated effects, no samples/ROM resources.
class TetrisSynth {
public:
    void reset();
    void trigger(uint32_t events);
    void render(int16_t* pcm,size_t count,bool music,uint8_t volume);
private:
    uint64_t sample_=0;
    uint32_t effect_remaining_=0,effect_length_=0;
    float music_phase_=0,bass_phase_=0,effect_phase_=0,effect_frequency_=0;
};
}
