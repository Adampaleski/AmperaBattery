// Host test: /workspace/tmp/zigvenv/bin/python -m ziglang c++ -std=c++17 -O2 -Iinclude scripts/test_lem_health.cpp -o /tmp/lh && /tmp/lh
#include "LemHealth.h"
#include <cstdio>
#include <string>
static int pass=0,fail=0;static void ok(bool c,const char*m){c?pass++:fail++;std::printf("%s %s\n",c?"PASS":"FAIL",m);}
using namespace LemHealth;
static BmsCore::CurrentCal cal;
static void pins(float A,float &a1,float &a2,float &vc){BmsCore::dhabPinVolts(cal,A,5.0f,a1,a2,vc);}
static uint8_t run(Monitor &m,uint32_t &t,uint32_t ms,float a1,float a2,float vc){uint8_t r=0;for(uint32_t e=t+ms;t<e;t+=10)r=m.update(t,a1,a2,vc);return r;}
int main(){float a1,a2,vc;uint32_t t=0;
  {Monitor m;pins(0,a1,a2,vc);ok(run(m,t,1000,a1,a2,vc)==OK,"healthy at 0 A: ok");
   pins(-300,a1,a2,vc);ok(run(m,t,1000,a1,a2,vc)==OK,"healthy at 300 A discharge side: ok");
   pins(200,a1,a2,vc);ok(run(m,t,1000,a1,a2,vc)==OK,"healthy at 200 A regen side: ok");
   pins(8,a1,a2,vc);ok(run(m,t,1000,a1,a2,vc)==OK&&m.zeroSelfTest(a1,a2,vc)==false&&m.latched==ZERO,"8 A at key-on self-test: ZERO latched");}
  {Monitor m;t=0;pins(0,a1,a2,vc);ok(m.zeroSelfTest(a1,a2,vc)&&!m.hardFault(),"0 A at key-on self-test: pass");
   pins(3,a1,a2,vc);ok(m.zeroSelfTest(a1,a2,vc),"3 A at self-test (inside 5 A): pass");}
  {Monitor m;t=0;pins(0,a1,a2,vc);ok(run(m,t,50,0.0f,a2,vc)==OK,"ch1 at 0 V for 50 ms: not yet (debounce)");
   ok(run(m,t,100,0.0f,a2,vc)==RAIL&&m.hardFault(),"ch1 at 0 V (unplugged) for 150 ms: RAIL hard fault");
   ok(!m.reset(),"reset refused while still unplugged");
   ok(run(m,t,100,a1,a2,vc)==RAIL,"plugged back in: stays latched until key cycle");
   ok(m.reset()&&!m.hardFault(),"key cycle with sensor healthy: cleared");}
  {Monitor m;t=0;pins(0,a1,a2,vc);ok(run(m,t,200,a1,a2,0.0f)==RAIL,"5 V supply lost: RAIL");}
  {Monitor m;t=0;pins(0,a1,a2,vc);ok(run(m,t,200,a1,(5.0f-0.1f)*cal.divider,vc)==RAIL,"ch2 stuck at 5 V rail: RAIL");}
  {Monitor m;t=0;// ch2 forced to read +30 A while ch1 says 0 A (ch2 < 15 A rule not met -> no AGREE) then ch2 +10 A
   pins(0,a1,a2,vc);float x1,x2,xv;pins(10,x1,x2,xv);
   ok(run(m,t,1000,a1,x2,vc)==AGREE,"ch1 0 A vs ch2 10 A for 1 s: AGREE");
   Monitor m2;uint32_t t2=0;ok(run(m2,t2,400,a1,x2,vc)==OK,"same disagreement for 400 ms: not yet (500 ms hold)");}
  {Monitor m;t=0;// ch2 clamps near -460 A (0.25 V), so test RANGE with a tighter limit
   pins(-440,a1,a2,vc);ok(run(m,t,200,a1,a2,vc)==OK,"440 A: inside 450 A range, ok");
   m.lim.rangeA=400;ok(run(m,t,200,a1,a2,vc)==RANGE,"440 A with range limit 400: RANGE");}
  {Monitor m;t=0;ok(std::string(name(RAIL)).size()>0,"names present");}
  std::printf("lem health: %d/%d passed\n",pass,pass+fail);return fail?1:0;}
