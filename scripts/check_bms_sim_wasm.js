// node scripts/check_bms_sim_wasm.js /tmp/bms_sim.wasm  -> same scenario table as scripts/sim_bms.cpp (parity) + 8 socpage checks (BmsCore::Soc exports) + 5 drivecycle checks (sim/drive_cycles.js) + 6 faultlog-page checks; exit 1 on any FAIL
// + charge-mode and vehicle-model/regen checks (sim/vehicle.js)
const fs=require("fs");
const inst=new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(process.argv[2])),{});
const X=inst.exports; X._initialize();
const cs=p=>{const m=new Uint8Array(X.memory.buffer);let e=p;while(m[e])e++;return Buffer.from(m.subarray(p,e)).toString();};
const t0=Date.now();const r=JSON.parse(cs(X.sim_run_scenarios()));
r.forEach(x=>console.log((x.pass?"PASS":"FAIL"),String(x.gotMs).padStart(6),x.name));
console.log(r.filter(x=>x.pass).length+"/"+r.length,"in",Date.now()-t0,"ms");
X.sim_reset();X.sim_set_key(1);X.sim_set_charge(1);X.sim_step(4000);
const s=JSON.parse(cs(X.sim_state_json()));console.log(s.state,s.chargeEnable,s.chargeA,s.ceilingV,s.busV,s.kellyPct);
X.sim_set_cell(0,6,4.0);X.sim_step(400);console.log(cs(X.sim_plan_text()));
for(let i=0;i<X.sim_log_count();i++)console.log(X.sim_log_ms(i),cs(X.sim_log_text(i)));
// ---- SoC readout (BmsCore::Soc on LEM A + B, sim only; 180 Ah SIM-ASSUMED capacity)
let sf=0,sn=0;const soc=()=>JSON.parse(cs(X.sim_state_json())).soc;
const sc=(ok,name)=>{sn++;if(!ok)sf++;const o=soc();console.log((ok?"PASS":"FAIL")+" socpage "+name.padEnd(40),`state=${o.state} pct=${o.pct.toFixed(2)} used=${o.usedAh.toFixed(3)} Ah drift=${o.drift} truth=${o.truthPct.toFixed(1)} (${o.why})`);};
const fill=pct=>{X.sim_set_charge_pct(pct);const v=X.sim_ceiling_for_pct(pct)+0.001;for(let b=0;b<8;b++)for(let c=0;c<X.sim_board_cells(b);c++)X.sim_set_cell(b,c,v);};
X.sim_reset();X.sim_set_key(1);X.sim_step(4000);
{const o=soc();sc(o.state==="unknown"&&o.pct<0&&!o.known&&X.sim_soc_pct()<0&&X.sim_soc_cap_ah()===180,"boot: unknown until full-charge anchor");}
X.sim_set_charge(1);fill(80);X.sim_step(200);
{const o=soc();sc(o.state==="anchored"&&Math.abs(o.pct-80)<0.01&&o.usedAh===0&&/not true 100/.test(o.why),"P80 'at charge limit' -> anchored 80 %");}
X.sim_set_charge(0);X.sim_set_pack_amps(180);X.sim_step(36000);X.sim_set_pack_amps(0);X.sim_step(100);
{const o=soc();sc(o.state==="counting"&&Math.abs(o.usedAh-1.8)<0.02&&Math.abs(o.pct-79)<0.02&&Math.abs(X.sim_soc_used_ah()-o.usedAh)<1e-3,"180 A x 36 s = 1.8 Ah -> 79.0 %");}
{const o=soc();const t0=X.sim_ceiling_for_pct(80)+0.001;sc(Math.abs(o.truthPct-((t0-0.008-3.30)/0.80*100))<0.3,"sim truth drains with current (1 % of 0.8 V)");}
X.sim_set_pack_amps(50);X.sim_lem_dropout(1500);X.sim_step(1600);
{const o=soc();sc(o.state==="counting"&&Math.abs(o.usedAh-1.8)<0.02,"LEM dropout 1.5 s: count holds, no Ah added");}
X.sim_lem_dropout(3000);X.sim_step(2600);
{const o=soc();sc(o.state==="unknown"&&o.pct<0&&/current lost/.test(cs(X.sim_soc_why())),"LEM dropout > 2 s -> unknown");}
X.sim_set_pack_amps(0);X.sim_set_charge(1);fill(100);X.sim_step(200);X.sim_set_charge(0);X.sim_step(100);
{const o=soc();sc(o.known&&o.pct>99.9&&o.why==="full anchor","P100 re-anchor -> 100 %");}
X.sim_set_cell(0,6,3.31);X.sim_step(100);
{const o=soc();sc(o.drift===1&&X.sim_soc_drift()===1&&/drift/.test(o.why),"min cell at low cut, SoC 100 % -> drift flag");}
console.log(`socpage: ${sn-sf}/${sn} checks passed`);
// ---- Drive cycles (sim/drive_cycles.js profiles through the page's driver-input model, 100 ms steps)
const path=require("path");
const DC=new Function(fs.readFileSync(path.join(__dirname,"../sim/drive_cycles.js"),"utf8")+";return {DRIVE_CYCLES,cycleLen,cycleAt};")();
const VM=new Function(fs.readFileSync(path.join(__dirname,"../sim/vehicle.js"),"utf8")+";return {VEH_DEF,VEH_GEARS,vehStep,vehRpm,vehRatio};")();  // same vehicle model as the page
let df=0,dn=0;
const dck=(ok,name,d)=>{dn++;if(!ok)df++;console.log((ok?"PASS":"FAIL")+" drivecycle "+name.padEnd(44),`end=${d.end} t=${d.t.toFixed(0)}s peak=${d.peakA.toFixed(0)}A regen=${(-d.minA).toFixed(0)}A dist=${d.dist.toFixed(2)}mi Ah=${d.ah.toFixed(2)} soc=${d.soc0.toFixed(1)}->${d.soc1.toFixed(1)} lowCell=${d.lo.toFixed(3)}`);};
const lowCell=()=>{let lo=9;for(let b=0;b<8;b++)for(let c=0;c<X.sim_board_cells(b);c++)lo=Math.min(lo,X.sim_get_cell(b,c));return lo;};
function drive(k,prep){
  X.sim_reset();X.sim_set_soc_drain(1);X.sim_set_key(1);X.sim_step(4000);X.sim_set_charge(1);fill(90);X.sim_step(200);X.sim_set_charge(0);X.sim_step(100);  // anchor 90 % (below full so regen is allowed)
  X.sim_set_key(0);X.sim_step(500);if(prep)prep();X.sim_set_key(1);
  const P=DC.DRIVE_CYCLES[k],dt=0.1;let t=0,spd=0,gear='H',was=false,end="complete";
  const d={peakA:0,minA:0,dist:0,ah:0,soc0:soc().pct,soc1:0,lo:lowCell(),t:0,end:""};
  for(;;){const S=JSON.parse(cs(X.sim_state_json())),closed=S.state==="Closed";
    if(closed)was=true;else if(was){end="opened";break;}else if(t>8){end="never closed";break;}
    const c=DC.cycleAt(P,t);if(c.done)break;
    let hold=false;if(c.gear!==gear){if(Math.abs(spd)>1&&(c.gear==="R")!==(gear==="R"))hold=true;else gear=c.gear;}
    const vp=Object.assign({},VM.VEH_DEF,{gradePct:c.grade});
    const o=VM.vehStep(vp,{mph:spd},{thr:hold?0:Math.round(c.thr),brake:hold?50:c.brk,gear,closed,allowPct:S.kellyPct,regenOk:!!S.kellyRegenOk,driveLocked:!!S.driveLocked,packV:S.busV},dt);
    X.sim_set_pack_amps(o.packA);X.sim_step(100);spd=o.mph;const s=Math.abs(spd);
    const S2=JSON.parse(cs(X.sim_state_json()));
    d.dist+=s*dt/3600;d.ah+=S2.packA*dt/3600;d.peakA=Math.max(d.peakA,S2.packA);d.minA=Math.min(d.minA,S2.packA);d.lo=Math.min(d.lo,lowCell());t+=dt;}
  X.sim_set_pack_amps(0);d.t=t;d.end=end;d.soc1=soc().pct;d.state=JSON.parse(cs(X.sim_state_json())).state;return d;
}
{const P=DC.DRIVE_CYCLES.trail,a=DC.cycleAt(P,6.5),z=DC.cycleAt(P,1e6);
 dck(a.i===1&&a.gear==="L"&&Math.abs(a.thr-12.5)<0.01&&z.done&&DC.cycleLen(P)===125,"profile: 1 s throttle ramp, done flag",{end:"-",t:0,peakA:0,minA:0,dist:0,ah:0,soc0:0,soc1:0,lo:0});}
{const d=drive("trail");dck(d.end==="complete"&&d.peakA<650&&d.minA<0&&d.dist>0.1&&Math.abs((d.soc0-d.soc1)-d.ah/180*100)<0.05,"trail loop: mains stay closed, regen, SoC = Ah/180",d);}
{const d=drive("launch");dck(d.end==="complete"&&d.peakA>400&&d.peakA<=550.5&&d.minA<0,"hard launches: peak <= 550 A Kelly limit, regen, no trip",d);}
{const d=drive("climb");dck(d.end==="complete"&&d.soc0-d.soc1>10&&Math.abs((d.soc0-d.soc1)-d.ah/180*100)<0.2,"long climb 10 min: SoC drop matches Ah",d);}
{const d=drive("trail",()=>X.sim_set_cell(3,5,3.25));dck(d.end!=="complete"&&d.ah<0.01,"cell 3.25 V < low cut: mains never close",d);}
console.log(`drivecycle: ${dn-df}/${dn} checks passed`);
// ---- Charge mode + vehicle model / regen (Adam 2026-10-21)
let vf=0,vn=0;const vk=(ok,name,info)=>{vn++;if(!ok)vf++;console.log((ok?"PASS":"FAIL")+" chargemode/vehicle "+name.padEnd(46),info||"");};
const st=()=>JSON.parse(cs(X.sim_state_json()));
X.sim_reset();X.sim_set_plug(1);X.sim_step(4000);
{const S=st();vk(S.state==="Closed"&&S.mode==="CHARGE"&&S.chargeEnable&&S.chargerPath&&S.driveLocked&&S.kellyPct===0&&!S.keyOn,"plug in, key off: CHARGE, mains + charger path, drive 0 %",`${S.state} ${S.mode} ${S.chargeA}A why='${S.kellyWhy}'`);}
X.sim_set_key(1);X.sim_step(300);
{const S=st();vk(S.keyRefused&&S.mode==="CHARGE"&&S.kellyPct===0&&S.kellyWhy==="charging - drive locked","key on while plugged: refused, drive locked",`${S.mode} refused=${S.keyRefused}`);
 const o=VM.vehStep(VM.VEH_DEF,{mph:0},{thr:100,brake:0,gear:"H",closed:S.state==="Closed",allowPct:S.kellyPct,regenOk:!!S.kellyRegenOk,driveLocked:!!S.driveLocked,packV:S.busV},1);
 vk(o.packA===0&&o.mph===0,"throttle 100 % while plugged: ignored (0 A, 0 mph)",`${o.packA} A ${o.mph} mph`);}
