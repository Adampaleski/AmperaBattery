// node scripts/check_bms_sim_wasm.js /tmp/bms_sim.wasm  -> same 13-row table as scripts/sim_bms.cpp (parity check)
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
