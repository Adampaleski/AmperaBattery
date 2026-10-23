// node scripts/test_controls.js — interlock + actuator checks for sim/controls.js
require('../sim/controls.js');const T=globalThis.TeryxControls;let n=0,f=0;
const ok=(c,m)=>{n++;if(!c){f++;console.log('FAIL',m);}};
let c=T.create(),t=1000;const run=(d,ms,step=100)=>{let r;for(let i=0;i<ms;i+=step){t+=step;r=c.tick(d,t);}return r;};
run({mph:0},100);
ok(!c.toggle4wd({mph:3,thr:0,now:t}).ok&&/Stop \(≤ 1 mph\)/.test(c.S.msg),'4WD refused at 3 mph: '+c.S.msg);
ok(!c.toggle4wd({mph:0,thr:30,now:t}).ok,'4WD refused with throttle');
ok(!c.toggle4wd({mph:null,thr:0,now:t}).ok,'4WD refused without speed');
ok(!c.setDiff({mph:0,thr:0,now:t},'full').ok&&c.S.msg==='Diff lock only in 4WD','diff refused in 2WD');
ok(c.toggle4wd({mph:0.5,thr:0,now:t}).ok,'4WD ok at 0.5 mph');run({mph:0},1000);ok(c.S.wd==='2WD','not yet engaged mid-travel');
run({mph:0},2000);ok(c.S.wd==='4WD'&&Math.abs(c.S.a4.pos-95)<=3,'4WD engaged at endpoint');
ok(!c.setDiff({mph:2,thr:0,now:t},'full').ok,'diff refused at 2 mph');
ok(c.setDiff({mph:0,thr:0,now:t},'partial').ok&&/steering angle not sensed/.test(c.S.msg),'partial ok with steering warning');
run({mph:0},2000);ok(c.S.diff==='partial'&&Math.abs(c.S.ad.pos-50)<=3,'diff partial preset');
ok(!c.toggle4wd({mph:0,thr:0,now:t}).ok,'2WD refused while diff locked');
ok(c.setDiff({mph:0,thr:0,now:t},'full').ok,'full ok');run({mph:0},2000);ok(c.S.diff==='full','diff full');
run({mph:16},200);ok(/auto-unlocked/.test(c.S.msg),'auto-unlock >15 mph');run({mph:16},3000);ok(c.S.diff==='off','diff off');
c.simStall('4wd',true);ok(c.toggle4wd({mph:0,thr:0,now:t}).ok,'2WD request');run({mph:0},1000);
ok(/stalled/.test(c.S.a4.fault)&&c.S.faults.length===1&&!c.S.a4.moving,'stall fault logged + H-bridge off: '+c.S.a4.fault);
ok(/actuator fault/.test(c.wdBlock({mph:0,thr:0})),'4WD blocked while faulted');
c.simStall('4wd',false);c.clearFault();c.S.set.actRatePct=1;c.S.set.stallMinPct=0.05;ok(c.toggle4wd({mph:0,thr:0,now:t}).ok,'retry');run({mph:0},4500);ok(/timeout/.test(c.S.a4.fault),'timeout fault: '+c.S.a4.fault);
let r=run({mph:0,brake:50,gear:'R'},100);ok(r.out.brake&&r.out.reverse,'auto brake + reverse');
c.setHazard(true);let on=0;for(let i=0;i<20;i++)on+=run({mph:0},100).out.turnL?1:0;ok(on>5&&on<15,'hazard blinks '+on);
c.setLight('high',true);r=run({mph:0},100);ok(r.out.headLow&&r.out.headHigh&&r.out.tail,'high beam implies low + tail');
// ---- park detection (mirror of include/ParkState.h + scripts/test_park.cpp)
{const P=globalThis.TeryxPark.create();let t=0;const i={contactA:false,contactB:false,rpmValid:true,rpm:0,neutral:true};
 const pr=ms=>{for(let k=0;k<ms;k++)P.step(++t,i);};
 pr(10);ok(!P.S.parked&&!P.prechargeAllowed(),'park off: not parked, precharge blocked');
 i.contactA=i.contactB=true;pr(10);ok(P.S.parked&&P.prechargeAllowed()&&P.kidChangeAllowed(),'park on');
 i.neutral=false;pr(5);ok(!P.S.parked&&P.prechargeAllowed(),'not neutral -> not parked');i.neutral=true;
 i.contactB=false;pr(50);ok(!P.S.fault,'disagree < 100 ms ok');pr(60);ok((P.S.fault&1)&&!P.prechargeAllowed(),'contact disagreement fault');
 i.contactB=true;pr(5);ok(!P.S.fault&&(P.S.latched&1),'disagree latched');P.clear();
 i.rpm=600;pr(400);ok((P.S.fault&2)&&!P.kidChangeAllowed(),'brake on while moving fault');
 i.contactA=i.contactB=false;pr(5);ok(!P.kidChangeAllowed(),'Kid change blocked while moving');
 i.rpm=0;i.rpmValid=false;i.contactA=i.contactB=true;pr(5);ok(!P.S.parked,'stale Kelly CAN -> not parked');
 const src=require('fs').readFileSync(__dirname+'/../include/ParkState.h','utf8');
 ok(/kDisagreeMs\s*=\s*100\b/.test(src)&&/kMovingMs\s*=\s*300\b/.test(src)&&/kMoveRpm\s*=\s*50\b/.test(src),'JS park timings match ParkState.h');}
