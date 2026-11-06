// FM4 four-operator phase-modulation DSP core, original code.
// No PolySynth dependency: designed for host tests and Tracker integration.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "FM4SineTable.h"
#include "FM4292.h"

namespace fm4 {
constexpr float kSampleRate = 44100.0f;
constexpr float kTwoPi = 6.2831853071795864769f;
constexpr float kInvTwoPi = 1.0f / kTwoPi;
constexpr float kInvPi = 0.3183098861837906715f;

// Ordered from upstream modulator (OP4) down to carrier (OP1).
// Bitmask for sources feeding each destination, source bit 0=OP1, 3=OP4.
struct Algorithm { std::array<uint8_t,4> sources; uint8_t carriers; };
constexpr std::array<Algorithm,8> kAlgorithms{{
    {{{2,4,8,0}}, 0x1},  // 4>3>2>1
    {{{6,0,8,0}}, 0x1},  // (4>3>1) + (2>1)
    {{{10,4,0,0}},0x1},  // (4>1) + (3>2>1)
    {{{4,0,8,0}}, 0x3},  // (4>3>1) + 2
    {{{2,0,8,0}}, 0x5},  // (4>3) + (2>1)
    {{{14,0,0,0}},0x1},  // (4>1) + (3>1) + (2>1)
    {{{0,0,8,0}}, 0x7},  // (4>3) + 2 + 1
    {{{0,0,0,0}}, 0xf},  // 4 + 3 + 2 + 1
}};

enum class Stage : uint8_t { Idle, Attack, Decay, Sustain, Release };
struct Operator {
    int waveform=0;
    float ratio=1.0f;
    bool fixedMode=false;
    float fixedHz=440.0f;
    float fineCents=0.0f;
    float level=0.0f;
    float attack=0.005f, decay=0.25f, sustain=0.8f, release=0.3f;
};
struct Runtime {
    float phase=0.0f, envelope=0.0f, releaseStep=0.0f;
    Stage stage=Stage::Idle;
};

inline float wrap(float p) { return p - std::floor(p); }
// 1024-segment linear-interpolated sine lookup keeps eight FM voices tractable on M7.
inline float sineLookup(float phase) {
    const float x=wrap(phase)*1024.0f;
    const int ix=static_cast<int>(x);
    const float frac=x-static_cast<float>(ix);
    return kSineTable[ix]+(kSineTable[ix+1]-kSineTable[ix])*frac;
}
// PolyBLEP correction for natural saw/square edge discontinuities. This reduces
// basic oscillator aliasing; strong phase-modulation sidebands still need listening
// and spectral testing on hardware before considering the instrument release-ready.
inline float polyBlep(float phase, float dt) {
    dt=std::clamp(dt,0.000001f,0.49f);
    if(phase<dt) {
        float x=phase/dt;
        return x+x-x*x-1.0f;
    }
    if(phase>1.0f-dt) {
        float x=(phase-1.0f)/dt;
        return x*x+x+x+1.0f;
    }
    return 0.0f;
}
// All eight waveforms are zero-DC (approximately) and share a single phase input.
inline float foldUnit(float x) {
    // Triangle-fold into [-1,1]; bounded, no heap allocations or fmod in audio path.
    while (x>1.0f) x=2.0f-x;
    while (x< -1.0f) x=-2.0f-x;
    return x;
}
// Waveform IDs 0-7 preserve v1 preset compatibility. New shapes are IDs 8-15.
inline float shape(int type, float unwrappedPhase, float dt) {
    const float p=wrap(unwrappedPhase);
    if(type==5) return p<0.5f ? 4.0f*p-1.0f : 3.0f-4.0f*p;
    if(type==6) return (2.0f*p-1.0f)-polyBlep(p,dt);
    if(type==7) return (p<0.5f ? 1.0f:-1.0f)
                        +polyBlep(p,dt)-polyBlep(wrap(p+0.5f),dt);
    if(type==15) return (p<0.25f ? 1.0f:-1.0f)
                         +polyBlep(p,dt)-polyBlep(wrap(p+0.75f),dt);
    if(type==4) return sineLookup(2.0f*p);
    const float s=sineLookup(p);
    switch(type) {
        case 1: return (std::max(0.0f,s)-kInvPi)*1.4669422f; // half-rectified sine
        case 2: return (std::fabs(s)-2.0f*kInvPi)*1.5707963f; // full-rectified sine
        case 3: return (p<0.25f ? s : 0.0f)-0.5f*kInvPi; // quarter sine
        case 8: return (std::min(0.0f,s)+kInvPi)*1.4669422f; // negative half wave
        case 9: return ((s>=0.0f?s:-0.5f*s)-1.5f*kInvPi)*1.2f; // asym rectifier
        case 10: return sineLookup(3.0f*p); // triple sine
        case 11: return (s+0.3333333f*sineLookup(3.0f*p))*0.75f; // odd harmonic
        case 12: return std::clamp(s*2.0f,-1.0f,1.0f); // clipped sine
        case 13: return foldUnit(s*2.4f); // static sine fold
        case 14: return std::round(s*4.0f)*0.25f; // quantized sine, digital edges
        default: return s;
    }
}

class Engine {
public:
    std::array<Operator,4> op{};
    int algorithm=0;
    float modIndex=2.5f;       // scale of source waveform in radians of phase modulation
    float feedback=0.0f;      // Legacy OP4 feedback; original preset parameter ID 34.
    std::array<float,3> extraFeedback{}; // OP1..OP3, appended parameter IDs.
    float outputGain=0.7f;  // Master gain AFTER fold/saturation and final stage.
    float foldDrive=1.0f;   // Static trim before folding, not an additional VCA.
    int outputMode=0;       // 0 VCA (original response), 1 LPG, 2 LPF
    float stageCutoff=0.8f; // LED brightness for LPF/LPG (native macro parameter 51)
    float stageEnvDepth=0.8f; // Carrier-envelope contribution to vactrol drive
    float lpgResponse=0.45f; // 0 fast, 1 slower vactrol-like closing
    float outputDrive=0.0f;  // 0..1, bypass at 0
    float outputFold=0.0f;   // 0..1, bypass at 0