X.sim_set_key(0);X.sim_set_temps(5,7);X.sim_step(200);
{const S=st();vk(S.chargeEnable&&S.chargeA<=S.ccfg.chargeMaxA*S.ccfg.chargeColdFrac+0.01&&S.chargeWhy==="cold: reduced","pack 5 C: charge reduced (0-10 C band, PLACEHOLDER 50 %)",`${S.chargeA} A`);}
X.sim_set_temps(-2,0);X.sim_step(200);
{const S=st();vk(!S.chargeEnable&&!S.chargerPath&&S.state==="Closed","pack -2 C: charge blocked, charger path open",`${S.chargeWhy}`);}
X.sim_set_temps(20,22);X.sim_set_charge_pct(80);for(let b=0;b<8;b++)for(let c=0;c<X.sim_board_cells(b);c++)X.sim_set_cell(b,c,3.93);X.sim_step(200);
{const S=st();vk(S.chargeEnable&&S.chargeA<4.9&&S.chargeA>0.9,"3.93 V near 80 % limit: tapered",`${S.chargeA.toFixed(2)} A`);}
let tStop=-1;for(let i=0;i<200&&tStop<0;i++){X.sim_step(500);if(st().chargeWhy==="at charge limit")tStop=i;}
{const S=st();vk(tStop>=0&&!S.chargeEnable&&S.plugIn,"taper then stop at the 80 % limit (plugged in)",`stop after ${(tStop+1)*0.5}s max cell ${(S.ceilingV).toFixed(3)} V`);}
X.sim_set_plug(0);X.sim_step(200);
{const S=st();vk(S.state==="Open"&&S.mode==="IDLE"&&!S.chargerPath,"unplug, key off: mains open, IDLE",S.state);}
X.sim_reset();X.sim_set_key(1);X.sim_step(4000);
{let spd=40,peak=0,below=null;const p=Object.assign({},VM.VEH_DEF);
 for(let i=0;i<200;i++){const S=st();const o=VM.vehStep(p,{mph:spd},{thr:0,brake:30,gear:"H",closed:S.state==="Closed",allowPct:S.kellyPct,regenOk:!!S.kellyRegenOk,driveLocked:!!S.driveLocked,packV:S.busV},0.1);
  X.sim_set_pack_amps(o.packA);X.sim_step(100);spd=o.mph;peak=Math.min(peak,st().packA);if(spd<1.5&&below==null)below=o;if(spd<=0)break;}
 vk(peak<=-59&&peak>=-60.5,"brake 30 % at 40 mph: regen to PROPOSED 60 A max",`${peak.toFixed(1)} A`);
 const lo=VM.vehStep(p,{mph:1.2},{thr:0,brake:30,gear:"H",closed:true,allowPct:100,regenOk:true,driveLocked:false,packV:140},0.01);
 vk(lo.regenA===0&&lo.Ffric>0&&below&&below.regenA<below.regenReqA,"below ~1.5 mph: regen faded, friction only",`${lo.regenA} A friction ${lo.Ffric.toFixed(0)} N`);}
{const p=Object.assign({},VM.VEH_DEF),S=st();const o=VM.vehStep(p,{mph:30},{thr:0,brake:15,gear:"H",closed:true,allowPct:100,regenOk:true,driveLocked:false,packV:S.busV},0.1);
 vk(o.regenA>15&&o.regenA<o.regenReqA+0.01&&Math.abs(o.regenReqA-30)<0.01,"brake 15 % = half the 30 % blend: 30 A asked",`${o.regenA.toFixed(1)}/${o.regenReqA} A`);
 const n=VM.vehStep(p,{mph:30},{thr:0,brake:30,gear:"H",closed:true,allowPct:100,regenOk:false,driveLocked:false,packV:S.busV},0.1);
 vk(n.regenA===0&&n.Ffric>0&&/BMS/.test(n.regenWhy),"BMS regen not allowed (full/cold): friction only",n.regenWhy);
 const l0=VM.vehStep(p,{mph:30},{thr:0,brake:0,gear:"H",closed:true,allowPct:100,regenOk:true,driveLocked:false,packV:S.busV},0.1);
 const l1=VM.vehStep(Object.assign({},p,{liftRegen:true}),{mph:30},{thr:0,brake:0,gear:"H",closed:true,allowPct:100,regenOk:true,driveLocked:false,packV:S.busV},0.1);
 vk(l0.regenA===0&&l1.regenA>19&&l1.regenA<20.01,"lift-off regen: off by default, 20 A when enabled",`${l0.regenA} / ${l1.regenA.toFixed(1)} A`);
 vk(Math.abs(VM.vehRpm(p,55)-6000)<15&&Math.abs(VM.vehRatio(p)-8.75)<0.01,"55 mph on 27 in, 16/32 x 4.375 diff = ~6000 RPM",`${VM.vehRpm(p,55).toFixed(0)} RPM`);}
