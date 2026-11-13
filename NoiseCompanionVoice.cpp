// Noise Companion v0.2 native Tracker synth integration — DEVELOPMENT ONLY.
// This implementation contains ORIGINAL test oscillators, NOT Befaco programs.
#include "config.h"
#ifdef FEATURE_SYNTH
#include "NoiseCompanionVoice.h"
#include "NoiseCompanionDevDSP.h"
#include "PolySynth/sdk/SynthVoiceSDK.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>

namespace polysynth {
namespace {
using Id=companion::Param;

static auto formatPrototype=[](const Patch&,Patch::paramvalue_t){return std::string("TEST ONLY");};
static auto formatFilter=[](const Patch&,Patch::paramvalue_t v){
  constexpr std::array<const char*,3> names{"Lowpass","Bandpass","Highpass"};
  return std::string(names[std::clamp(int(std::lround(v)),0,2)]);
};
static auto formatSource=[](const Patch&,Patch::paramvalue_t v){
  return std::string(v>=.5f?"White":"Grit");
};
static auto formatPlay=[](const Patch&,Patch::paramvalue_t v){
  return std::string(v>=.5f?"Trigger":"Pitched");
};
static auto formatToggle=[](const Patch&,Patch::paramvalue_t v){
  return std::string(v>=.5f?"On":"Off");
};
static auto formatPhase=[](const Patch&,Patch::paramvalue_t v){
  constexpr std::array<const char*,3> names{"Free","Note","Transport"};
  return std::string(names[std::clamp(int(std::lround(v)),0,2)]);
};
static auto formatLfoWave=[](const Patch&,Patch::paramvalue_t v){
  constexpr std::array<const char*,14> names{
    "Sine","Triangle","Ramp Up","Ramp Down","Square","Pulse 25",
    "Pulse 75","Exp Rise","Exp Fall","Half Sine","Rect Sine",
    "Sample Hold","Smooth Random","Four Step"};
  return std::string(names[std::clamp(int(std::lround(v)),0,13)]);
};
static auto formatDivision=[](const Patch&,Patch::paramvalue_t v){
  constexpr std::array<const char*,21> names{
    "1/32","1/32.","1/32T", "1/16","1/16.","1/16T",
    "1/8","1/8.","1/8T", "1/4","1/4.","1/4T",
    "1/2","1/2.","1/2T", "1 Bar","1 Bar.","1 Bar T",
    "2 Bars","3 Bars","4 Bars"};
  return std::string(names[std::clamp(int(std::lround(v)),0,20)]);
};
static auto formatDestination=[](const Patch&,Patch::paramvalue_t v){
  constexpr std::array<const char*,16> names{
    "Off","X A","Y A","X B","Y B","Cut A","Res A",
    "Cut B","Res B","Cut C","Res C","Grit Qty",
    "Level A","Level B","Level C","Master"};
  return std::string(names[std::clamp(int(std::lround(v)),0,15)]);
};

class NoiseCompanionVoice final : public sdk::CustomSynthVoice<NoiseCompanionVoice,Id> {
public:
    companion::DevVoiceDSP dsp;
    int currentNote=60;
    bool isActive() final { return dsp.active(); }
    void onNoteOn(const InstrumentVoiceContext&, note_t note,float velocity,bool legato) final {
        currentNote=int(note);
        if (!legato || !dsp.active()) dsp.noteOn(currentNote, velocity);
        else dsp.noteHz = sdk::noteToFreq(note, pitchBendSemitones);
    }
    void onNoteOff(const InstrumentVoiceContext&, note_t) final {dsp.noteOff();}
    void onStart() final {dsp.onStart();}
    void onClock() final {dsp.onClock();}
    void renderVoiceAudio(const InstrumentVoiceContext& ctx,
                          std::optional<AudioBufferStereoRef>,AudioBufferStereoRef outputBuffer,
                          std::span<AudioBufferRef>) final {
        auto mono=outputBuffer[0];
        if(!isActive()) {std::fill(mono.begin(),mono.end(),0.f);return;}
        dsp.core.clock.setHostBpm(ctx.tempo);
        dsp.noteHz=sdk::noteToFreq(static_cast<note_t>(currentNote),pitchBendSemitones);
        for(auto& s:mono) s=dsp.tick();
    }
    void setParam(Id id, float x) {
        auto& e=dsp.core;
        switch(id) {
        case Id::ProgramA: case Id::ProgramB: break; // Reserved: original algorithms NOT implemented.
        case Id::XA: e.base.xA=companion::clamp01(x);break;
        case Id::YA: e.base.yA=companion::clamp01(x);break;
        case Id::XB: e.base.xB=companion::clamp01(x);break;
        case Id::YB: e.base.yB=companion::clamp01(x);break;
        case Id::CutA: e.base.cutoff[0]=x;break;
        case Id::CutB: e.base.cutoff[1]=x;break;
        case Id::CutC: e.base.cutoff[2]=x;break;
        case Id::ResA: e.base.resonance[0]=companion::clamp01(x);break;
        case Id::ResB: e.base.resonance[1]=companion::clamp01(x);break;
        case Id::ResC: e.base.resonance[2]=companion::clamp01(x);break;
        case Id::ModeA: e.base.mode[0]=std::clamp(int(std::lround(x)),0,2);break;
        case Id::ModeB: e.base.mode[1]=std::clamp(int(std::lround(x)),0,2);break;
        case Id::ModeC: e.base.mode[2]=std::clamp(int(std::lround(x)),0,2);break;
        case Id::LevelA: e.base.level[0]=companion::clamp01(x);break;
        case Id::LevelB: e.base.level[1]=companion::clamp01(x);break;
        case Id::LevelC: e.base.level[2]=companion::clamp01(x);break;
        case Id::SourceC: e.base.useWhiteC=(x>=.5f);break;
        case Id::GritQty: e.base.grit=companion::clamp01(x);break;
        case Id::MasterLevel: e.base.master=companion::clamp01(x);break;
        case Id::PlayMode: dsp.amp.oneShot=x>=.5f;break;
        case Id::Attack: dsp.amp.attack=x;break;
        case Id::Decay: dsp.amp.decay=x;break;
        case Id::Sustain: dsp.amp.sustain=x;break;
        case Id::Release: dsp.amp.release=x;break;
        case Id::MuteA: e.base.mute[0]=(x>=.5f);break;
        case Id::MuteB: e.base.mute[1]=(x>=.5f);break;
        case Id::MuteC: e.base.mute[2]=(x>=.5f);break;
        default: {
            const int n=int(id);
            const int l0=int(Id::L1Shape), l1=int(Id::L2Shape);
            if ((n>=l0 && n<l0+7)||(n>=l1 && n<l1+7)) {
                const int idx=(n>=l1)?1:0;
                auto& settings=e.mod.lfo[idx].settings;
                const int j=n-(idx?l1:l0);
                switch(j){
                  case 0:settings.shape=static_cast<companion::LfoWave>(std::clamp(int(std::lround(x)),0,13));break;
                  case 1:settings.rateHz=x;break;
                  case 2:settings.sync=x>=.5f;break;
                  case 3:settings.division=unsigned(std::clamp(int(std::lround(x)),0,20));break;
                  case 4:settings.depth=companion::clamp01(x);break;
                  case 5:settings.phaseMode=static_cast<companion::PhaseMode>(std::clamp(int(std::lround(x)),0,2));break;
                  case 6:settings.phaseOffset=companion::clamp01(x);break;
                }
            } else if(n>=int(Id::Route1Destination) && n<=int(Id::Route8Amount)) {
                const int slot=(n-int(Id::Route1Destination))/2;
                auto& route=e.mod.routes[slot/4][slot%4];
                if((n-int(Id::Route1Destination))%2==0)
                    route.destination=static_cast<companion::Destination>(std::clamp(int(std::lround(x)),0,15));
                else route.amount=std::clamp(x,-1.f,1.f);
            }
            break;
        }}
    }
    static void createMetadata(Patch&,Patch::Metadata&);
};

void NoiseCompanionVoice::createMetadata(Patch&,Patch::Metadata& meta) {
    using Param=Patch::Metadata::Param;
    using EditorSection=Patch::Metadata::EditorLayout::EditorSection;
    using EditorElement=Patch::Metadata::EditorLayout::EditorElement;
    constexpr int count=enumutil::EnumMax<Id>::value+1;
    static_assert(count==companion::kParamCount);
    meta.instrumentTypeId=static_cast<int>(InstrumentTypeId::kNoiseCompanionDevSynth);
    meta.outputChannels=Patch::Metadata::OutputChannels::kMono;
    meta.maxVoices=1; // development: no fake polyphony claim
    meta.params.resize(count);
    paramHandlers.resize(count);
    sdk::ParamHelper<NoiseCompanionVoice> helper(meta,paramHandlers);
    helper.addParam(int(Id::ProgramA), {.displayName="Test source", .defaultValue=0.0f, .minValue=0.0f, .maxValue=0.0f, .isEnum=true, .valueFormatter=formatPrototype},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::ProgramA,*p);});
    helper.addParam(int(Id::XA), {.displayName="X", .defaultValue=0.5f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::XA,*p);});
    helper.addParam(int(Id::YA), {.displayName="Y", .defaultValue=0.5f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::YA,*p);});
    helper.addParam(int(Id::CutA), {.displayName="Cutoff", .defaultValue=15000.0f, .minValue=25.0f, .maxValue=18000.0f, .editCurve=Param::Curve::kExponential, .valueFormatter=sdk::createFrequencyFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::CutA,*p);});
    helper.addParam(int(Id::ResA), {.displayName="Resonance", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::ResA,*p);});
    helper.addParam(int(Id::ModeA), {.displayName="Filter", .defaultValue=0.0f, .minValue=0.0f, .maxValue=2.0f, .isEnum=true, .valueFormatter=formatFilter},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::ModeA,*p);});
    helper.addParam(int(Id::LevelA), {.displayName="Level", .defaultValue=0.5f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::LevelA,*p);});
    helper.addParam(int(Id::ProgramB), {.displayName="Test source", .defaultValue=0.0f, .minValue=0.0f, .maxValue=0.0f, .isEnum=true, .valueFormatter=formatPrototype},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::ProgramB,*p);});
    helper.addParam(int(Id::XB), {.displayName="X", .defaultValue=0.5f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::XB,*p);});
    helper.addParam(int(Id::YB), {.displayName="Y", .defaultValue=0.5f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::YB,*p);});
    helper.addParam(int(Id::CutB), {.displayName="Cutoff", .defaultValue=15000.0f, .minValue=25.0f, .maxValue=18000.0f, .editCurve=Param::Curve::kExponential, .valueFormatter=sdk::createFrequencyFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::CutB,*p);});
    helper.addParam(int(Id::ResB), {.displayName="Resonance", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::ResB,*p);});
    helper.addParam(int(Id::ModeB), {.displayName="Filter", .defaultValue=0.0f, .minValue=0.0f, .maxValue=2.0f, .isEnum=true, .valueFormatter=formatFilter},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::ModeB,*p);});
    helper.addParam(int(Id::LevelB), {.displayName="Level", .defaultValue=0.5f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::LevelB,*p);});
    helper.addParam(int(Id::SourceC), {.displayName="Source", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f, .isEnum=true, .valueFormatter=formatSource},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::SourceC,*p);});
    helper.addParam(int(Id::GritQty), {.displayName="Grit Qty", .defaultValue=0.4f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::GritQty,*p);});
    helper.addParam(int(Id::CutC), {.displayName="Cutoff", .defaultValue=15000.0f, .minValue=25.0f, .maxValue=18000.0f, .editCurve=Param::Curve::kExponential, .valueFormatter=sdk::createFrequencyFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::CutC,*p);});
    helper.addParam(int(Id::ResC), {.displayName="Resonance", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::ResC,*p);});
    helper.addParam(int(Id::ModeC), {.displayName="Filter", .defaultValue=0.0f, .minValue=0.0f, .maxValue=2.0f, .isEnum=true, .valueFormatter=formatFilter},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::ModeC,*p);});
    helper.addParam(int(Id::LevelC), {.displayName="Level", .defaultValue=0.5f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::LevelC,*p);});
    helper.addParam(int(Id::MasterLevel), {.displayName="Master", .defaultValue=0.7f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::MasterLevel,*p);});
    helper.addParam(int(Id::PlayMode), {.displayName="Play mode", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f, .isEnum=true, .valueFormatter=formatPlay},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::PlayMode,*p);});
    helper.addParam(int(Id::Attack), {.displayName="Attack", .defaultValue=0.005f, .minValue=0.001f, .maxValue=4.0f, .editCurve=Param::Curve::kExponential, .valueFormatter=sdk::createTimeFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Attack,*p);});
    helper.addParam(int(Id::Decay), {.displayName="Decay", .defaultValue=0.25f, .minValue=0.005f, .maxValue=8.0f, .editCurve=Param::Curve::kExponential, .valueFormatter=sdk::createTimeFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Decay,*p);});
    helper.addParam(int(Id::Sustain), {.displayName="Sustain", .defaultValue=0.6f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Sustain,*p);});
    helper.addParam(int(Id::Release), {.displayName="Release", .defaultValue=0.3f, .minValue=0.005f, .maxValue=8.0f, .editCurve=Param::Curve::kExponential, .valueFormatter=sdk::createTimeFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Release,*p);});
    helper.addParam(int(Id::L1Shape), {.displayName="Shape", .defaultValue=0.0f, .minValue=0.0f, .maxValue=13.0f, .isEnum=true, .valueFormatter=formatLfoWave},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::L1Shape,*p);});
    helper.addParam(int(Id::L1Hz), {.displayName="Rate Hz", .defaultValue=1.0f, .minValue=0.01f, .maxValue=20.0f, .editCurve=Param::Curve::kExponential, .valueFormatter=sdk::createFrequencyFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::L1Hz,*p);});
    helper.addParam(int(Id::L1Sync), {.displayName="Tempo sync", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f, .isEnum=true, .valueFormatter=formatToggle},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::L1Sync,*p);});
    helper.addParam(int(Id::L1Division), {.displayName="Division", .defaultValue=9.0f, .minValue=0.0f, .maxValue=20.0f, .isEnum=true, .valueFormatter=formatDivision},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::L1Division,*p);});
    helper.addParam(int(Id::L1Depth), {.displayName="Depth", .defaultValue=1.0f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::L1Depth,*p);});
    helper.addParam(int(Id::L1PhaseMode), {.displayName="Phase mode", .defaultValue=0.0f, .minValue=0.0f, .maxValue=2.0f, .isEnum=true, .valueFormatter=formatPhase},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::L1PhaseMode,*p);});
    helper.addParam(int(Id::L1PhaseOffset), {.displayName="Phase offset", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::L1PhaseOffset,*p);});
    helper.addParam(int(Id::L2Shape), {.displayName="Shape", .defaultValue=0.0f, .minValue=0.0f, .maxValue=13.0f, .isEnum=true, .valueFormatter=formatLfoWave},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::L2Shape,*p);});
    helper.addParam(int(Id::L2Hz), {.displayName="Rate Hz", .defaultValue=1.0f, .minValue=0.01f, .maxValue=20.0f, .editCurve=Param::Curve::kExponential, .valueFormatter=sdk::createFrequencyFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::L2Hz,*p);});
    helper.addParam(int(Id::L2Sync), {.displayName="Tempo sync", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f, .isEnum=true, .valueFormatter=formatToggle},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::L2Sync,*p);});
    helper.addParam(int(Id::L2Division), {.displayName="Division", .defaultValue=9.0f, .minValue=0.0f, .maxValue=20.0f, .isEnum=true, .valueFormatter=formatDivision},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::L2Division,*p);});
    helper.addParam(int(Id::L2Depth), {.displayName="Depth", .defaultValue=1.0f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::L2Depth,*p);});
    helper.addParam(int(Id::L2PhaseMode), {.displayName="Phase mode", .defaultValue=0.0f, .minValue=0.0f, .maxValue=2.0f, .isEnum=true, .valueFormatter=formatPhase},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::L2PhaseMode,*p);});
    helper.addParam(int(Id::L2PhaseOffset), {.displayName="Phase offset", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f, .valueFormatter=sdk::createPercentFormatter()},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::L2PhaseOffset,*p);});
    helper.addParam(int(Id::Route1Destination), {.displayName="Route 1 To", .defaultValue=0.0f, .minValue=0.0f, .maxValue=15.0f, .isEnum=true, .valueFormatter=formatDestination},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route1Destination,*p);});
    helper.addParam(int(Id::Route1Amount), {.displayName="Route 1 Amt", .defaultValue=0.0f, .minValue=-1.0f, .maxValue=1.0f, .valueFormatter=[](const Patch&,Patch::paramvalue_t v){return fmt::format("{:+.2f}",v);}},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route1Amount,*p);});
    helper.addParam(int(Id::Route2Destination), {.displayName="Route 2 To", .defaultValue=0.0f, .minValue=0.0f, .maxValue=15.0f, .isEnum=true, .valueFormatter=formatDestination},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route2Destination,*p);});
    helper.addParam(int(Id::Route2Amount), {.displayName="Route 2 Amt", .defaultValue=0.0f, .minValue=-1.0f, .maxValue=1.0f, .valueFormatter=[](const Patch&,Patch::paramvalue_t v){return fmt::format("{:+.2f}",v);}},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route2Amount,*p);});
    helper.addParam(int(Id::Route3Destination), {.displayName="Route 3 To", .defaultValue=0.0f, .minValue=0.0f, .maxValue=15.0f, .isEnum=true, .valueFormatter=formatDestination},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route3Destination,*p);});
    helper.addParam(int(Id::Route3Amount), {.displayName="Route 3 Amt", .defaultValue=0.0f, .minValue=-1.0f, .maxValue=1.0f, .valueFormatter=[](const Patch&,Patch::paramvalue_t v){return fmt::format("{:+.2f}",v);}},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route3Amount,*p);});
    helper.addParam(int(Id::Route4Destination), {.displayName="Route 4 To", .defaultValue=0.0f, .minValue=0.0f, .maxValue=15.0f, .isEnum=true, .valueFormatter=formatDestination},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route4Destination,*p);});
    helper.addParam(int(Id::Route4Amount), {.displayName="Route 4 Amt", .defaultValue=0.0f, .minValue=-1.0f, .maxValue=1.0f, .valueFormatter=[](const Patch&,Patch::paramvalue_t v){return fmt::format("{:+.2f}",v);}},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route4Amount,*p);});
    helper.addParam(int(Id::Route5Destination), {.displayName="Route 5 To", .defaultValue=0.0f, .minValue=0.0f, .maxValue=15.0f, .isEnum=true, .valueFormatter=formatDestination},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route5Destination,*p);});
    helper.addParam(int(Id::Route5Amount), {.displayName="Route 5 Amt", .defaultValue=0.0f, .minValue=-1.0f, .maxValue=1.0f, .valueFormatter=[](const Patch&,Patch::paramvalue_t v){return fmt::format("{:+.2f}",v);}},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route5Amount,*p);});
    helper.addParam(int(Id::Route6Destination), {.displayName="Route 6 To", .defaultValue=0.0f, .minValue=0.0f, .maxValue=15.0f, .isEnum=true, .valueFormatter=formatDestination},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route6Destination,*p);});
    helper.addParam(int(Id::Route6Amount), {.displayName="Route 6 Amt", .defaultValue=0.0f, .minValue=-1.0f, .maxValue=1.0f, .valueFormatter=[](const Patch&,Patch::paramvalue_t v){return fmt::format("{:+.2f}",v);}},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route6Amount,*p);});
    helper.addParam(int(Id::Route7Destination), {.displayName="Route 7 To", .defaultValue=0.0f, .minValue=0.0f, .maxValue=15.0f, .isEnum=true, .valueFormatter=formatDestination},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route7Destination,*p);});
    helper.addParam(int(Id::Route7Amount), {.displayName="Route 7 Amt", .defaultValue=0.0f, .minValue=-1.0f, .maxValue=1.0f, .valueFormatter=[](const Patch&,Patch::paramvalue_t v){return fmt::format("{:+.2f}",v);}},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route7Amount,*p);});
    helper.addParam(int(Id::Route8Destination), {.displayName="Route 8 To", .defaultValue=0.0f, .minValue=0.0f, .maxValue=15.0f, .isEnum=true, .valueFormatter=formatDestination},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route8Destination,*p);});
    helper.addParam(int(Id::Route8Amount), {.displayName="Route 8 Amt", .defaultValue=0.0f, .minValue=-1.0f, .maxValue=1.0f, .valueFormatter=[](const Patch&,Patch::paramvalue_t v){return fmt::format("{:+.2f}",v);}},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::Route8Amount,*p);});
    helper.addParam(int(Id::MuteA), {.displayName="Mute A", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f, .isEnum=true, .valueFormatter=formatToggle},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::MuteA,*p);});
    helper.addParam(int(Id::MuteB), {.displayName="Mute B", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f, .isEnum=true, .valueFormatter=formatToggle},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::MuteB,*p);});
    helper.addParam(int(Id::MuteC), {.displayName="Mute C", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f, .isEnum=true, .valueFormatter=formatToggle},
       [](NoiseCompanionVoice& v,Patch::optional_paramvalue_t p){v.setParam(Id::MuteC,*p);});
    auto el=[](Id id) -> EditorElement{return {.type=EditorElement::Type::kParam,.id=int(id)};};
    meta.layoutFlat.sections={
      EditorSection{.name="Oscillator",.shortName="A",.elements={el(Id::ProgramA),el(Id::XA),el(Id::YA),el(Id::CutA),el(Id::ResA),el(Id::ModeA),el(Id::LevelA)}},
      EditorSection{.name="Oscillator",.shortName="B",.elements={el(Id::ProgramB),el(Id::XB),el(Id::YB),el(Id::CutB),el(Id::ResB),el(Id::ModeB),el(Id::LevelB)}},
      EditorSection{.name="Shaper",.shortName="C",.elements={el(Id::SourceC),el(Id::GritQty),el(Id::CutC),el(Id::ResC),el(Id::ModeC),el(Id::LevelC)}},
      EditorSection{.name="LFO",.shortName="LFO1",.elements={el(Id::L1Shape),el(Id::L1Hz),el(Id::L1Sync),el(Id::L1Division),el(Id::L1Depth),el(Id::L1PhaseMode),el(Id::L1PhaseOffset),el(Id::Route1Destination),el(Id::Route1Amount),el(Id::Route2Destination),el(Id::Route2Amount),el(Id::Route3Destination),el(Id::Route3Amount),el(Id::Route4Destination),el(Id::Route4Amount)}},
      EditorSection{.name="LFO",.shortName="LFO2",.elements={el(Id::L2Shape),el(Id::L2Hz),el(Id::L2Sync),el(Id::L2Division),el(Id::L2Depth),el(Id::L2PhaseMode),el(Id::L2PhaseOffset),el(Id::Route5Destination),el(Id::Route5Amount),el(Id::Route6Destination),el(Id::Route6Amount),el(Id::Route7Destination),el(Id::Route7Amount),el(Id::Route8Destination),el(Id::Route8Amount)}},
      EditorSection{.name="Voice",.shortName="MIX",.elements={el(Id::MasterLevel),el(Id::PlayMode),el(Id::Attack),el(Id::Decay),el(Id::Sustain),el(Id::Release),el(Id::MuteA),el(Id::MuteB),el(Id::MuteC)}},
    };
    meta.layout23=meta.layoutFlat;
    meta.initDefaultMacros=[](auto& macros){
        macros[0]=Patch::Metadata::Macro{.displayName="X A",.modifiers={{int(Id::XA),1.f}},.bipolar=true};
        macros[1]=Patch::Metadata::Macro{.displayName="Y B",.modifiers={{int(Id::YB),1.f}},.bipolar=true};
        macros[2]=Patch::Metadata::Macro{.displayName="LFO 1 Depth",.modifiers={{int(Id::L1Depth),1.f}},.bipolar=true};
        macros[3]=Patch::Metadata::Macro{.displayName="LFO 2 Depth",.modifiers={{int(Id::L2Depth),1.f}},.bipolar=true};
        macros[4]=Patch::Metadata::Macro{.displayName="LFO 1 Rate",.modifiers={{int(Id::L1Hz),.7f}},.bipolar=true};
        macros[5]=Patch::Metadata::Macro{.displayName="LFO 2 Rate",.modifiers={{int(Id::L2Hz),.7f}},.bipolar=true};
    };
}
} // namespace
std::unique_ptr<InstrumentType> createNoiseCompanionDevSynth() {
    return std::make_unique<InstrumentType>(InstrumentType{
        .name="NPL DEV", .createMetadata=&NoiseCompanionVoice::createMetadata,
        .createVoice=[](int) -> std::unique_ptr<InstrumentVoice>{
            return std::make_unique<NoiseCompanionVoice>();
        }});
}
} // namespace polysynth
#endif // FEATURE_SYNTH
