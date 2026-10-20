// node scripts/check_bms_sim_wasm.js /tmp/bms_sim.wasm  -> same scenario table as scripts/sim_bms.cpp (parity) + 8 socpage checks (BmsCore::Soc exports); exit 1 on any FAIL
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
const scFail=r.filter(x=>!x.pass).length;if(sf||scFail)process.exitCode=1;