    Engine() {
        op[0].level=0.85f;
        op[1].ratio=2.0f; op[1].level=0.55f;
        op[2].ratio=3.0f; op[2].level=0.45f;
        op[3].ratio=1.0f; op[3].level=0.45f;
    }
    void noteOn(int note, float vel, bool legato=false) {
        note_ = std::clamp(note,0,127);
        velocity_=std::clamp(vel,0.0f,1.0f);
        if(legato && active()) return;
        feedbackSamples_.fill(0.0f);
        lowpassGate_.reset();
        for(auto& r:rt_) { r.phase=0.0f; r.envelope=0.0f; r.stage=Stage::Attack; }
    }
    void noteOff() {
        for(int i=0;i<4;++i) {
            auto& r=rt_[i];
            if(r.stage==Stage::Idle) continue;
            r.stage=Stage::Release;
            r.releaseStep=r.envelope/(std::max(0.002f,op[i].release)*kSampleRate);
        }
    }
    bool active() const {
        for(const auto& r:rt_) if(r.stage!=Stage::Idle) return true;
        return false;
    }
    void render(float* out, std::size_t frames, float bendSemitones=0.0f) {
        if(!active()) { std::fill(out,out+frames,0.0f); return; }
        const float pitch=static_cast<float>(note_)-69.0f+bendSemitones;
        const float baseHz=440.0f*std::exp2(pitch/12.0f);
        std::array<float,4> inc{};
        for(int i=0;i<4;++i) {
            const float tunedHz=(op[i].fixedMode ? std::clamp(op[i].fixedHz,10.0f,8000.0f)
                             : baseHz*std::max(0.125f,op[i].ratio));
            const float hz=std::min(19000.0f,tunedHz * std::exp2(op[i].fineCents/1200.0f));
            inc[i]=hz/kSampleRate;
        }
        const auto& routing=kAlgorithms[std::clamp(algorithm,0,7)];
        float carrierNorm=0.0f;
        for(int i=0;i<4;++i) if(routing.carriers&(1<<i)) carrierNorm+=op[i].level;
        const float carrierScale=1.0f/(std::max(0.8f,carrierNorm));
        // Shared 292-style optoresistance; these controls remain native macros.
        // All four carrier ADSRs drive the optical state at AUDIO rate, not at
        // block boundaries. Each voice has its own capacitor and vactrol state.
        lowpassGate_.configure(outputMode,stageCutoff,stageEnvDepth,lpgResponse);
        for(std::size_t n=0;n<frames;++n) {
            float generated[4]{};
            // No recursive graph: OP4 calculated first, OP1 last.
            for(int i=3;i>=0;--i) {
                auto& r=rt_[i]; const auto& p=op[i];
                advanceEnvelope(r,p);
                float phaseOffset=0.0f;
                for(int src=i+1;src<4;++src)
                    if(routing.sources[i]&(1<<src)) phaseOffset+=generated[src];
                phaseOffset*=modIndex*kInvTwoPi;
                const float fbAmount=(i==3 ? feedback : extraFeedback[i]);
                phaseOffset+=(fbAmount*feedbackSamples_[i])*kInvTwoPi;
                // Advance the base phase separately from modulation (phase modulation).
                r.phase=wrap(r.phase+inc[i]);
                generated[i]=shape(p.waveform,r.phase+phaseOffset,inc[i])*r.envelope*p.level;
                feedbackSamples_[i]=generated[i];
            }
            float sample=0.0f;
            for(int i=0;i<4;++i) if(routing.carriers&(1<<i)) sample+=generated[i];
            sample*=carrierScale*velocity_;
            // Optional voice-local audio-rate wavefolder, followed by soft saturation.
            // Both default to bypass, making saved v1 patches sound unchanged.
            if(outputFold>0.0f) {
                const float mix=std::clamp(outputFold,0.0f,1.0f);
                const float folded=foldUnit(std::clamp(sample*std::clamp(foldDrive,0.25f,4.0f),-4.0f,4.0f)*(1.0f+5.0f*mix));
                sample=sample+(folded-sample)*mix;
            }
            if(outputDrive>0.0f) {
                const float mix=std::clamp(outputDrive,0.0f,1.0f);
                const float x=sample*(1.0f+8.0f*mix);
                const float saturated=x/(1.0f+std::fabs(x));
                sample=sample+(saturated-sample)*mix;
            }
            float carrierEnvelope=0.0f;
            for(int i=0;i<4;++i) if(routing.carriers&(1<<i))
                carrierEnvelope+=rt_[i].envelope*op[i].level;
            carrierEnvelope=std::clamp(carrierEnvelope / std::max(0.8f,carrierNorm)
                                       *velocity_,0.0f,1.0f);
            sample=lowpassGate_.process(sample,carrierEnvelope);
            out[n]=std::clamp(sample*outputGain,-1.0f,1.0f);
        }
    }
private:
    std::array<Runtime,4> rt_{};
    int note_=60;
    float velocity_=1.0f;
    std::array<float,4> feedbackSamples_{};
    Vactrol292 lowpassGate_{};

    static void advanceEnvelope(Runtime& r, const Operator& p) {
        switch(r.stage) {
            case Stage::Attack:
                r.envelope+=1.0f/(std::max(0.001f,p.attack)*kSampleRate);
                if(r.envelope>=1.0f) { r.envelope=1.0f; r.stage=Stage::Decay; }
                break;
            case Stage::Decay:
                r.envelope-=(1.0f-std::clamp(p.sustain,0.0f,1.0f)) /
                             (std::max(0.002f,p.decay)*kSampleRate);
                if(r.envelope<=p.sustain) {
                    r.envelope=std::max(0.0f,p.sustain);
                    r.stage=(r.envelope>0.0f)?Stage::Sustain:Stage::Idle;
                }
                break;
            case Stage::Sustain: r.envelope=std::clamp(p.sustain,0.0f,1.0f); break;
            case Stage::Release:
                r.envelope-=r.releaseStep;
                if(r.envelope<=0.00001f) {r.envelope=0.0f;r.stage=Stage::Idle;}
                break;
            case Stage::Idle: r.envelope=0.0f; break;
        }
    }
};
} // namespace fm4