// ---- analog brake regen (vehicle.js)
{const vm=require('vm');const ctx={};vm.createContext(ctx);vm.runInContext(require('fs').readFileSync(__dirname+'/../sim/vehicle.js','utf8')+';this.V=vehStep;this.D=VEH_DEF;',ctx);
 const p=Object.assign({},ctx.D,{analogBrake:true}),I=(b,x)=>Object.assign({thr:0,brake:b,gear:'H',closed:true,allowPct:100,regenOk:true,packV:133},x||{});
 const r=b=>ctx.V(p,{mph:25},I(b),0.05).regenA;
 ok(r(10)===0&&r(30)>0&&r(50)>r(30)&&r(80)>r(50)&&Math.abs(r(100)-r(80))<1e-6,'regen proportional between dead zones');
 ok(ctx.V(p,{mph:25},I(50,{regenOk:false}),0.05).regenA===0,'BMS veto kills regen');
 ok(ctx.V(p,{mph:25},I(50,{regenAllowPct:50}),0.05).regenA<r(50)*0.6,'BMS taper scales regen');
 ok(ctx.V(p,{mph:10},I(5,{thr:80}),0.05).driveA===0,'brake switch overrides throttle');
 {const q=Object.assign({},ctx.D);ok(q.analogBrake===false,'Brake_AN DEFERRED: default off');const a=ctx.V(q,{mph:25},I(10),0.05).regenReqA,b=ctx.V(q,{mph:25},I(90),0.05).regenReqA;
  ok(a>0&&Math.abs(a-b)<1e-9&&Math.abs(a-q.maxRegenA*q.brkSwRegenPct/50)<1e-9,'default: brake-switch regen at a fixed level ('+a+' A asked)');}
 ok(ctx.V(p,{mph:0},I(0,{thr:80,parkBrake:true}),0.05).driveA===0,'park brake interlock: no drive');}
{const vm=require('vm');const ctx={};vm.createContext(ctx);vm.runInContext(require('fs').readFileSync(__dirname+'/../sim/vehicle.js','utf8')+';this.V=vehStep;this.D=VEH_DEF;this.R=vehRpm;',ctx);
 const p=Object.assign({},ctx.D);const drive=x=>{let st={mph:0},o;for(let i=0;i<1200;i++){o=ctx.V(p,st,Object.assign({thr:100,brake:0,gear:'H',closed:true,allowPct:100,regenOk:true,packV:140},x),0.05);st.mph=o.mph;}return o;};
 const dead=drive({teensyDead:true,kellyLowCap:true}),kid=drive({gear:'KID',kellyLowCap:true}),free=drive({});
 ok(dead.rpm<=p.lowCapRpm+1&&free.rpm>p.lowCapRpm*2,'both Teensys dead: Kelly Low cap holds ('+dead.rpm.toFixed(0)+' RPM ≈ '+dead.mph.toFixed(1)+' mph)');
 ok(kid.rpm<=p.lowCapRpm+1,'Kid mode engages the Kelly Low cap');}
