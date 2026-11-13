#pragma once
// Original, standalone prototype DSP for the Tracker Mini Noise Plethora companion.
// No Befaco/VCV code is incorporated here. Not a Polyend firmware build.
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace companion {
constexpr float pi = 3.14159265358979323846f;
inline float clamp01(float x) { return std::clamp(x, 0.f, 1.f); }
inline float saturate(float x) { return std::tanh(x); }

// All public settings planned for the native patch editor; max numbered ID = 58.
// Append-only numbering once native patch serialization is implemented.
enum class Param : int {
    ProgramA, XA, YA, CutA, ResA, ModeA, LevelA,
    ProgramB, XB, YB, CutB, ResB, ModeB, LevelB,
    SourceC, GritQty, CutC, ResC, ModeC, LevelC,
    MasterLevel, PlayMode, Attack, Decay, Sustain, Release,
    L1Shape, L1Hz, L1Sync, L1Division, L1Depth, L1PhaseMode, L1PhaseOffset,
    L2Shape, L2Hz, L2Sync, L2Division, L2Depth, L2PhaseMode, L2PhaseOffset,
    Route1Destination, Route1Amount, Route2Destination, Route2Amount,
    Route3Destination, Route3Amount, Route4Destination, Route4Amount,
    Route5Destination, Route5Amount, Route6Destination, Route6Amount,
    Route7Destination, Route7Amount, Route8Destination, Route8Amount,
    MuteA, MuteB, MuteC
};
inline constexpr int kParamCount = static_cast<int>(Param::MuteC) + 1;
static_assert(kParamCount == 59, "Parameter map changed unexpectedly");
static_assert(kParamCount <= 64, "SDK parameter enumeration budget exceeded");

// Stable destinations for route slots; destination 0 explicitly means 'none'.
enum class Destination : int {
    None, XA, YA, XB, YB, CutA, ResA, CutB, ResB, CutC, ResC,
    GritQty, LevelA, LevelB, LevelC, MasterLevel
};
constexpr std::size_t kDestinationCount = 16;
constexpr std::size_t kRouteCount = 4; // per LFO, 8 total

// 14 user-selectable shapes. Random shapes are seeded and deterministic.
enum class LfoWave : int {
    Sine, Triangle, RampUp, RampDown, Square, Pulse25, Pulse75,
    ExpRise, ExpFall, HalfSine, RectifiedSine, SampleHold,
    SmoothRandom, FourStepRandom
};
enum class PhaseMode : int { Free, NoteRetrigger, Transport };

// Beats per cycle. 1 beat = 1/4 note in 4/4. Includes straight/dotted/triplet.
constexpr std::array<float, 21> kBeatsPerCycle = {
    0.125f, 0.1875f, 0.083333333f, // 1/32
    0.25f,  0.375f,  0.166666667f, // 1/16
    0.5f,   0.75f,   0.333333333f, // 1/8
    1.f,    1.5f,    0.666666667f, // 1/4
    2.f,    3.f,     1.333333333f, // 1/2
    4.f,    6.f,     2.666666667f, // 1 bar
    8.f,    12.f,    16.f            // 2,3,4 bars
};

struct Clock {
    float sampleRate = 44100.f;
    float bpm = 0.f; // 0 means no known host/clock tempo. No invented BPM.
    uint64_t elapsedSamples = 0;
    uint64_t previousTickSample = 0;
    uint64_t ticks = 0;
    bool hasTick = false;
    bool transportRunning = false;

    void setHostBpm(float value) { if (value > 10.f && value < 400.f) bpm = value; }
    void onStart() { transportRunning = true; ticks = 0; hasTick = false; previousTickSample = elapsedSamples; }
    void onStop() { transportRunning = false; }
    void onTick() {
        if (hasTick && elapsedSamples > previousTickSample) {
            const float measured = 60.f * sampleRate / (24.f * float(elapsedSamples - previousTickSample));
            if (measured >= 10.f && measured <= 400.f)
                bpm = bpm == 0.f ? measured : bpm * 0.85f + measured * 0.15f;
        }
        previousTickSample = elapsedSamples;
        hasTick = true;
        ++ticks;
    }
    float beatPosition() const {
        if (!hasTick) return 0.f;
        float subTick = 0.f;
        if (transportRunning && bpm > 0.f)
            subTick = std::clamp(float(elapsedSamples - previousTickSample) * bpm * 24.f / (60.f * sampleRate), 0.f, 1.f);
        return (float(ticks - 1) + subTick) / 24.f;
    }
    void advance() { ++elapsedSamples; }
};

inline uint32_t xorshift(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
inline float randomBipolar(uint32_t& s) { return float(xorshift(s) & 0xffffff) * (2.f / 16777215.f) - 1.f; }

struct LfoSettings {
    LfoWave shape = LfoWave::Sine;
    PhaseMode phaseMode = PhaseMode::Free;
    bool sync = false;
    unsigned division = 9; // 1/4 note
    float rateHz = 1.f, depth = 1.f, phaseOffset = 0.f;
};
struct Lfo {
    LfoSettings settings{};
    float phase = 0.f, oldRandom = 0.f, newRandom = 0.f;
    std::array<float,4> steps{};
    uint32_t rng = 0x8fa21a5du;
    void noteOn() { if (settings.phaseMode == PhaseMode::NoteRetrigger) phase = 0.f; }
    void transportStart() { if (settings.phaseMode == PhaseMode::Transport) phase = 0.f; }
    float waveform(float p) const {
        switch (settings.shape) {
        case LfoWave::Sine: return std::sin(2*pi*p);
        case LfoWave::Triangle: return 1.f-4.f*std::fabs(p-0.5f);
        case LfoWave::RampUp: return 2.f*p-1.f;
        case LfoWave::RampDown: return 1.f-2.f*p;
        case LfoWave::Square: return p<.5f ? 1.f:-1.f;
        case LfoWave::Pulse25: return p<.25f?1.f:-1.f;
        case LfoWave::Pulse75: return p<.75f?1.f:-1.f;
        case LfoWave::ExpRise: return 2.f*(std::exp(4.f*p)-1.f)/(std::exp(4.f)-1.f)-1.f;
        case LfoWave::ExpFall: return 1.f-2.f*(std::exp(4.f*p)-1.f)/(std::exp(4.f)-1.f);
        case LfoWave::HalfSine: return 2.f*std::max(0.f, std::sin(2*pi*p))-1.f;
        case LfoWave::RectifiedSine: return 2.f*std::fabs(std::sin(2*pi*p))-1.f;
        case LfoWave::SampleHold: return newRandom;
        case LfoWave::SmoothRandom: return oldRandom + (newRandom-oldRandom)*(p*p*(3.f-2.f*p));
        case LfoWave::FourStepRandom: return steps[std::min(3, int(p*4.f))];
        }
        return 0.f;
    }
    float advance(const Clock& clock) {
        const auto& s = settings;
        const bool clockAvailable = clock.bpm > 0.f;
        if (s.sync && !clockAvailable) return 0.f; // No silent hardcoded tempo.
        float hz = std::clamp(s.rateHz, 0.001f, 80.f);
        if (s.sync) hz = clock.bpm / (60.f * kBeatsPerCycle[std::min<unsigned>(20,s.division)]);
        const float incr = std::min(hz / clock.sampleRate, .49f);
        float oldPhase = phase;
        if (s.sync && s.phaseMode == PhaseMode::Transport && clock.hasTick) {
            phase = std::fmod(clock.beatPosition()/kBeatsPerCycle[std::min<unsigned>(20,s.division)],1.f);
        } else {
            phase += incr;
            if (phase >= 1.f) phase -= 1.f;
        }
        if (phase < oldPhase) {
            oldRandom = newRandom;
            newRandom = randomBipolar(rng);
            for (auto& step:steps) step=randomBipolar(rng);
        }
        float p = std::fmod(phase + s.phaseOffset,1.f);
        if (p < 0.f) p += 1.f;
        return waveform(p)*std::clamp(s.depth,0.f,1.f);
    }
};

struct Route { Destination destination = Destination::None; float amount = 0.f; };
struct Modulation {
    Lfo lfo[2];
    std::array<Route,kRouteCount> routes[2]{};
    void onNote() { for(auto& l:lfo) l.noteOn(); }
    void onStart() { for(auto& l:lfo) l.transportStart(); }
    std::array<float,kDestinationCount> advance(const Clock& clock) {
        std::array<float,kDestinationCount> values{};
        for (int n=0;n<2;++n) {
            const float x=lfo[n].advance(clock);
            for (const auto& r: routes[n]) {
                int d=int(r.destination);
                if (d>0 && d<int(kDestinationCount))
                    values[d] += x*std::clamp(r.amount,-1.f,1.f);
            }
        }
        return values;
    }
};

// Original noise source inspired by the mapped density control in VCV's code.
// It is NOT a bit-exact port of Befaco's gritty noise algorithm.
struct GritNoise {
    uint32_t rng = 0x473cb971u;
    float tail=0.f;
    uint64_t eventCount=0;
    static float densityHz(float g) { return 0.1f + 20000.f*clamp01(g)*clamp01(g); }
    float process(float grit, float sampleRate) {
        const float p = 1.f-std::exp(-densityHz(grit)/sampleRate);
        // Fixed decay; variable arrival rate rather than variable output gain.
        tail *= std::exp(-1.f/(sampleRate*.0007f));
        if ((float(xorshift(rng)&0xffffff)/16777216.f)<p) {
            tail += randomBipolar(rng);
            ++eventCount;
        }
        return saturate(tail*1.5f);
    }
};

// Stable zero-delay-feedback 2-pole prototype. NOT YET a validated AS3320 emulation.
struct Filter2P {
    float ic1=0.f, ic2=0.f;
    enum Mode { Lowpass=0, Bandpass=1, Highpass=2 };
    float process(float in, float fc, float res, Mode mode, float fs) {
        float f = std::clamp(fc,20.f, fs*.45f);
        float g = std::tan(pi*f/fs);
        float q = .707f + clamp01(res)*15.f;
        float k=1.f/q;
        float a=1.f/(1.f+g*(g+k));
        float v1=(ic1+g*(in-ic2))*a;
        float v2=ic2+g*v1;
        ic1=2.f*v1-ic1; ic2=2.f*v2-ic2;
        float hp=in-k*v1-v2;
        return mode==Lowpass?v2:mode==Bandpass?v1:hp;
    }
};

struct Controls {
    float xA=.5f,yA=.5f,xB=.5f,yB=.5f;
    float cutoff[3]={15000.f,15000.f,15000.f};
    float resonance[3]={0.f,0.f,0.f};
    int mode[3]={0,0,0};
    float level[3]={.5f,.5f,.5f};
    bool mute[3]={false,false,false};
    float master=.5f,grit=.4f;
    bool useWhiteC=false;
};
struct Frame { float a,b,c,mix; };
struct Engine {
    Clock clock;
    Modulation mod;
    GritNoise grit;
    Filter2P filters[3][2]; // Channel C intentionally cascades two poles, A/B one.
    uint32_t whiteRng=0xa91e542bu;
    Controls base{};
    void onNoteOn() { mod.onNote(); }
    void onStart() {clock.onStart(); mod.onStart();}
    void onStop() {clock.onStop();}
    void onClockTick() {clock.onTick();}
    static float withMod(float v, const std::array<float,kDestinationCount>& m, Destination d) {
        return clamp01(v+m[int(d)]);
    }
    float filtered(int idx,float x,float c,float res,int mode) {
        int m=std::clamp(mode,0,2);
        float y=filters[idx][0].process(x,c,res,Filter2P::Mode(m),clock.sampleRate);
        if(idx==2) y=filters[idx][1].process(y,c,res,Filter2P::Mode(m),clock.sampleRate);
        return y;
    }
    Frame process(float sampleA, float sampleB,
                  const std::array<float,kDestinationCount>* externalMod = nullptr) {
        const auto m=externalMod ? *externalMod : mod.advance(clock);
        float gritQ=withMod(base.grit,m,Destination::GritQty);
        float white=randomBipolar(whiteRng);
        float c=base.useWhiteC?white:grit.process(gritQ,clock.sampleRate);
        float cut[3]={base.cutoff[0],base.cutoff[1],base.cutoff[2]};
        for(int i=0;i<3;++i) {
            Destination d=Destination(int(Destination::CutA)+2*i);
            // Modulation for cutoff is in octave fractions over an 8-octave sweep.
            cut[i]=std::clamp(cut[i]*std::pow(2.f,m[int(d)]*8.f),20.f,clock.sampleRate*.45f);
        }
        float r[3]={base.resonance[0],base.resonance[1],base.resonance[2]};
        r[0]=withMod(r[0],m,Destination::ResA);
        r[1]=withMod(r[1],m,Destination::ResB);
        r[2]=withMod(r[2],m,Destination::ResC);
        auto fa=filtered(0,sampleA,cut[0],r[0],base.mode[0]);
        auto fb=filtered(1,sampleB,cut[1],r[1],base.mode[1]);
        auto fc=filtered(2,c,cut[2],r[2],base.mode[2]);
        float lv[3]={base.level[0],base.level[1],base.level[2]};
        lv[0]=base.mute[0]?0.f:withMod(lv[0],m,Destination::LevelA);
        lv[1]=base.mute[1]?0.f:withMod(lv[1],m,Destination::LevelB);
        lv[2]=base.mute[2]?0.f:withMod(lv[2],m,Destination::LevelC);
        float master=withMod(base.master,m,Destination::MasterLevel);
        float sum=(fa*lv[0]+fb*lv[1]+fc*lv[2])*master;
        // Conservative master soft clipping to contain resonant test excursions.
        Frame out{fa*lv[0],fb*lv[1],fc*lv[2],saturate(sum)};
        clock.advance();
        return out;
    }
};
}
