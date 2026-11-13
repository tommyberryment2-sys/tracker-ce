#include "../src/NoiseCompanionProgramCatalog.h"
#include <cassert>
#include <set>
#include <string>
#include <cstdio>
int main(){
 std::set<std::string> names;
 for(int n=0;n<30;++n){assert(!companion::programName(n).empty()); names.emplace(companion::programName(n));}
 assert(names.size()==30);
 companion::ProgramSelection p;
 p.choose(0,0);p.choose(1,29);
 assert(p.get(0)==0 && p.get(1)==29);
 p.choose(0,999);
 assert(p.get(0)==29 && p.get(1)==29);
 assert(companion::bankOf(11)==1 && companion::slotOf(11)==1);
 std::puts("PASS 30-name catalog and independent A/B selectors (DSP not included)");
}
