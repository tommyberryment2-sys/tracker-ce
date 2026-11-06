// FM4 v1.1.2: per-voice Buchla 292-inspired analog-topology lowpass gate.
// Original implementation for FM4. This is NOT a circuit-calibrated component-level
// model; see README for modeling boundaries and the DAFx-13 source reference.
//
// Two matched photoresistor arms surround node x. C2 is shunted to ground at x;
// C1 is shunted to ground at output node y. The LPF position adds C3 between x
// and y (capacitive feedback); the LPG ('Both') position leaves C3 disconnected.
// Trapezoidal capacitor companions preserve the *capacitor voltages*, instead of
// changing coefficients of a direct-form biquad as resistance varies.
#pragma once
#include <algorithm>
#include <cmath>

namespace fm4 {
class Vactrol292 {
public:
    void reset() {
        light_=0.0f;
        vx_=vy_=0.0f;
        h1_=h2_=h3_=0.0f;
        lastMode_=-1;
    }
    void configure(int mode, float brightness, float envDepth, float response) {
        mode_=std::clamp(mode,0,2);
        brightness_=std::clamp(brightness,0.0f,1.0f);
        envDepth_=std::clamp(envDepth,0.0f,1.0f);
        const float r=std::clamp(response,0.0f,1.0f);
        // Empirical, per-voice vactrol memory: roughly 12 ms rising and ~250 ms
        // falling around the middle of the knob, with state-dependent speed.
        rise_ = 1.0f / (1.0f + 44100.0f*(0.004f+0.018f*r));
        fall_ = 1.0f / (1.0f + 44100.0f*(0.060f+0.55f*r));
    }
    float process(float input, float carrierEnvelope) {
        const float env=std::clamp(carrierEnvelope,0.0f,1.0f);
        const float target=brightness_*((1.0f-envDepth_)+envDepth_*env);
        // Current state affects response speed. LED illumination rises quickly;
        // the photoresistor returns more slowly after the LED goes dark.
        const float a=(target>light_?rise_:fall_)*(0.70f+0.50f*light_);
        light_+=a*(target-light_);
        light_=std::clamp(light_,0.0f,1.0f);
        // Preserve original FM4 VCA mode exactly: carrier ADSRs already supply
        // the clean amplitude envelope; no redundant second ADSR is applied.
        if(mode_==0) {lastMode_=0;return input;}

        // One shared optical state sets the CONDUCTANCE of both LDR arms.
        // Positive, bounded 1 kOhm..25 MOhm analogue-inspired range.
        const float l2=light_*light_;
        const float G=0.00000004f+0.00099996f*l2*l2;
        constexpr float fs=44100.0f;
        // Passive two-pole circuit: C1=1nF, C2=220pF; LPF adds C3=4.7nF.
        constexpr float c1=2.0f*fs*1.0e-9f;
        constexpr float c2=2.0f*fs*220.0e-12f;
        constexpr float c3=2.0f*fs*4.7e-9f;
        constexpr float gAlpha=1.0f/5.0e6f;
        const bool lowpass=(mode_==2);
        const float g3=lowpass?c3:0.0f;

        if(lastMode_!=mode_) {
            // Enter LPF with the feedback capacitor precharged to the current
            // x-y voltage. This avoids spuriously injecting a switching impulse.
            h3_=lowpass?c3*(vx_-vy_):0.0f;
            lastMode_=mode_;
        }
        // Nodal equations for x and y after trapezoidal companion substitution:
        // (2G + gC2 + gC3)*x + (-G-gC3)*y = G*vin + hC2 + hC3
        // (-G)*x + (G + gAlpha + gC1)*y = hC1
        const float A=2.0f*G+c2+g3;
        const float B=G+g3;
        const float D=G+gAlpha+c1;
        const float rhsX=G*input+h2_+(lowpass?h3_:0.0f);
        const float det=A*D-B*G;
        const float y=(A*h1_+G*rhsX)/det;
        const float x=(rhsX+B*y)/A;
        // Update actual circuit states, not DF-biquad delay samples.
        h1_=2.0f*c1*y-h1_;
        h2_=2.0f*c2*x-h2_;
        if(lowpass)h3_=2.0f*c3*(x-y)-h3_;
        vx_=x; vy_=y;
        return y;
    }
    float opticalStateForTest() const {return light_;}
private:
    int mode_=0, lastMode_=-1;
    float brightness_=0.8f, envDepth_=0.8f;
    float rise_=0.0f, fall_=0.0f, light_=0.0f;
    float vx_=0.0f,vy_=0.0f;
    // Capacitor companion histories: h = Gc * previous_voltage + previous_current.
    float h1_=0.0f,h2_=0.0f,h3_=0.0f;
};
} // namespace fm4
