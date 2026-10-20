// Drive-cycle profiles for tools/bms_sim.html (Cluster tab) and scripts/check_bms_sim_wasm.js.
// SIM PROFILES: made-up driver inputs for exercising the BMS, NOT logged Teryx data.
// Segment: [seconds, gear L/M/H/R, throttle %, brake 0/1, label]. The page feeds these into
// the same driver-input model as the Throttle slider (gear scales and speed caps PROPOSED).
const DRIVE_CYCLES={
  trail:{name:'Trail loop',note:'crawl, launch, cruise, climb, coast, regen stop, back out',seg:[
    [6,'H',0,0,'Key on, mains closing'],[20,'L',25,0,'Crawl in Low'],[6,'H',85,0,'Launch'],[40,'H',35,0,'Cruise'],
    [15,'H',90,0,'Hill climb'],[10,'H',0,0,'Coast'],[10,'H',0,1,'Brake + regen'],[4,'H',0,0,'Stopped'],
    [6,'R',40,0,'Back out'],[4,'R',0,1,'Brake'],[4,'H',0,0,'Park']]},
  launch:{name:'Hard launches',note:'5 × full throttle in High, brake to a stop',seg:[[6,'H',0,0,'Key on, mains closing']].concat(
    ...Array.from({length:5},(_,i)=>[[5,'H',100,0,'Full launch '+(i+1)+'/5'],[6,'H',0,1,'Brake + regen'],[4,'H',0,0,'Idle']]))},
  climb:{name:'Long climb',note:'10 min at 75 % in Medium (drains the pack; use 20×)',seg:[
    [6,'H',0,0,'Key on, mains closing'],[600,'M',75,0,'Long climb'],[10,'M',0,1,'Brake + regen'],[4,'M',0,0,'Stopped']]}
};
function cycleLen(p){return p.seg.reduce((a,s)=>a+s[0],0);}
// Inputs at t seconds into profile p: {i, seg, gear, thr, brk, label, done}. Throttle ramps 1 s.
function cycleAt(p,t){
  let t0=0;
  for(let i=0;i<p.seg.length;i++){const [d,g,thr,brk,label]=p.seg[i];
    if(t<t0+d){const prev=i?p.seg[i-1][2]:0,k=Math.min(1,(t-t0)/1);return {i,gear:g,thr:prev+(thr-prev)*k,brk:!!brk,label,done:false};}
    t0+=d;}
  return {i:p.seg.length,gear:'H',thr:0,brk:false,label:'Done',done:true};
}