// cold boot -> key on -> mains closed, drive allowed, throttle reaches speed (2026-10-21 regression guard)
X.sim_reset();X.sim_step(500);
{const s0=st();vk(s0.state==="Open"&&!s0.faults&&!s0.latched,"cold boot: open, no faults",`${s0.state} f=${s0.faults} l=${s0.latched}`);
 X.sim_set_key(1);let tC=-1;for(let i=0;i<60&&tC<0;i++){X.sim_step(100);if(st().state==="Closed")tC=(i+1)*100;}
 const s1=st();vk(tC>0&&tC<=3000&&s1.mode==="DRIVE"&&!s1.driveLocked&&!s1.keyRefused&&s1.kellyPct>0,"cold boot key-on: mains close <= 3 s, drive allowed",`closed at ${tC} ms mode=${s1.mode} locked=${s1.driveLocked} kelly=${s1.kellyPct}%`);
 const p=Object.assign({},VM.VEH_DEF);let spd=0;for(let i=0;i<150;i++){const S=st();const o=VM.vehStep(p,{mph:spd},{thr:60,brake:0,gear:"H",closed:S.state==="Closed",allowPct:S.kellyPct,regenOk:!!S.kellyRegenOk,driveLocked:!!S.driveLocked,packV:S.busV},0.1);X.sim_set_pack_amps(o.packA);X.sim_step(100);spd=o.mph;}
 X.sim_set_pack_amps(0);vk(spd>15&&st().state==="Closed","cold boot: 60 % throttle 15 s reaches > 15 mph, mains stay closed",`${spd.toFixed(1)} mph`);}
