// Host test: /workspace/tmp/zigvenv/bin/python -m ziglang c++ -std=c++17 -O2 -Iinclude scripts/test_estop_latch.cpp -o /tmp/el && /tmp/el
#include "EstopLatch.h"
#include <cstdio>
static int pass=0,fail=0;static void ok(bool c,const char*m){c?pass++:fail++;std::printf("%s %s\n",c?"PASS":"FAIL",m);}
int main(){Estop::Latch L;
  L.update(true);ok(L.coilsAllowed(),"input closed at boot: coils allowed");
  L.update(false);ok(!L.coilsAllowed()&&L.trips==1,"NC input open (pressed): coil drivers dropped, latched");
  L.update(true);ok(!L.coilsAllowed(),"button released: stays latched, nothing closes");
  for(int i=0;i<1000;i++)L.update(true);ok(!L.coilsAllowed(),"released for 1000 loops: still latched");
  L.update(false);ok(!L.reset(false)&&!L.coilsAllowed(),"reset refused while still pressed / wire broken");
  L.update(true);ok(L.reset(true)&&L.coilsAllowed(),"explicit reset with input closed: cleared");
  L.update(false);L.update(false);ok(L.trips==2,"held open counts one trip");
  std::printf("estop latch: %d/%d passed\n",pass,pass+fail);return fail?1:0;}
