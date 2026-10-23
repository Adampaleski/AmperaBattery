// Host test: /workspace/tmp/zigvenv/bin/python -m ziglang c++ -std=c++17 -O1 -Iinclude scripts/test_lem_link.cpp -o /tmp/ll && /tmp/ll
#include "LemLink.h"
#include <cstdio>
#include <cmath>
static int pass=0,fail=0;static void ok(bool c,const char*m){c?pass++:fail++;std::printf("%s %s\n",c?"PASS":"FAIL",m);}
int main(){uint8_t b[8];float a=0;LemLink::Rx r;
  ok(!r.get(0,200,&a),"nothing received: no LEM B (fail closed)");
  LemLink::encode(123.4f,true,1,b);r.onFrame(b,1000);ok(r.get(1000,200,&a)&&std::fabs(a-123.4f)<0.06f,"123.4 A discharge round-trips");
  LemLink::encode(-87.25f,true,2,b);r.onFrame(b,1050);ok(r.get(1050,200,&a)&&std::fabs(a+87.3f)<0.06f,"-87.3 A regen round-trips (sign kept)");
  LemLink::encode(750.0f,true,3,b);r.onFrame(b,1100);ok(r.get(1100,200,&a)&&std::fabs(a-750.0f)<0.06f,"750 A (instant-trip level) fits");
  LemLink::encode(50.0f,false,4,b);r.onFrame(b,1150);ok(!r.get(1150,200,&a),"B says invalid (CURRENT_SENSE_ENABLE 0): no LEM B");
  LemLink::encode(5000.0f,true,5,b);r.onFrame(b,1200);ok(!r.get(1200,200,&a)&&b[2]==0,"5000 A out of range: sent invalid");
  LemLink::encode(NAN,true,6,b);r.onFrame(b,1250);ok(!r.get(1250,200,&a),"NaN: sent invalid");
  LemLink::encode(10.0f,true,7,b);r.onFrame(b,1300);ok(r.get(1500,200,&a),"fresh at exactly 200 ms");
  ok(!r.get(1501,200,&a),"stale after 200 ms: no LEM B");
  LemLink::encode(10.0f,true,7,b);r.onFrame(b,1600);ok(!r.get(1600,200,&a),"frozen heartbeat (same hb) does not refresh");
  LemLink::encode(10.0f,true,8,b);r.onFrame(b,1650);ok(r.get(1650,200,&a),"heartbeat advances: fresh again");
  uint8_t h[8]={0x00,0x80,1,0,0,0,0,9};r.onFrame(h,1700);ok(!r.get(1700,200,&a),"saturated raw -32768 rejected on A");
  std::printf("lem link: %d/%d passed\n",pass,pass+fail);return fail?1:0;}