console.log(`chargemode/vehicle: ${vn-vf}/${vn} checks passed`);if(vf)process.exitCode=1;
const scFail=r.filter(x=>!x.pass).length;if(sf||df||scFail)process.exitCode=1;
// ---- Fault log page (BmsCore::FaultLog on the simulated EEPROM, sim only)
{let ff=0,fn=0;const fl=()=>JSON.parse(cs(X.sim_flog_json()));
 const fk=(ok,name)=>{fn++;if(!ok)ff++;const L=fl();console.log((ok?"PASS":"FAIL")+" faultlog-page "+name.padEnd(44),`boot=${L.boot} count=${L.count} written=${L.written} torn=${L.torn} newest=${L.recs[0]?L.recs[0].name+'/'+L.recs[0].kind:'-'}`);};
 X.sim_flog_clear();X.sim_reset();
 fk(fl().count===0&&fl().torn===0,"cleared: empty log");
 X.sim_set_key(1);X.sim_step(4000);X.sim_set_cell(0,6,3.10);X.sim_step(1500);
 {const L=fl();fk(L.recs.some(r=>r.kind===1&&r.code===3&&r.value===3100),"cell 3.10 V -> FAULT cell-low logged, value 3100 mV");}
 const b0=fl().boot,n0=fl().count;X.sim_power_cycle();
 {const L=fl();fk(L.boot===b0+1&&L.count===n0&&L.recs.some(r=>r.code===3&&r.boot===b0)&&L.written===0,"power cycle: records survive, boot +1");}
 X.sim_set_key(1);X.sim_set_pack_amps(800);X.sim_step(50);X.sim_set_pack_amps(0);X.sim_step(100);
 {const L=fl();fk(L.recs.some(r=>r.kind===1&&r.code===6&&r.value>=750&&r.boot===L.boot),"800 A spike -> FAULT overcurrent logged with amps");}
 const n1=fl().count;X.sim_flog_tear();
 {const L=fl();fk(L.torn===1&&L.count===n1&&L.recs[0].code===6,"pull power mid-write: torn record skipped, rest intact");}
 X.sim_set_key(1);X.sim_set_pack_amps(800);X.sim_step(50);X.sim_set_pack_amps(0);X.sim_step(100);
 {const L=fl();fk(L.recs[0].code===6&&L.recs[0].seq>L.recs[1].seq&&L.torn===0,"next write reuses torn slot, seq continues");}
 X.sim_flog_clear();X.sim_reset();
 console.log(`faultlog-page: ${fn-ff}/${fn} checks passed`);if(ff)process.exitCode=1;}
