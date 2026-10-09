#include "tetris_sound.hpp"
#include "tetris_model.hpp"
#include <algorithm>
#include <cmath>
namespace el_sistema {
void TetrisSynth::reset() { sample_=0;effect_remaining_=0;music_phase_=bass_phase_=effect_phase_=0; }
void TetrisSynth::trigger(uint32_t e) {
    effect_frequency_= e&TetrisEnd?110:e&TetrisLevel?880:e&TetrisLines?660:e&TetrisLock?165:e&TetrisRotate?495:330;
    effect_length_=e&TetrisEnd?4800:e&(TetrisLevel|TetrisLines)?2400:640;
    effect_remaining_=effect_length_;effect_phase_=0;
}
void TetrisSynth::render(int16_t* pcm,size_t n,bool music,uint8_t volume) {
    constexpr int melody[]={72,0,79,76,74,0,71,74,76,79,83,79,76,74,71,0,
                            69,72,76,0,77,76,72,69,67,71,74,77,74,71,67,0};
    constexpr int bass[]={48,53,45,55};
    for(size_t i=0;i<n;++i,++sample_) {
        float v=0;
        if(music) {
            const unsigned beat=unsigned(sample_/2400);const unsigned t=unsigned(sample_%2400);
            const int note=melody[beat%32];
            if(note) {
                const float frequency=440*std::pow(2.f,(note-69)/12.f);
                music_phase_+=frequency/16000;music_phase_-=std::floor(music_phase_);
                const float envelope=std::min(1.f,t/64.f)*std::max(0.f,(2200.f-t)/500.f);
                v+=(music_phase_<.25f?1.f:-.333f)*.16f*std::min(1.f,envelope);
            }
            const float frequency=440*std::pow(2.f,(bass[(beat/8)%4]-69)/12.f);
            bass_phase_+=frequency/16000;bass_phase_-=std::floor(bass_phase_);
            v+=(1.f-4.f*std::abs(bass_phase_-.5f))*.08f;
        }
        if(effect_remaining_) {
            const float progress=1.f-float(effect_remaining_)/effect_length_;
            effect_phase_+=effect_frequency_*(1+.4f*progress)/16000;effect_phase_-=std::floor(effect_phase_);
            const float envelope=std::min(1.f,progress*30)*std::min(1.f,effect_remaining_/128.f);
            v+=(effect_phase_<.5f?1.f:-1.f)*.28f*envelope;--effect_remaining_;
        }
        pcm[i]=int16_t(std::clamp(v*volume*.01f,-.95f,.95f)*32767);
    }
}
}
