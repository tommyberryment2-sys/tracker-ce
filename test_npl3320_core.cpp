#include "../src/NoiseCompanionCore3320Experiment.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
  companion::Engine e;
  e.clock.sampleRate=44100.f;e.clock.setHostBpm(120.f);
  e.base.level[0]=.5f;e.base.level[1]=.5f;e.base.level[2]=.5f;
  for(int i=0;i<44100*3;++i){
    float a=.20f*std::sin(i*.08f),b=.2f*std::cos(i*.04f);
    if(i%32==0){
      for(int k=0;k<3;++k){
        e.base.cutoff[k]=40+18000*(.5+.5*std::sin(i*.02+k));
        e.base.resonance[k]=.5+.5*std::sin(i*.075+k);
        e.base.mode[k]=(i/256+k)%3;
      }
    }
    auto f=e.process(a,b);
    assert(std::isfinite(f.mix) && std::abs(f.mix)<=1.001f);
  }
  std::puts("PASS combined 3-channel prototype: stable rapid cutoff/resonance/modulation sweeps");
}
