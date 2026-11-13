#pragma once
// Original C++ experimental DSP, not code taken from Befaco or VCV Rack.
// AS3320-inspired FOUR-pole voltage controlled filter approximation.
// This is NOT a circuit-exact model of the Noise Plethora hardware.
// TPT one-pole sections with four-stage resonant negative feedback.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace companion {
class Filter3320Experimental {
public:
    enum Mode : int { Lowpass=0, Bandpass=1, Highpass=2 };

    void reset() { z_ = {}; alpha_=0.2f; resonance_=0.f; da_=dr_=0.f; ramp_=0; configured_=false; }

    void configure(float cutoffHz, float resonance, float sampleRate, int smoothingSamples=32) {
        if (!(std::isfinite(cutoffHz)&&std::isfinite(resonance)&&std::isfinite(sampleRate)) || sampleRate < 1000.f) {
            return; // preserve previous stable coefficients
        }
        constexpr float pi = 3.14159265358979323846f;
        const float fc = std::clamp(cutoffHz, 15.f, 0.40f*sampleRate);
        const float g=std::tan(pi*fc/sampleRate);
        const float newAlpha=g/(1.f+g);
        // max feedback approximates 3320 resonance range; should be measured on Mini.
        const float newRes=std::clamp(resonance,0.f,1.f)*4.05f;
        if(!configured_ || smoothingSamples<1) {
            alpha_=newAlpha;resonance_=newRes;ramp_=0;configured_=true;
        } else {
            ramp_=std::clamp(smoothingSamples,1,4096);
            da_=(newAlpha-alpha_)/float(ramp_);
            dr_=(newRes-resonance_)/float(ramp_);
        }
    }
    float processPrepared(float signal, Mode mode) {
        if(ramp_>0){ alpha_+=da_; resonance_+=dr_; --ramp_; }
        const float a=std::clamp(alpha_,0.0001f,.85f);
        const float a2=a*a, a3=a2*a, a4=a2*a2;
        // All 4 pole outputs are affine in the input. Solve the closed-loop
        // relationship before applying bounded nonlinear input drive.
        const float b=1.f-a;
        const float feedthrough=b*(a3*z_[0] + a2*z_[1] + a*z_[2] + z_[3]);
        const float in=std::clamp(signal,-4.f,4.f);
        const float u=(in - resonance_*feedthrough)/(1.f+resonance_*a4);
        float x=std::tanh(u*1.2f)/1.2f; // saturates the feedback drive
        std::array<float,4> taps{};
        for(int i=0;i<4;++i) {
            const float y=a*x+b*z_[i];
            z_[i]=2.f*y-z_[i];
            x=y;
            taps[i]=y;
        }
        // Tap mixes approximate four-pole LP/BP/HP response.
        // Different modes are NOT a validated recreation of the board switch.
        float out=0.f;
        switch(mode){
            case Lowpass: out=taps[3]; break;
            case Bandpass: out=4.f*(taps[1]-2.f*taps[2]+taps[3]); break;
            case Highpass: out=in-4.f*taps[0]+6.f*taps[1]-4.f*taps[2]+taps[3]; break;
            default: out=taps[3]; break;
        }
        return std::tanh(out*.8f); // bounded output at high resonance
    }
    float process(float in, float fc, float res, Mode mode, float fs) {
        configure(fc,res,fs,0);return processPrepared(in,mode);
    }
    std::size_t memoryBytes() const { return sizeof(*this); }
private:
    std::array<float,4> z_{};
    float alpha_=0.2f,resonance_=0.f,da_=0.f,dr_=0.f;
    int ramp_=0;
    bool configured_=false;
};
}
