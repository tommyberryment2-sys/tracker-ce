// Host regression for the shared-vactrol 292-inspired filter topology.
#include "../core/synth/engines/FM4DSP.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
static constexpr float sr=44100.0f;
static constexpr float pi=3.14159265358979323846f;
static float gain(int mode,float frequency,float ledDrive, float env=.3f) {
    fm4::Vactrol292 gate;
    gate.reset();gate.configure(mode,ledDrive,0.0f,0.45f);
    // 2 seconds warm-up to settle optical memory and filter states, 1 sec measurement.
    double eIn=0,eOut=0;
    for(int i=0;i<3*44100;++i){
        const float x=0.3f*std::sin(2.0f*pi*frequency*i/sr);
        const float y=gate.process(x,env);
        assert(std::isfinite(y) && std::abs(y)<3.0f);
        if(i>2*44100){eIn+=double(x)*x;eOut+=double(y)*y;}
    }
    return std::sqrt(eOut/eIn);
}
static std::vector<float> render(int mode,float sustain) {
    fm4::Engine e;
    e.algorithm=7;e.modIndex=0.0f;e.outputFold=0.0f;e.outputDrive=0.0f;
    e.outputMode=mode;e.stageCutoff=0.58f;e.stageEnvDepth=0.85f;
    e.lpgResponse=0.45f;e.outputGain=0.7f;
    for(int i=0;i<4;++i){
        e.op[i].level=i==0?1.0f:0.0f;e.op[i].waveform=7;
        e.op[i].attack=.003f;e.op[i].decay=.12f;
        e.op[i].sustain=sustain;e.op[i].release=.25f;
    }
    e.noteOn(60,1.0f);
    std::vector<float> a(220*128);
    for(int b=0;b<220;++b)e.render(a.data()+b*128,128);
    for(float x:a)assert(std::isfinite(x) && std::abs(x)<=1.00001f);
    return a;
}
static double difference(const std::vector<float>& x,const std::vector<float>& y){
    double s=0;for(size_t i=0;i<x.size();++i)s+=std::abs(double(x[i])-double(y[i]));
    return s/x.size();
}
static void memory(){
    fm4::Vactrol292 g;g.reset();g.configure(1,1.0f,1.0f,0.45f);
    for(int i=0;i<44100;++i) g.process(0.0f,1.0f);
    const float on=g.opticalStateForTest();assert(on>.90f);
    for(int i=0;i<4410;++i)g.process(0.0f,0.0f);
    const float after100ms=g.opticalStateForTest();
    assert(after100ms>0.30f && after100ms<on);
    for(int i=0;i<44100;++i)g.process(0.0f,0.0f);
    assert(g.opticalStateForTest()<0.10f);
}
int main(){
    memory();
    const float lpgLow=gain(1,220.0f,.27f), lpgHigh=gain(1,3600.0f,.27f);
    const float lpfLow=gain(2,220.0f,.27f), lpfHigh=gain(2,3600.0f,.27f);
    std::printf("LPG 220Hz=%.4f 3.6kHz=%.4f | LPF 220Hz=%.4f 3.6kHz=%.4f\n",lpgLow,lpgHigh,lpfLow,lpfHigh);
    // The switched-in LPF capacitor changes frequency response (not a gain taper).
    assert(std::abs(lpgHigh-lpfHigh)>0.06f);
    assert(std::abs(lpgHigh/lpgLow-lpfHigh/lpfLow)>0.10f);
    const auto vca=render(0,.3f);
    const auto lpf=render(2,.3f);
    const auto lpg=render(1,.3f);
    std::printf("waveform diff: LPF vs LPG %.5f; VCA vs LPG %.5f\n",difference(lpf,lpg),difference(vca,lpg));
    assert(difference(lpf,lpg)>.002);
    assert(difference(vca,lpg)>.005);
    for(int mode=0;mode<3;++mode){
        const auto aud=render(mode,1.0f);
        double energy=0;for(float x:aud)energy+=double(x)*x;
        assert(energy>0.005);
    }
    // Repeated mode switching and sweeps of all native controls cannot create NaNs.
    fm4::Vactrol292 gate;
    for(int n=0;n<300000;++n){
        if(n%113==0)gate.configure((n/113)%3,float((n/17)%101)/100.0f,
                                  float((n/19)%101)/100.0f,float((n/23)%101)/100.0f);
        const float y=gate.process(std::sin(n*.17f),float((n/31)%101)/100.0f);
        assert(std::isfinite(y) && std::fabs(y)<8.0f);
    }
    std::puts("FM4 292-inspired LPG/LPF topology, optical memory, mode sweep: PASS");
}
