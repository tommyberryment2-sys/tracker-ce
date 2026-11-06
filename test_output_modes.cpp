#include "../core/synth/engines/FM4DSP.h"
#include <cassert>
#include <array>
#include <cmath>
#include <cstdio>
static constexpr int N=128;
static float renderEnergy(fm4::Engine & e){
    std::array<float,N> a{}; float energy=0;
    e.noteOn(60,1);
    for(int b=0;b<200;++b){e.render(a.data(),N);
        for(auto x:a){assert(std::isfinite(x));assert(fabs(x)<=1.000001f);energy+=x*x;}}
    return energy;
}
int main(){
    fm4::Engine e;
    e.algorithm=7;e.modIndex=0;e.outputFold=0.8f;e.outputDrive=0.4f;
    for(int i=0;i<4;++i){e.op[i].level=i==0?1.0f:0.0f;e.op[i].attack=.005f;e.op[i].sustain=.5f;}
    e.outputMode=0;float vca=renderEnergy(e);assert(vca>0.05f);
    e.outputMode=1;e.stageCutoff=.4f;float lpg=renderEnergy(e);assert(lpg>0.001f);
    e.outputMode=2;float lpf=renderEnergy(e);assert(lpf>0.001f);
    assert(fabs(vca-lpg)>0.01f && fabs(vca-lpf)>0.01f);
    // Post-stage output gain must not modify the pre-folding signal's harmonic proportions.
    e.outputMode=0;e.outputGain=.5f;
    e.noteOn(60,1);std::array<float,N>a{},b{};e.render(a.data(),N);
    e.outputGain=1.0f;e.noteOn(60,1);e.render(b.data(),N);
    for(int i=0;i<N;++i) assert(fabs(b[i]-2*a[i])<.0003f);
    puts("Output modes, output independence, finite audio PASS");
}
