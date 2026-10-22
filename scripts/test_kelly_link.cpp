// Host test: /workspace/tmp/zigvenv/bin/python -m ziglang c++ -std=c++17 -O2 -Iinclude scripts/test_kelly_link.cpp -o /tmp/kl && /tmp/kl
#include "KellyLink.h"
#include <cstdio>
#include <string>
static int pass=0,fail=0;static void ok(bool c,const char*m){c?pass++:fail++;std::printf("%s %s\n",c?"PASS":"FAIL",m);}
using namespace KellyLink;
// run for ms in 10 ms steps; frames every 50 ms when rpm/thr true
static uint8_t run(Watchdog &w,uint32_t &t,uint32_t ms,bool rpm,bool thr){uint8_t r=0;for(uint32_t e=t+ms;t<e;t+=10){if(t%50==0){if(rpm)w.rpmFrame(t);if(thr)w.thrFrame(t);}r=w.step(t);}return r;}
int main(){uint32_t t=0;
  {Watchdog w;ok(w.step(0)==OK&&!w.capActive,"key off: no fault, no cap");}
  {Watchdog w;t=0;w.keyOn(t);ok(run(w,t,1500,false,false)==OK,"key on, Kelly booting 1.5 s, no frames: inside grace, ok");
   ok(run(w,t,600,false,false)==NEVER_SEEN&&w.capActive,"no frames after 2 s grace: NEVER-SEEN + Low cap");
   ok(run(w,t,500,true,true)==OK&&w.capActive,"frames arrive: live ok but cap held (1 s recover)");
   ok(run(w,t,600,true,true)==OK&&!w.capActive,"fresh 1.1 s: cap lifts");
   ok(w.latched==NEVER_SEEN,"latched NEVER-SEEN kept for the log");}
  {Watchdog w;t=0;w.keyOn(t);run(w,t,3000,true,true);ok(w.code==OK&&!w.capActive,"healthy 3 s: ok");
   ok(run(w,t,150,false,false)==OK,"frames stop 150 ms: not yet (200 ms stale)");
   ok(run(w,t,100,false,false)==LOST&&w.capActive,"frames stop 250 ms: LOST + Low cap");
   ok(!w.parkTrusted(t),"LOST: Park not trusted");
   ok(run(w,t,1500,true,true)==OK&&!w.capActive&&w.parkTrusted(t),"frames back 1.5 s: recovered");
   ok(w.lossCount==1&&w.latched==LOST,"one loss counted, latched LOST");}
  {Watchdog w;t=0;w.keyOn(t);run(w,t,3000,true,true);ok(run(w,t,300,true,false)==LOST,"only 0x0CF11F05 (temps/dir) missing: LOST");}
  {Watchdog w;t=0;w.keyOn(t);run(w,t,3000,true,true);ok(run(w,t,300,false,true)==LOST,"only 0x0CF11E05 (RPM) missing: LOST");}
  {Watchdog w;t=0;w.keyOn(t);run(w,t,500,true,true);ok(run(w,t,300,false,false)==LOST,"seen then gone inside boot grace: LOST (not hidden by grace)");}
  {Watchdog w;t=0;w.keyOn(t);run(w,t,3000,true,true);
   for(int i=0;i<3;i++){run(w,t,300,false,false);run(w,t,400,true,true);}
   ok(w.capActive&&w.lossCount==3,"flapping link (300 ms out / 400 ms in): cap never lifts, 3 losses");}
  {Watchdog w;t=0;w.keyOn(t);run(w,t,2500,false,false);w.keyOff();ok(w.step(t)==OK&&w.latched==OK&&!w.capActive,"key off clears everything");}
  ok(std::string(name(LOST))=="KELLY-CAN-LOST","name KELLY-CAN-LOST");
  std::printf("kelly link: %d/%d passed\n",pass,pass+fail);return fail?1:0;}
