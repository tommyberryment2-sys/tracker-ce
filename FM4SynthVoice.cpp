#include "config.h"
#ifdef FEATURE_SYNTH
#include "FM4SynthVoice.h"
#include "FM4DSP.h"
#include "PolySynth/sdk/SynthVoiceSDK.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace polysynth {
namespace {

// Keep parameter IDs stable: saved patches store numeric IDs.
enum class FM4ParamId : int {
    kOp1Wave=0, kOp1Ratio, kOp1Fine, kOp1Level, kOp1Attack, kOp1Decay, kOp1Sustain, kOp1Release,
    kOp2Wave, kOp2Ratio, kOp2Fine, kOp2Level, kOp2Attack, kOp2Decay, kOp2Sustain, kOp2Release,
    kOp3Wave, kOp3Ratio, kOp3Fine, kOp3Level, kOp3Attack, kOp3Decay, kOp3Sustain, kOp3Release,
    kOp4Wave, kOp4Ratio, kOp4Fine, kOp4Level, kOp4Attack, kOp4Decay, kOp4Sustain, kOp4Release,
    kAlgorithm, kModIndex, kFeedback, kOutputGain, // v1 IDs 32-35: DO NOT CHANGE
    kOp1Feedback, kOp2Feedback, kOp3Feedback,      // New IDs 36-38
    kOp1FreqMode, kOp2FreqMode, kOp3FreqMode, kOp4FreqMode, // 39-42
    kOp1FixedHz, kOp2FixedHz, kOp3FixedHz, kOp4FixedHz, // 43-46
    kOutputDrive, kOutputFold, // 47-48
    kFoldDrive, kOutputMode, kStageCutoff, kStageEnvDepth, kLpgResponse // 49-53
};
static constexpr std::array<const char*,16> kWaveNames{
    "Sine", "Half Sine", "Full Rect", "Quarter Sine",
    "Double Sine", "Triangle", "Saw", "Square",
    "Neg Half", "Asym Rect", "Triple Sine", "Odd Harm",
    "Clip Sine", "Fold Sine", "Quant Sine", "Pulse 25"
};
static constexpr std::array<const char*,8> kAlgorithmNames{
    "4>3>2>1", "4>3>1 + 2>1", "4>1 + 3>2>1", "4>3>1 + 2",
    "4>3 + 2>1", "4,3,2 > 1", "4>3 + 2 + 1", "4 + 3 + 2 + 1"
};
static auto formatWave = [](const Patch&, Patch::paramvalue_t v) {
    return std::string(kWaveNames[std::clamp<int>(static_cast<int>(std::lround(v)),0,15)]);
};
static auto formatAlgorithm = [](const Patch&, Patch::paramvalue_t v) {
    return std::string(kAlgorithmNames[std::clamp<int>(static_cast<int>(std::lround(v)),0,7)]);
};
static auto formatRatio = [](const Patch&, Patch::paramvalue_t v) {
    return fmt::format("{:.3f} x",v);
};
static auto formatOutputMode = [](const Patch&, Patch::paramvalue_t v) {
    static constexpr std::array<const char*,3> names{"VCA", "LPG", "LPF"};
    return std::string(names[std::clamp<int>(static_cast<int>(std::lround(v)),0,2)]);
};
static auto formatFreqMode = [](const Patch&, Patch::paramvalue_t v) {
    return std::string(v>=0.5f ? "Fixed Hz" : "Ratio");
};

class FM4SynthVoice : public sdk::CustomSynthVoice<FM4SynthVoice, FM4ParamId> {
public:
    fm4::Engine dsp;
    bool isActive() final { return dsp.active(); }
    void onNoteOn(const InstrumentVoiceContext&, note_t note, float velocity, bool legato) final {
        dsp.noteOn(static_cast<int>(note), velocity, legato);
    }
    void onNoteOff(const InstrumentVoiceContext&, note_t) final { dsp.noteOff(); }
    void renderVoiceAudio(const InstrumentVoiceContext&,
                          std::optional<AudioBufferStereoRef>,
                          AudioBufferStereoRef outputBuffer,
                          std::span<AudioBufferRef>) final {
        const auto mono = outputBuffer[0];
        dsp.render(mono.data(), mono.size(), pitchBendSemitones);
    }
    static void createMetadata(Patch& patch, Patch::Metadata& metadata);
};

// Helper macro intentionally uses literal operator indexes, so every lambda is capture-free.
// The SDK requires stateless callbacks in its precomputed handler table.
#define FM4_ADD_OP(PREFIX, IDX, DEF_RATIO, DEF_LEVEL) \
    helper.addParam(static_cast<int>(FM4ParamId::k##PREFIX##Wave), \
       {.displayName="Waveform", .defaultValue=0.0f, .minValue=0.0f, .maxValue=15.0f, \
        .isEnum=true, .valueFormatter=formatWave}, \
       [](FM4SynthVoice& v, Patch::optional_paramvalue_t p) { \
           v.dsp.op[IDX].waveform=std::clamp<int>(static_cast<int>(std::lround(*p)),0,15); }); \
    helper.addParam(static_cast<int>(FM4ParamId::k##PREFIX##Ratio), \
       {.displayName="Ratio", .defaultValue=DEF_RATIO, .minValue=0.125f, .maxValue=16.0f, \
        .editCurve=Param::Curve::kExponential, .valueFormatter=formatRatio}, \
       [](FM4SynthVoice& v, Patch::optional_paramvalue_t p) { v.dsp.op[IDX].ratio=*p; }); \
    helper.addParam(static_cast<int>(FM4ParamId::k##PREFIX##Fine), \
       {.displayName="Fine Tune", .defaultValue=0.0f, .minValue=-100.0f, .maxValue=100.0f, \
        .valueFormatter=sdk::createCentsFormatter()}, \
       [](FM4SynthVoice& v, Patch::optional_paramvalue_t p) { v.dsp.op[IDX].fineCents=*p; }); \
    helper.addParam(static_cast<int>(FM4ParamId::k##PREFIX##Level), \
       {.displayName="Level", .defaultValue=DEF_LEVEL, .minValue=0.0f, .maxValue=1.0f, \
        .valueFormatter=sdk::createPercentFormatter()}, \
       [](FM4SynthVoice& v, Patch::optional_paramvalue_t p) { v.dsp.op[IDX].level=*p; }); \
    helper.addParam(static_cast<int>(FM4ParamId::k##PREFIX##Attack), \
       {.displayName="Attack", .defaultValue=0.005f, .minValue=0.001f, .maxValue=5.0f, \
        .editCurve=Param::Curve::kExponential, .valueFormatter=sdk::createTimeFormatter()}, \
       [](FM4SynthVoice& v, Patch::optional_paramvalue_t p) { v.dsp.op[IDX].attack=*p; }); \
    helper.addParam(static_cast<int>(FM4ParamId::k##PREFIX##Decay), \
       {.displayName="Decay", .defaultValue=0.25f, .minValue=0.005f, .maxValue=8.0f, \
        .editCurve=Param::Curve::kExponential, .valueFormatter=sdk::createTimeFormatter()}, \
       [](FM4SynthVoice& v, Patch::optional_paramvalue_t p) { v.dsp.op[IDX].decay=*p; }); \
    helper.addParam(static_cast<int>(FM4ParamId::k##PREFIX##Sustain), \
       {.displayName="Sustain", .defaultValue=0.8f, .minValue=0.0f, .maxValue=1.0f, \
        .valueFormatter=sdk::createPercentFormatter()}, \
       [](FM4SynthVoice& v, Patch::optional_paramvalue_t p) { v.dsp.op[IDX].sustain=*p; }); \
    helper.addParam(static_cast<int>(FM4ParamId::k##PREFIX##Release), \
       {.displayName="Release", .defaultValue=0.3f, .minValue=0.005f, .maxValue=8.0f, \
        .editCurve=Param::Curve::kExponential, .valueFormatter=sdk::createTimeFormatter()}, \
       [](FM4SynthVoice& v, Patch::optional_paramvalue_t p) { v.dsp.op[IDX].release=*p; }); \
    helper.addParam(static_cast<int>(FM4ParamId::k##PREFIX##FreqMode), \
       {.displayName="Frequency Mode", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f, \
        .isEnum=true, .valueFormatter=formatFreqMode}, \
       [](FM4SynthVoice& v,Patch::optional_paramvalue_t p){v.dsp.op[IDX].fixedMode=(*p>=0.5f);}); \
    helper.addParam(static_cast<int>(FM4ParamId::k##PREFIX##FixedHz), \
       {.displayName="Fixed Hz", .defaultValue=440.0f, .minValue=10.0f, .maxValue=8000.0f, \
        .editCurve=Param::Curve::kExponential, .valueFormatter=sdk::createFrequencyFormatter(8000.0f)}, \
       [](FM4SynthVoice& v,Patch::optional_paramvalue_t p){v.dsp.op[IDX].fixedHz=*p;});

void FM4SynthVoice::createMetadata(Patch& /*patch*/, Patch::Metadata& metadata) {
    using Param=Patch::Metadata::Param;
    using EditorElement=Patch::Metadata::EditorLayout::EditorElement;
    using EditorSection=Patch::Metadata::EditorLayout::EditorSection;
    constexpr int kCount = enumutil::EnumMax<FM4ParamId>::value + 1;
    metadata.instrumentTypeId=static_cast<int>(InstrumentTypeId::kFM4Synth);
    metadata.outputChannels=Patch::Metadata::OutputChannels::kMono;
    metadata.maxVoices=8;
    metadata.params.resize(kCount);
    paramHandlers.resize(kCount);
    sdk::ParamHelper<FM4SynthVoice> helper(metadata,paramHandlers);
    FM4_ADD_OP(Op1,0,1.0f,0.85f)
    FM4_ADD_OP(Op2,1,2.0f,0.55f)
    FM4_ADD_OP(Op3,2,3.0f,0.45f)
    FM4_ADD_OP(Op4,3,1.0f,0.45f)
    // Individual OP1-OP3 feedback; OP4 retains the original ID and handler.
#define FM4_ADD_FB(PREFIX, IDX) \
    helper.addParam(static_cast<int>(FM4ParamId::k##PREFIX##Feedback), \
       {.displayName="Feedback", .defaultValue=0.0f, .minValue=0.0f, .maxValue=4.0f, \
        .valueFormatter=[](const Patch&,Patch::paramvalue_t v){return fmt::format("{:.2f}",v);}}, \
       [](FM4SynthVoice& v,Patch::optional_paramvalue_t p){v.dsp.extraFeedback[IDX]=*p;});
    FM4_ADD_FB(Op1,0)
    FM4_ADD_FB(Op2,1)
    FM4_ADD_FB(Op3,2)
#undef FM4_ADD_FB

    helper.addParam(static_cast<int>(FM4ParamId::kAlgorithm),
       {.displayName="Algorithm", .defaultValue=0.0f, .minValue=0.0f, .maxValue=7.0f,
        .isEnum=true, .valueFormatter=formatAlgorithm},
       [](FM4SynthVoice& v, Patch::optional_paramvalue_t p) {
           v.dsp.algorithm=std::clamp<int>(static_cast<int>(std::lround(*p)),0,7);
       });
    helper.addParam(static_cast<int>(FM4ParamId::kModIndex),
       {.displayName="FM Index", .defaultValue=2.5f, .minValue=0.0f, .maxValue=12.0f,
        .valueFormatter=[](const Patch&,Patch::paramvalue_t v){return fmt::format("{:.2f}",v);}},
       [](FM4SynthVoice& v,Patch::optional_paramvalue_t p){v.dsp.modIndex=*p;});
    helper.addParam(static_cast<int>(FM4ParamId::kFeedback),
       {.displayName="OP4 Feedback", .defaultValue=0.0f, .minValue=0.0f, .maxValue=4.0f,
        .valueFormatter=[](const Patch&,Patch::paramvalue_t v){return fmt::format("{:.2f}",v);}},
       [](FM4SynthVoice& v,Patch::optional_paramvalue_t p){v.dsp.feedback=*p;});
    helper.addParam(static_cast<int>(FM4ParamId::kOutputGain),
       {.displayName="Output", .defaultValue=0.7f, .minValue=0.0f, .maxValue=1.5f,
        .valueFormatter=sdk::createPercentFormatter()},
       [](FM4SynthVoice& v,Patch::optional_paramvalue_t p){v.dsp.outputGain=*p;});
    helper.addParam(static_cast<int>(FM4ParamId::kOutputDrive),
       {.displayName="Saturation", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f,
        .valueFormatter=sdk::createPercentFormatter()},
       [](FM4SynthVoice& v,Patch::optional_paramvalue_t p){v.dsp.outputDrive=*p;});
    helper.addParam(static_cast<int>(FM4ParamId::kOutputFold),
       {.displayName="Wavefold", .defaultValue=0.0f, .minValue=0.0f, .maxValue=1.0f,
        .valueFormatter=sdk::createPercentFormatter()},
       [](FM4SynthVoice& v,Patch::optional_paramvalue_t p){v.dsp.outputFold=*p;});

    helper.addParam(static_cast<int>(FM4ParamId::kFoldDrive),
       {.displayName="Fold Input", .defaultValue=1.0f, .minValue=0.25f, .maxValue=4.0f,
        .valueFormatter=[](const Patch&,Patch::paramvalue_t v){return fmt::format("{:.2f} x",v);}},
       [](FM4SynthVoice& v,Patch::optional_paramvalue_t p){v.dsp.foldDrive=*p;});
    helper.addParam(static_cast<int>(FM4ParamId::kOutputMode),
       {.displayName="Output Mode", .defaultValue=0.0f, .minValue=0.0f, .maxValue=2.0f,
        .isEnum=true, .valueFormatter=formatOutputMode},
       [](FM4SynthVoice& v,Patch::optional_paramvalue_t p){v.dsp.outputMode=std::clamp<int>(static_cast<int>(std::lround(*p)),0,2);});
    helper.addParam(static_cast<int>(FM4ParamId::kStageCutoff),
       {.displayName="Brightness", .defaultValue=0.8f, .minValue=0.0f, .maxValue=1.0f,
        .valueFormatter=sdk::createPercentFormatter()},
       [](FM4SynthVoice& v,Patch::optional_paramvalue_t p){v.dsp.stageCutoff=*p;});
    helper.addParam(static_cast<int>(FM4ParamId::kStageEnvDepth),
       {.displayName="Env to Cutoff", .defaultValue=0.8f, .minValue=0.0f, .maxValue=1.0f,
        .valueFormatter=sdk::createPercentFormatter()},
       [](FM4SynthVoice& v,Patch::optional_paramvalue_t p){v.dsp.stageEnvDepth=*p;});
    helper.addParam(static_cast<int>(FM4ParamId::kLpgResponse),
       {.displayName="LPG Response", .defaultValue=0.45f, .minValue=0.0f, .maxValue=1.0f,
        .valueFormatter=sdk::createPercentFormatter()},
       [](FM4SynthVoice& v,Patch::optional_paramvalue_t p){v.dsp.lpgResponse=*p;});

    auto elem=[](FM4ParamId id) -> EditorElement {
        return {.type=EditorElement::Type::kParam,.id=static_cast<int>(id)};
    };
#define FM4_SECTION(OP,NAME,SHORT,FB) EditorSection{.name=NAME,.shortName=SHORT,.elements={ \
    elem(FM4ParamId::k##OP##Wave),elem(FM4ParamId::k##OP##Ratio), \
    elem(FM4ParamId::k##OP##FreqMode),elem(FM4ParamId::k##OP##FixedHz), \
    elem(FM4ParamId::k##OP##Fine),elem(FM4ParamId::k##OP##Level), \
    elem(FM4ParamId::k##OP##Attack),elem(FM4ParamId::k##OP##Decay), \
    elem(FM4ParamId::k##OP##Sustain),elem(FM4ParamId::k##OP##Release), \
    elem(FM4ParamId::FB)}}
    metadata.layoutFlat.sections={
      FM4_SECTION(Op1,"Oscillator","OP 1",kOp1Feedback),
      FM4_SECTION(Op2,"Oscillator","OP 2",kOp2Feedback),
      FM4_SECTION(Op3,"Oscillator","OP 3",kOp3Feedback),
      FM4_SECTION(Op4,"Oscillator","OP 4",kFeedback),
      EditorSection{.name="Modulation",.shortName="Mod",.elements={
        elem(FM4ParamId::kAlgorithm),elem(FM4ParamId::kModIndex)}},
      EditorSection{.name="Shaper",.shortName="Shp",.elements={
        elem(FM4ParamId::kOutputGain),elem(FM4ParamId::kOutputDrive),
        elem(FM4ParamId::kOutputFold),elem(FM4ParamId::kFoldDrive),
        elem(FM4ParamId::kOutputMode),elem(FM4ParamId::kStageCutoff),
        elem(FM4ParamId::kStageEnvDepth),elem(FM4ParamId::kLpgResponse)}}
    };
#undef FM4_SECTION
    metadata.layout23=metadata.layoutFlat;
    metadata.initDefaultMacros=[](auto& macros) {
        macros[0]=Patch::Metadata::Macro{.displayName="FM Intensity",
                    .modifiers={{static_cast<int>(FM4ParamId::kModIndex),0.65f}}, .bipolar=true};
        macros[1]=Patch::Metadata::Macro{.displayName="Feedback",
                    .modifiers={{static_cast<int>(FM4ParamId::kFeedback),0.7f}}, .bipolar=false};
        // M3-M6 intentionally empty; users can assign any FM4 parameter to any macro.
    };
}
#undef FM4_ADD_OP
} // anonymous namespace

std::unique_ptr<InstrumentType> createFM4Synth() {
    return std::make_unique<InstrumentType>(InstrumentType{
        .name="FM4", .createMetadata=&FM4SynthVoice::createMetadata,
        .createVoice=[](int) -> std::unique_ptr<InstrumentVoice> {
            return std::make_unique<FM4SynthVoice>();
        }});
}
} // namespace polysynth
#endif // FEATURE_SYNTH
