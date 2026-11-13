#include "NoiseCompanionDevDSP.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace companion;
int main(){
    static_assert(kParamCount == 59);
    DevVoiceDSP v;
    v.core.base.useWhiteC=false;
    v.core.clock.setHostBpm(120.f);
    v.noteOn(60,.8f);
    float peak=0.f;
    for(int n=0;n<44100;++n){
        float x=v.tick();
        assert(std::isfinite(x));
        assert(std::fabs(x)<=1.f);
        peak=std::max(peak,std::fabs(x));
    }
    assert(peak>.01f);
    v.noteOff();
    for(int n=0;n<44100;++n){float x=v.tick();assert(std::isfinite(x));}
    assert(!v.active());
    // With A/B/C muted, output is exactly silent even when note envelope is open.
    v.noteOn(61,1.f);
    v.core.base.mute[0]=v.core.base.mute[1]=v.core.base.mute[2]=true;
    for(int n=0;n<100;++n)assert(std::fabs(v.tick())<1e-6f);
    v.core.base.mute[0]=v.core.base.mute[1]=false;
    // A modulation route actually alters the audio oscillator X control.
    DevVoiceDSP noRoute, withRoute;
    noRoute.core.base.level[2]=withRoute.core.base.level[2]=0.f;
    withRoute.core.mod.lfo[0].settings.shape=LfoWave::Square;
    withRoute.core.mod.lfo[0].settings.rateHz=5.f;
    withRoute.core.mod.lfo[0].settings.depth=1.f;
    withRoute.core.mod.routes[0][0]={Destination::XA,1.f};
    noRoute.noteOn(60,1.f);withRoute.noteOn(60,1.f);
    float diff=0;
    for(int n=0;n<44100;++n)diff+=std::fabs(noRoute.tick()-withRoute.tick());
    assert(diff>1.f);
    // The tempo setting for synchronized LFOs comes from the host-provided BPM.
    Clock a,b; a.bpm=120.f;b.bpm=60.f;
    Lfo u,t;u.settings.sync=t.settings.sync=true;
    assert(kBeatsPerCycle[9]==1.f);
    assert(u.settings.division==9);
    // Capture one-eighth of a second of audio; higher tempo causes more LFO travel.
    u.settings.shape=t.settings.shape=LfoWave::RampUp;
    float fast=0,slow=0;
    for(int n=0;n<5512;++n){fast=u.advance(a);slow=t.advance(b);a.advance();b.advance();}
    assert(fast>slow);
    GritNoise sparse,dense;
    for(int n=0;n<88200;++n){sparse.process(.05f,44100.f);dense.process(.9f,44100.f);}
    assert(dense.eventCount>20*sparse.eventCount);
    std::cout<<"PASS v0.2 host DSP: note/release, mute, independent modulation, BPM, grit density, finite output\n";
}