// ---- grade physics sign + no runaway (Build 1 bug: 0 throttle on 15 % reached 80 mph rolling back)
{const vm=require('vm');const ctx={};vm.createContext(ctx);vm.runInContext(require('fs').readFileSync(__dirname+'/../sim/vehicle.js','utf8')+';this.V=vehStep;this.D=VEH_DEF;this.M=vehMphAtRpm;this.R=vehRpm;this.G=VEH_GEARS;',ctx);
 const run=(g,gear,m0,x,secs)=>{const p=Object.assign({},ctx.D,{gradePct:g});let st={mph:m0},o,mx=0,first=null;for(let i=0;i<(secs||60)*20;i++){o=ctx.V(p,st,Object.assign({thr:0,brake:0,gear,closed:true,allowPct:100,regenOk:true,packV:140},x||{}),0.05);if(i===0)first=o.mph;st.mph=o.mph;mx=Math.max(mx,Math.abs(o.mph));}return {end:o.mph,mx,first,o};};
 const cap=ctx.M(ctx.D,6000);
 {const r=run(15,'H',20);ok(r.first<20,'uphill 15 %, 0 throttle: decelerates ('+r.first.toFixed(2)+' mph after 50 ms)');ok(r.end<0,'uphill: then rolls back ('+r.end.toFixed(1)+' mph)');ok(r.mx<=20+1e-6,'uphill: never faster than entry speed');}
 {const r=run(-15,'H',0,null,5);ok(r.end>0,'downhill 15 %, 0 throttle: accelerates forward ('+r.end.toFixed(1)+' mph at 5 s)');}
 for(const [g,gear,x,lbl] of [[15,'H',null,'rollback live'],[-15,'H',null,'downhill live'],[-30,'H',null,'30 % downhill live'],[15,'H',{closed:false},'rollback contactors open'],[-30,'H',{regenOk:false},'downhill regen vetoed'],[-15,'R',null,'reverse gear downhill']]){
  const r=run(g,gear,0,x,120);ok(r.mx<=cap+1e-6,'0 throttle never above the 6000 RPM cap: '+lbl+' (max '+r.mx.toFixed(1)+' ≤ '+cap.toFixed(1)+' mph)');}
 {const r=run(-15,'H',0,null,120);ok(r.mx<=cap&&r.o.regenA>0,'downhill: Kelly overspeed regen holds near the cap (regen '+r.o.regenA.toFixed(0)+' A)');}
 {const r=run(15,'H',0,null,120);ok(Math.abs(r.end)<=ctx.M(ctx.D,ctx.G.R.rpm)+1e-6,'rollback held at the reverse cap ('+r.end.toFixed(1)+' mph)');}
 {const r=run(-15,'H',0,{thr:100},120);ok(r.mx<=cap+1e-6,'full throttle downhill: never above the cap ('+r.mx.toFixed(1)+')');}
 {const r=run(15,'H',0,{brake:100},20);ok(Math.abs(r.end)<1e-9,'uphill + full brake: holds at 0');}
 {const r=run(15,'H',0,{parkBrake:true},20);ok(Math.abs(r.end)<1e-9,'uphill 15 % + park brake: holds at 0');}}
