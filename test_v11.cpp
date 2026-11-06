// FM4 v1.1 regression suite: 16 waves, per-op feedback, fixed pitch, shapers.
// Host DSP verification only. Firmware compile and hardware tests are separate gates.
#include "../core/synth/engines/FM4DSP.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

using fm4::Engine;
static constexpr size_t kFrames=128;

static std::vector<float> audio(Engine& e,int note,int blocks=160) {
    std::vector<float> out(blocks*kFrames);
    e.noteOn(note,1.0f);
    for(int b=0;b<blocks;++b) e.render(out.data()+b*kFrames,kFrames);
    for(float v:out) { assert(std::isfinite(v)); assert(std::fabs(v)<=1.000001f); }
    return out;
}
static double difference(const std::vector<float>& a,const std::vector<float>& b) {
    assert(a.size()==b.size()); double s=0;
    for(size_t i=0;i<a.size();++i) s+=std::abs(a[i]-b[i]);
    return s/static_cast<double>(a.size());
}
static double frequency(const std::vector<float>& a) {
    // Count zero crossings after attack; close for pure sine at 44.1k.
    const size_t start=6000; int count=0;
    for(size_t i=start+1;i<a.size();++i) if(a[i-1]<=0.0f && a[i]>0.0f) ++count;
    return count*fm4::kSampleRate/static_cast<double>(a.size()-start);
}
int main() {
    Engine e;
    e.algorithm=7; e.modIndex=0; e.feedback=0;
    for(int i=0;i<4;++i) { e.op[i].level=i==0 ? 1.0f:0.0f; e.op[i].attack=0.001f; e.op[i].sustain=1.0f; }
    e.outputGain=0.7f;
    std::vector<std::vector<float>> waves;
    for(int wave=0;wave<16;++wave) {
        e.op[0].waveform=wave;
        auto sample=audio(e,69);
        double peak=0; for(float v:sample) peak=std::max(peak,static_cast<double>(std::abs(v)));
        assert(peak>0.05); waves.push_back(std::move(sample));
    }
    // Verify every newly added waveform differs numerically from a pure sine.
    for(int w=8;w<16;++w) assert(difference(waves[0],waves[w])>0.015);

    // Ratio mode follows the note; fixed mode ignores note but not fine tuning.
    e.op[0].waveform=0; e.op[0].ratio=1; e.op[0].fixedMode=false;
    double r69=frequency(audio(e,69)), r81=frequency(audio(e,81));
    assert(r69>425 && r69<455); assert(r81>860 && r81<900);
    e.op[0].fixedMode=true; e.op[0].fixedHz=220.0f;
    double f69=frequency(audio(e,69)), f81=frequency(audio(e,81));
    assert(std::abs(f69-220.0)<7.0 && std::abs(f81-f69)<2.0);
    e.op[0].fineCents=1200.0f; // DSP accepts pitch shift, UI restricts to +-100ct.
    double doubled=frequency(audio(e,69));
    assert(std::abs(doubled-440.0)<9.0);
    e.op[0].fineCents=0;

    // Each operator's feedback must affect its own output even when independent.
    e.op[0].fixedMode=false; e.op[0].ratio=1;
    for(int op=0;op<4;++op) {
        for(int j=0;j<4;++j) e.op[j].level=j==op?1.0f:0.0f;
        e.feedback=0; e.extraFeedback={0,0,0};
        auto clean=audio(e,60);
        if(op==3) e.feedback=3.0f; else e.extraFeedback[op]=3.0f;
        auto selfMod=audio(e,60);
        assert(difference(clean,selfMod)>0.01);
    }

    // Drive and fold both change the signal and are bypassed by default.
    for(int i=0;i<4;++i) e.op[i].level=i==0?1.0f:0.0f;
    e.feedback=0; e.extraFeedback={0,0,0};
    e.outputDrive=0; e.outputFold=0;
    auto dry=audio(e,60);
    e.outputDrive=0.85f;
    auto driven=audio(e,60);
    assert(difference(dry,driven)>0.03);
    e.outputDrive=0; e.outputFold=0.9f;
    auto folded=audio(e,60);
    assert(difference(dry,folded)>0.03);
    e.outputDrive=0.95f;

    // Stress every waveform and FM algorithm with all feedback loops enabled.
    for(int alg=0;alg<8;++alg) for(int wave=0;wave<16;++wave) {
        e.algorithm=alg; e.modIndex=12; e.feedback=4; e.extraFeedback={4,4,4};
        e.outputDrive=0.95f; e.outputFold=0.95f;
        for(int i=0;i<4;++i){e.op[i].waveform=wave;e.op[i].level=0.9f;
            e.op[i].fixedMode=(i%2)==1;e.op[i].fixedHz=10.0f+1000.0f*i;}
        (void)audio(e,105,10);
        e.noteOff();
        std::array<float,kFrames> tail{};
        for(int b=0;b<1000 && e.active();++b) e.render(tail.data(),tail.size());
        assert(!e.active());
    }
    std::puts("FM4 v1.1: 16 waveforms, 4 feedback loops, fixed pitch, drive/fold, extremes PASS");
}
