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
 const p=Object.assign({},ctx.D),I=(b,x)=>Object.assign({thr:0,brake:b,gear:'H',closed:true,allowPct:100,regenOk:true,packV:133},x||{});
 const r=b=>ctx.V(p,{mph:25},I(b),0.05).regenA;
 ok(r(10)===0&&r(30)>0&&r(50)>r(30)&&r(80)>r(50)&&Math.abs(r(100)-r(80))<1e-6,'regen proportional between dead zones');
 ok(ctx.V(p,{mph:25},I(50,{regenOk:false}),0.05).regenA===0,'BMS veto kills regen');
 ok(ctx.V(p,{mph:25},I(50,{regenAllowPct:50}),0.05).regenA<r(50)*0.6,'BMS taper scales regen');
 ok(ctx.V(p,{mph:10},I(5,{thr:80}),0.05).driveA===0,'brake switch overrides throttle');
 ok(ctx.V(p,{mph:0},I(0,{thr:80,parkBrake:true}),0.05).driveA===0,'park brake interlock: no drive');}
{const vm=require('vm');const ctx={};vm.createContext(ctx);vm.runInContext(require('fs').readFileSync(__dirname+'/../sim/vehicle.js','utf8')+';this.V=vehStep;this.D=VEH_DEF;this.R=vehRpm;',ctx);
 const p=Object.assign({},ctx.D);const drive=x=>{let st={mph:0},o;for(let i=0;i<1200;i++){o=ctx.V(p,st,Object.assign({thr:100,brake:0,gear:'H',closed:true,allowPct:100,regenOk:true,packV:140},x),0.05);st.mph=o.mph;}return o;};
 const dead=drive({teensyDead:true,kellyLowCap:true}),kid=drive({gear:'KID',kellyLowCap:true}),free=drive({});
 ok(dead.rpm<=p.lowCapRpm+1&&free.rpm>p.lowCapRpm*2,'both Teensys dead: Kelly Low cap holds ('+dead.rpm.toFixed(0)+' RPM ≈ '+dead.mph.toFixed(1)+' mph)');
 ok(kid.rpm<=p.lowCapRpm+1,'Kid mode engages the Kelly Low cap');}
console.log(`controls: ${n-f}/${n} passed`);process.exit(f?1:0);

