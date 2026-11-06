// Host-only numerical regression for 4-op FM DSP; NOT a Tracker hardware test.
#include "../core/synth/engines/FM4DSP.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>

static void checkFinite(const std::array<float,128>& samples) {
    for(float v:samples) {
        assert(std::isfinite(v));
        assert(std::fabs(v)<=1.00001f);
    }
}
int main() {
    fm4::Engine engine;
    std::array<float,128> samples{};
    for(int algorithm=0;algorithm<8;++algorithm) {
        engine.algorithm=algorithm;
        engine.noteOn(60,1.0f);
        float maxAbs=0, energy=0;
        for(int j=0;j<150;++j) {
            engine.render(samples.data(),samples.size());
            checkFinite(samples);
            for(float v:samples) {
                maxAbs=std::max(maxAbs,std::fabs(v));
                energy+=v*v;
            }
        }
        assert(maxAbs>0.001f && energy>0.01f);
        engine.noteOff();
        for(int j=0;j<1000 && engine.active();++j) {
            engine.render(samples.data(),samples.size());
            checkFinite(samples);
        }
        assert(!engine.active());
        std::printf("Algorithm %d: peak %.3f energy %.2f OK\n",algorithm+1,maxAbs,energy);
    }
    // Ensure all combinations remain finite and bounded at high FM index,
    // feedback and pitch bend; no voice state or heap allocation needed.
    for(int wave=0;wave<8;++wave) {
        for(auto& op:engine.op) op.waveform=wave;
        for(int alg=0;alg<8;++alg) {
            engine.algorithm=alg;
            engine.modIndex=12.0f;
            engine.feedback=4.0f;
            engine.outputGain=1.5f;
            engine.noteOn(110,0.95f);
            for(int i=0;i<16;++i) {
                engine.render(samples.data(),samples.size(),2.0f);
                checkFinite(samples);
            }
            engine.noteOff();
            for(int i=0;i<150 && engine.active();++i) engine.render(samples.data(),samples.size());
            assert(!engine.active());
        }
    }
    std::puts("8 waveforms x 8 routings under extreme modulation: finite bounded output OK");
}