// ---- reverse cap per mode (PROPOSED, Adam 2026-10-22): 15 mph High/Medium/Snow/Mud/Sand (never above the mode's forward top), 10 mph Low/Kid
{const vm=require('vm');const ctx={};vm.createContext(ctx);vm.runInContext(require('fs').readFileSync(__dirname+'/../sim/vehicle.js','utf8')+';this.V=vehStep;this.D=VEH_DEF;this.G=VEH_GEARS;this.T=vehRevTop;this.F=VEH_MODE_DEF;this.M=vehMphAtRpm;',ctx);
 const want={H:15,M:15,L:10,SNOW:15,MUD:15,SAND:15,KID:10};
 ok(Object.entries(want).every(([m,v])=>ctx.T(m)===v),'reverse cap table: '+Object.keys(want).map(m=>m+' '+ctx.T(m)).join(', '));
 ok(ctx.F.R.cur===25&&ctx.F.R.expo===0.3&&ctx.F.R.ramp===500,'reverse keeps the gentle feel (25 % pedal cap, expo 0.3, 500 A/s)');
 ok(Math.abs(ctx.M(ctx.D,ctx.F.R.rpm)-15)<0.5&&Math.abs(ctx.M(ctx.D,ctx.D.lowRevRpm)-10)<0.5,'Kelly reverse caps: '+ctx.F.R.rpm+' RPM ≈ '+ctx.M(ctx.D,ctx.F.R.rpm).toFixed(1)+' mph, low '+ctx.D.lowRevRpm+' RPM ≈ '+ctx.M(ctx.D,ctx.D.lowRevRpm).toFixed(1)+' mph');
 const rev=(m,x)=>{const p=Object.assign({},ctx.D);const st={mph:0,trim:{filt:0,out:0}};let o,mx=0;for(let i=0;i<120*20;i++){o=ctx.V(p,st,Object.assign({thr:100,brake:0,gear:'R',revMode:m,closed:true,allowPct:100,regenOk:true,packV:140,kellyLowCap:m==='KID'},x||{}),0.05);st.mph=o.mph;mx=Math.max(mx,Math.abs(o.mph));}return {end:o.mph,mx,o};};
 for(const m of Object.keys(want)){const r=rev(m);ok(r.end<0&&r.mx<=want[m]+0.05&&r.mx>=want[m]-2.5,'full pedal in reverse from '+m+': backs up and holds '+(-r.end).toFixed(1)+' mph (cap '+want[m]+', max '+r.mx.toFixed(1)+')');}
 {const r=rev('KID',{teensyDead:true});ok(r.mx<=ctx.M(ctx.D,ctx.D.lowRevRpm)+0.05,'Kid reverse, both Teensys dead: Kelly LowSpeed Rev holds ('+r.mx.toFixed(1)+' mph)');}
 {const r=rev('H',{teensyDead:true,kellyLowCap:false});ok(r.mx<=ctx.M(ctx.D,ctx.F.R.rpm)+0.05,'High reverse, both Teensys dead (raw pedal): Kelly Rev Speed 31 % still holds ('+r.mx.toFixed(1)+' mph)');}
 {const h0=ctx.G.H.top;ctx.G.H.top=12;const t=ctx.T('H');ctx.G.H.top=h0;ok(t===12,'reverse cap never above the mode forward top (High edited to 12 -> reverse 12)');}
 {const r0=ctx.G.R.top;ctx.G.R.top=8;const t=[ctx.T('H'),ctx.T('KID')];ctx.G.R.top=r0;ok(t[0]===8&&t[1]===8,'lowering the Reverse top in Settings lowers every mode');}
 {const one=(gear,m)=>{const st={mph:0,trim:{filt:0,out:0}};let o;for(let i=0;i<20;i++){o=ctx.V(Object.assign({},ctx.D),st,{thr:100,brake:0,gear,revMode:m,closed:true,allowPct:100,regenOk:true,packV:140},0.05);st.mph=o.mph;}return Math.abs(o.mph);};
  const rv=one('R','H'),fw=one('H');ok(rv>0&&rv<fw*0.5,'reverse stays gentle: '+rv.toFixed(1)+' mph after 1 s full pedal vs High forward '+fw.toFixed(1)+' mph');}}
console.log(`controls: ${n-f}/${n} passed`);process.exit(f?1:0);

