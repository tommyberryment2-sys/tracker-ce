// CPU-fix regression: high modulation, aggressive filter controls,
// six concurrent voices and immediate channel mute handling.
#include "NoiseCompanionDevDSP.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using namespace companion;
    std::array<DevVoiceDSP, 6> voices;
    for(int i=0; i<6; ++i) {
        auto& v=voices[i];
        v.core.clock.setHostBpm(163.f);
        v.core.mod.lfo[0].settings.shape=LfoWave::SmoothRandom;
        v.core.mod.lfo[0].settings.rateHz=18.f;
        v.core.mod.lfo[0].settings.depth=1.f;
        v.core.mod.lfo[1].settings.shape=LfoWave::RampUp;
        v.core.mod.lfo[1].settings.sync=true;
        v.core.mod.lfo[1].settings.division=3;
        v.core.mod.lfo[1].settings.depth=1.f;
        v.core.mod.routes[0][0]={Destination::CutA,1.f};
        v.core.mod.routes[0][1]={Destination::CutB,-1.f};
        v.core.mod.routes[0][2]={Destination::ResC,1.f};
        v.core.mod.routes[1][0]={Destination::GritQty,1.f};
        v.core.mod.routes[1][1]={Destination::CutC,1.f};
        v.core.base.cutoff[0]=9000.f;
        v.core.base.cutoff[1]=10000.f;
        v.core.base.cutoff[2]=12000.f;
        v.core.base.resonance[0]=.85f;
        v.core.base.resonance[1]=.99f;
        v.core.base.resonance[2]=1.f;
        v.noteOn(40+i*7,.9f);
    }
    float peak=0.f;
    for(int s=0;s<44100*3;++s) {
        if(s==100) {
            for(auto& v:voices) v.core.base.mute[2]=true;
        }
        for(auto& v:voices) {
            float out=v.tick();
            assert(std::isfinite(out));
            assert(std::fabs(out)<=1.00001f);
            peak=std::max(peak,std::fabs(out));
        }
    }
    assert(peak>.001f);
    for(auto& v: voices) v.noteOff();
    for(int s=0;s<44100;++s) for(auto& v: voices) {
        float out=v.tick();
        assert(std::isfinite(out));
    }
    for(auto& v: voices) assert(!v.active());
    std::cout << "PASS v0.2.1: six-voice stress, resonant filter modulation, no NaNs, note release\n";
}
