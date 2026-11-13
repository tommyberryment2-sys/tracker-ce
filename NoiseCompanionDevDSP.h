#pragma once
// ORIGINAL prototype audio sources ONLY: these are not Befaco algorithms.
// Native Tracker integration milestone for 3-channel mixing, two LFOs,
// Grit Qty, mono note lifecycle and macro-addressable parameters.
#include "NoiseCompanionCore.h"

namespace companion {

struct TestOscillator {
    float phase = 0.f, fmPhase = 0.f;
    float process(float hz, float x, float y, float sampleRate, bool second) {
        hz = std::clamp(hz, 20.f, sampleRate*.30f);
        // Distinct original prototype oscillators: ringed cluster / folded pulse.
        const float fmHz = hz*(.45f+7.5f*clamp01(y));
        fmPhase += fmHz/sampleRate;
        fmPhase -= std::floor(fmPhase);
        phase += hz/sampleRate;
        phase -= std::floor(phase);
        if (second) {
            const float pulse=(phase < (.1f+.8f*clamp01(x))) ? 1.f:-1.f;
            return .38f*pulse*std::sin(2.f*pi*fmPhase);
        }
        const float ring=std::sin(2.f*pi*fmPhase);
        return .38f*std::sin(2.f*pi*phase + 3.5f*clamp01(x)*ring);
    }
};

struct Envelope {
    enum Stage { Off, Attack, Decay, Sustain, Release };
    Stage stage = Off;
    float value=0.f, releaseStart=0.f;
    float attack=.005f, decay=.25f, sustain=.6f, release=.3f;
    bool oneShot=false;
    void noteOn() { stage=Attack; }
    void noteOff() { if(stage!=Off){releaseStart=value;stage=Release;} }
    bool active() const { return stage != Off; }
    float next(float fs) {
        switch(stage) {
        case Off: return 0.f;
        case Attack:
            value+=1.f/(std::max(.001f,attack)*fs);
            if(value>=1.f){value=1.f;stage=Decay;}
            break;
        case Decay:
            value-= (1.f-std::clamp(sustain,0.f,1.f))/(std::max(.005f,decay)*fs);
            if(value<=sustain){value=sustain;if(oneShot){releaseStart=value;stage=Release;}else stage=Sustain;}
            break;
        case Sustain: value=sustain;break;
        case Release:
            // Linear response follows the level at note-off, never causes a step.
            value-=releaseStart/(std::max(.005f,release)*fs);
            if(value<=.00001f){value=0.f;stage=Off;}
            break;
        }
        return clamp01(value);
    }
};

struct DevVoiceDSP {
    Engine core;
    Envelope amp;
    TestOscillator a,b;
    float noteHz=220.f;
    float velocity=1.f;
    bool hold=false;

    void noteOn(int midi,float vel) {
        const int n=std::clamp(midi,0,127);
        noteHz=440.f*std::pow(2.f,(float(n)-69.f)/12.f);
        velocity=clamp01(vel);
        core.onNoteOn();
        amp.noteOn();
    }
    void noteOff() { amp.noteOff(); }
    bool active() const {return amp.active();}
    void onStart() { core.onStart(); }
    void onClock() { core.onClockTick(); }
    float tick() {
        const auto mod=core.mod.advance(core.clock);
        const float x=core.withMod(core.base.xA,mod,Destination::XA);
        const float y=core.withMod(core.base.yA,mod,Destination::YA);
        const float xb=core.withMod(core.base.xB,mod,Destination::XB);
        const float yb=core.withMod(core.base.yB,mod,Destination::YB);
        // Pass one shared modulation frame to both generators and the mixer.
        const float inA=a.process(noteHz,x,y,core.clock.sampleRate,false);
        const float inB=b.process(noteHz*1.51f,xb,yb,core.clock.sampleRate,true);
        const auto f=core.process(inA,inB,&mod);
        return f.mix * amp.next(core.clock.sampleRate) * velocity;
    }
};
} // namespace companion
