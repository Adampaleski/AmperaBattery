// node scripts/check_bms_sim_wasm.js /tmp/bms_sim.wasm  -> same scenario table as scripts/sim_bms.cpp (parity) + 8 socpage checks (BmsCore::Soc exports) + 5 drivecycle checks (sim/drive_cycles.js); exit 1 on any FAIL
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
const GEARS={L:{sc:.45,cap:25},M:{sc:.70,cap:40},H:{sc:1,cap:55},R:{sc:.25,cap:10}};  // same PROPOSED placeholders as the page
let df=0,dn=0;
const dck=(ok,name,d)=>{dn++;if(!ok)df++;console.log((ok?"PASS":"FAIL")+" drivecycle "+name.padEnd(44),`end=${d.end} t=${d.t.toFixed(0)}s peak=${d.peakA.toFixed(0)}A regen=${(-d.minA).toFixed(0)}A dist=${d.dist.toFixed(2)}mi Ah=${d.ah.toFixed(2)} soc=${d.soc0.toFixed(1)}->${d.soc1.toFixed(1)} lowCell=${d.lo.toFixed(3)}`);};
const lowCell=()=>{let lo=9;for(let b=0;b<8;b++)for(let c=0;c<X.sim_board_cells(b);c++)lo=Math.min(lo,X.sim_get_cell(b,c));return lo;};
function drive(k,prep){
  X.sim_reset();X.sim_set_soc_drain(1);X.sim_set_key(1);X.sim_step(4000);X.sim_set_charge(1);fill(100);X.sim_step(200);X.sim_set_charge(0);X.sim_step(100);  // anchor 100 %
  X.sim_set_key(0);X.sim_step(500);if(prep)prep();X.sim_set_key(1);
  const P=DC.DRIVE_CYCLES[k],dt=0.1;let t=0,spd=0,gear='H',was=false,end="complete";
  const d={peakA:0,minA:0,dist:0,ah:0,soc0:soc().pct,soc1:0,lo:lowCell(),t:0,end:""};
  for(;;){const S=JSON.parse(cs(X.sim_state_json())),closed=S.state==="Closed";
    if(closed)was=true;else if(was){end="opened";break;}else if(t>8){end="never closed";break;}
    const c=DC.cycleAt(P,t);if(c.done)break;
    let hold=false;if(c.gear!==gear){if(Math.abs(spd)>1&&(c.gear==="R")!==(gear==="R"))hold=true;else gear=c.gear;}
    const g=GEARS[gear],thr=hold?0:Math.round(c.thr)/100,brk=c.brk||hold;
    let a=0;if(closed&&!brk)a=thr*g.sc*550*S.kellyPct/100;if(closed&&brk&&Math.abs(spd)>1&&S.kellyRegenOk)a=-60;
    X.sim_set_pack_amps(a);X.sim_step(100);
    const S2=JSON.parse(cs(X.sim_state_json())),kw=S2.busV*S2.packA/1000,cap=g.cap*(S2.kellyLimp?.4:1);
    const acc=(Math.max(0,kw)/120)*12*(1-Math.abs(spd)/Math.max(1,cap))-(brk?15:0)-1.0-(kw<0?3:0);
    let s=Math.abs(spd)+acc*dt;s=Math.max(0,Math.min(cap,s));spd=(gear==="R"?-1:1)*s;
    d.dist+=s*dt/3600;d.ah+=S2.packA*dt/3600;d.peakA=Math.max(d.peakA,S2.packA);d.minA=Math.min(d.minA,S2.packA);d.lo=Math.min(d.lo,lowCell());t+=dt;}
  X.sim_set_pack_amps(0);d.t=t;d.end=end;d.soc1=soc().pct;d.state=JSON.parse(cs(X.sim_state_json())).state;return d;
}
{const P=DC.DRIVE_CYCLES.trail,a=DC.cycleAt(P,6.5),z=DC.cycleAt(P,1e6);
 dck(a.i===1&&a.gear==="L"&&Math.abs(a.thr-12.5)<0.01&&z.done&&DC.cycleLen(P)===125,"profile: 1 s throttle ramp, done flag",{end:"-",t:0,peakA:0,minA:0,dist:0,ah:0,soc0:0,soc1:0,lo:0});}
{const d=drive("trail");dck(d.end==="complete"&&d.peakA<650&&d.minA<0&&d.dist>0.1&&Math.abs((d.soc0-d.soc1)-d.ah/180*100)<0.05,"trail loop: mains stay closed, regen, SoC = Ah/180",d);}
{const d=drive("launch");dck(d.end==="complete"&&d.peakA>500&&d.peakA<=550.5,"hard launches: peak <= 550 A Kelly limit, no trip",d);}
{const d=drive("climb");dck(d.end==="complete"&&d.soc0-d.soc1>15&&Math.abs((d.soc0-d.soc1)-d.ah/180*100)<0.2,"long climb 10 min: SoC drop matches Ah",d);}
{const d=drive("trail",()=>X.sim_set_cell(3,5,3.25));dck(d.end!=="complete"&&d.ah<0.01,"cell 3.25 V < low cut: mains never close",d);}
console.log(`drivecycle: ${dn-df}/${dn} checks passed`);
const scFail=r.filter(x=>!x.pass).length;if(sf||df||scFail)process.exitCode=1;
