#include "../src/NoiseCompanion3320Filter.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <algorithm>
using companion::Filter3320Experimental;
static constexpr float SR=44100.f;
static constexpr float pi=3.14159265358979323846f;
float response(Filter3320Experimental::Mode m,float freq){
    Filter3320Experimental f;
    f.configure(1500.f,0.f,SR,0);
    double sum=0.;
    for(int i=0;i<44100;++i){
        float x=0.25f*std::sin(2*pi*freq*i/SR);
        float y=f.processPrepared(x,m);
        assert(std::isfinite(y));
        if(i>=22050)sum+=double(y)*y;
    }
    return std::sqrt(sum/(44100-22050));
}
int main(){
    float ll=response(Filter3320Experimental::Lowpass,150.f);
    float lh=response(Filter3320Experimental::Lowpass,9000.f);
    float hl=response(Filter3320Experimental::Highpass,150.f);
    float hh=response(Filter3320Experimental::Highpass,9000.f);
    assert(ll>lh*3.f);
    assert(hh>hl*2.f);
    // Resonance behavior: the experimental feedback path can sustain ringing at max.
    {
      Filter3320Experimental f;
      f.configure(900.f,1.f,SR,0);
      double resonantEnergy=0.;
      for(int i=0;i<2*44100;++i){
        float excitation=(i<100)? .05f*std::sin(i*.7f) : 0.f;
        float y=f.processPrepared(excitation,Filter3320Experimental::Lowpass);
        assert(std::isfinite(y));
        if(i>=44100) resonantEnergy+=double(y)*y;
      }
      assert(resonantEnergy/44100.0 > 0.000001);
    }
    // Each of the 3 modes, sweeping resonance and cutoff rapidly.
    double checksum=0.;
    for (int mode=0;mode<3;++mode){
      Filter3320Experimental f;
      for (int n=0;n<44100*6;++n){
        if(n%32==0) {
          float cutoff=20.f+18000.f*(.5f+.5f*std::sin(n*.033f));
          float res=.5f+.5f*std::sin(n*.091f);
          f.configure(cutoff,res,SR,32);
        }
        float y=f.processPrepared(.27f*std::sin(n*.62f),Filter3320Experimental::Mode(mode));
        assert(std::isfinite(y));assert(std::fabs(y)<=1.00001f); checksum+=std::abs(y);
      }
    }
    // 6 voice x 3 filters mixed concurrently at 44.1k for 1s.
    std::array<std::array<Filter3320Experimental,3>,6> voices{};
    auto start=std::chrono::steady_clock::now();
    for(int s=0;s<44100;++s){
      for(int v=0;v<6;++v){
        if(s%32==0)for(int k=0;k<3;++k)voices[v][k].configure(200.f+15000.f*float((s/32+k*19+v*13)%111)/110.f,.88f,SR,32);
        for(int k=0;k<3;++k){
          float y=voices[v][k].processPrepared(.2f*std::sin((s+k*91)*.031f),Filter3320Experimental::Mode(k));
          assert(std::isfinite(y));checksum+=y;
        }
      }
    }
    auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    std::printf("PASS filter experimental: LP low/high %.5f/%.5f; HP low/high %.5f/%.5f; checksum %.1f; host six-voice 1s %.2fms\n",ll,lh,hl,hh,checksum,elapsed);
}
