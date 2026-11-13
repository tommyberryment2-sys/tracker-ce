#pragma once
// Program NAMES and normalized gain data are factual metadata transcribed
// from Befaco Noise Plethora's public Banks_Def.hpp, not DSP implementations.
// This catalogue does NOT mean 30 algorithms have been ported.
#include <array>
#include <algorithm>
#include <cstddef>
#include <string_view>
namespace companion {
struct ProgramInfo {std::string_view name; float gain;};
inline constexpr std::array<ProgramInfo,30> kProgramCatalog = {{
  {"radioOhNo",1.f}, {"Rwalk_SineFMFlange",1.f}, {"xModRingSqr",1.f},
  {"XModRingSine",1.f}, {"CrossModRing",1.f}, {"resonoise",1.f},
  {"grainGlitch",1.f}, {"grainGlitchII",1.f}, {"grainGlitchIII",1.f}, {"basurilla",1.f},
  {"clusterSaw",1.f}, {"pwCluster",1.f}, {"crCluster2",1.f},
  {"sineFMcluster",1.f}, {"TriFMcluster",1.f}, {"PrimeCluster",.8f},
  {"PrimeCnoise",.8f}, {"FibonacciCluster",1.f}, {"partialCluster",1.f}, {"phasingCluster",1.f},
  {"BasuraTotal",1.f}, {"Atari",1.f}, {"WalkingFilomena",1.f},
  {"S_H",1.f}, {"arrayOnTheRocks",1.f}, {"existencelsPain",1.f},
  {"whoKnows",1.f}, {"satanWorkout",1.f}, {"Rwalk_BitCrushPW",1.f}, {"Rwalk_LFree",1.f}
}};
static_assert(kProgramCatalog.size()==30);
inline constexpr int kBankCount=3, kProgramsPerBank=10;
inline int normalizeProgram(int program){return std::clamp(program,0,29);}
inline int bankOf(int program){return normalizeProgram(program)/10;}
inline int slotOf(int program){return normalizeProgram(program)%10;}
inline std::string_view programName(int id){return kProgramCatalog[normalizeProgram(id)].name;}
// Separate selectors for the two digital channels, no global active-algo state.
struct ProgramSelection {
    std::array<int,2> selected{0,0};
    void choose(int channel,int program){if(channel>=0 && channel<2)selected[channel]=normalizeProgram(program);}
    int get(int channel)const{return selected[std::clamp(channel,0,1)];}
};
}
