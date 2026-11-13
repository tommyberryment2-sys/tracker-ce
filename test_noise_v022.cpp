// Regression for instability caused by independently interpolating the
// state-variable filter normalization coefficient at high resonance.
// Unlike a guard that discards NaNs, this verifies the coefficient relation
// is preserved at *every* sample while rapidly changing cutoff and Q.
#include "NoiseCompanionCore.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

int main() {
    using namespace companion;
    constexpr float fs=44100.f;
    Filter2P f;
    uint32_t rng=0x9D4A2B77u;
    float peak=0.f;
    // Mix rapid cutoff steps, large jumps, and high-Q modulation.
    for(int n=0; n<44100*12; ++n) {
        if((n%32)==0) {
            float t = float(n)/fs;
            // Sweep logarithmically across almost the entire passband.
            float z = .5f+.5f*std::sin(t*21.37f);
            float cutoff=20.f*std::pow(925.f,z);
            // Sudden jumps to emulate extreme macro / FX automation.
            if((n/32)%17==0) cutoff=18000.f;
            if((n/32)%23==0) cutoff=80.f;
            float res=((n/32)%7==0)?1.f:.92f+.08f*(.5f+.5f*std::sin(t*5.47f));
            f.configure(cutoff,res,fs,32);
        }
        float impulse = ((n%1237)==0) ? .5f : 0.f;
        float in = .3f*randomBipolar(rng) + impulse;
        auto mode=Filter2P::Mode((n/44100)%3);
        float out=f.processPrepared(in,mode);
        assert(std::isfinite(out));
        assert(std::isfinite(f.ic1) && std::isfinite(f.ic2));
        assert(std::isfinite(f.a) && std::isfinite(f.g) && std::isfinite(f.k));
        float expectedA=1.f/(1.f+f.g*(f.g+f.k));
        assert(std::fabs(expectedA-f.a)<1.e-6f);
        peak=std::max(peak,std::fabs(out));
    }
    assert(peak>.001f);
    std::cout << "PASS v0.2.2: coefficient-invariant sweep, finite filter state, LP/BP/HP modes\n";
}
