/* Teryx body controls: lights, 2WD/4WD, diff lock (Adam 2026-10-21). Pure logic, no DOM.
   Hardware plan (Mechanic): two 12 V linear actuators with pot feedback on the stock 4WD and diff-lock cables,
   each on an H-bridge + ADC (include/Config.h). Actuators/H-bridges UNVERIFIED, nothing bought.
   Conditions from the Kawasaki Teryx owner's manual (2010+ editions, e.g. KRF800 99803-0268 "2WD/4WD Shifting",
   "Shifting the Differential"): "Always stop the vehicle before changing from 2WD to 4WD and vice versa";
   diff lock: stop completely, do not lock while turning. 2008 KRF750 manual wording UNVERIFIED.
   Every threshold is PROPOSED and editable in Settings (localStorage teryxCtlSettings). */
(function(g){
const CTL_DEF={wdMaxMph:1,wdThrMaxPct:2,wdNeedBrake:false,diffMaxMph:1,diffNeedThrOff:true,diffAutoUnlockMph:15,
  wd2Pos:5,wd4Pos:95,difOffPos:5,difPartPos:50,difFullPos:95,posTolPct:3,actTimeoutMs:4000,stallMs:600,stallMinPct:1,
  actRatePct:40,blinkHz:1.5,brakeOnPct:3};
const CTL_META=[['wdMaxMph','2WD/4WD: shift only at or below','mph','PROPOSED 1 mph ≈ stopped (manual: stop the vehicle)',0,10,0.5],
 ['wdThrMaxPct','Throttle counts as released below','%','PROPOSED',0,20,1],
 ['wdNeedBrake','2WD/4WD: also require brake','bool','PROPOSED off; manual names no brake step',0,1,1],
 ['diffMaxMph','Diff lock: engage only at or below','mph','PROPOSED 1 mph (manual: stop completely)',0,10,0.5],
 ['diffNeedThrOff','Diff lock: require throttle released','bool','PROPOSED',0,1,1],
 ['diffAutoUnlockMph','Diff lock: auto-unlock above','mph','PROPOSED 15 (crew)',3,40,1],
 ['wd2Pos','4WD actuator endpoint: 2WD','% travel','PLACEHOLDER: set on the bench',0,100,1],
 ['wd4Pos','4WD actuator endpoint: 4WD','% travel','PLACEHOLDER (~3/4 in cable travel)',0,100,1],
 ['difOffPos','Diff actuator: Off','% travel','PLACEHOLDER',0,100,1],
 ['difPartPos','Diff actuator: Partial','% travel','PLACEHOLDER',0,100,1],
 ['difFullPos','Diff actuator: Full','% travel','PLACEHOLDER',0,100,1],
 ['posTolPct','Position reached within','%','PROPOSED',1,10,0.5],
 ['actTimeoutMs','Actuator timeout fault after','ms','PROPOSED',500,15000,100],
 ['stallMs','Actuator stall fault: no movement for','ms','PROPOSED',100,5000,50],
 ['stallMinPct','Stall: movement less than','%','PROPOSED',0.2,10,0.1],
 ['actRatePct','Sim actuator speed','%/s','SIM PLACEHOLDER (real speed per part datasheet)',5,200,5],
 ['blinkHz','Turn / hazard blink rate','Hz','PROPOSED 1.5 Hz (~90/min)',0.5,3,0.1],
 ['brakeOnPct','Brake light on above','% pedal','PROPOSED',0,20,1]];
const LIGHTS=[['low','Headlights low'],['high','High beam'],['tail','Tail lights'],['bar','Light bar / aux'],['work','Work lights']];
const DIFF=['off','partial','full'];
function loadSet(){try{return Object.assign({},CTL_DEF,JSON.parse(localStorage.getItem('teryxCtlSettings')||'{}'));}catch(e){return Object.assign({},CTL_DEF);}}
function create(){
  const s0=loadSet();
  const act=(name,pos)=>({name,pos,target:pos,moving:false,t0:0,lastMove:0,lastPos:pos,fault:'',stall:false});
  const S={set:s0,wd:'2WD',diff:'off',a4:act('4WD actuator',s0.wd2Pos),ad:act('Diff actuator',s0.difOffPos),
    turn:null,hazard:false,lights:{low:false,high:false,tail:false,bar:false,work:false},msg:'',msgT:0,log:[],faults:[],onLog:null};
  const save=()=>localStorage.setItem('teryxCtlSettings',JSON.stringify(S.set));
  const note=(t,now)=>{S.msg=t;S.msgT=now||Date.now();S.log.push(t);if(S.onLog)S.onLog(t);};
  const refuse=(t,now)=>{note(t,now);return {ok:false,why:t};};
  const diffPos=k=>({off:S.set.difOffPos,partial:S.set.difPartPos,full:S.set.difFullPos})[k];
  const busy=()=>S.a4.moving||S.ad.moving;
  function wdBlock(d){const s=S.set,mph=Math.abs(d.mph||0);
    if(S.a4.fault)return '4WD actuator fault: '+S.a4.fault+' (clear in Lights · 4WD)';
    if(d.mph==null)return 'No wheel speed: cannot check the 4WD shift speed';
    if(busy())return 'Wait: actuator still moving';
    if(S.diff!=='off')return 'Unlock the diff before leaving 4WD';
    if(mph>s.wdMaxMph)return `Stop (≤ ${s.wdMaxMph} mph) to shift 2WD/4WD`;
    if((d.thr||0)>s.wdThrMaxPct)return 'Release the throttle to shift 2WD/4WD';
    if(s.wdNeedBrake&&!((d.brake||0)>s.brakeOnPct))return 'Press the brake to shift 2WD/4WD';
    return '';}
  function diffBlock(d,k){const s=S.set,mph=Math.abs(d.mph||0);
    if(S.ad.fault)return 'Diff actuator fault: '+S.ad.fault+' (clear in Lights · 4WD)';
    if(k==='off')return '';
    if(S.wd!=='4WD')return 'Diff lock only in 4WD';
    if(d.mph==null)return 'No wheel speed: cannot check the diff lock speed';
    if(busy())return 'Wait: actuator still moving';
    if(mph>s.diffMaxMph)return `Stop (≤ ${s.diffMaxMph} mph) to lock the diff`;
    if(s.diffNeedThrOff&&(d.thr||0)>s.wdThrMaxPct)return 'Release the throttle to lock the diff';
    return '';}
  const move=(a,to,now)=>{a.target=to;a.moving=true;a.t0=now;a.lastMove=now;a.lastPos=a.pos;};
  function toggle4wd(d){const now=d.now||Date.now(),w=wdBlock(d);if(w)return refuse(w,now);const to=S.wd==='4WD'?'2WD':'4WD';
    S.a4.goal=to;move(S.a4,to==='4WD'?S.set.wd4Pos:S.set.wd2Pos,now);note('4WD actuator → '+to,now);return {ok:true};}
  function setDiff(d,k){const now=d.now||Date.now(),w=diffBlock(d,k);if(w)return refuse(w,now);
    S.ad.goal=k;move(S.ad,diffPos(k),now);note('Diff → '+k+(k!=='off'?' · WARNING: steering angle not sensed, keep wheels straight':''),now);return {ok:true};}
  function clearFault(){S.a4.fault=S.ad.fault='';note('Actuator faults cleared by driver');}
  function stepAct(a,dt,now,onDone){const s=S.set;
    if(!a.moving)return;
    if(!a.stall){const dp=s.actRatePct*dt,e=a.target-a.pos;a.pos+=Math.abs(e)<=dp?e:Math.sign(e)*dp;}
    if(Math.abs(a.pos-a.lastPos)>=s.stallMinPct){a.lastPos=a.pos;a.lastMove=now;}
    if(Math.abs(a.target-a.pos)<=s.posTolPct){a.moving=false;onDone();return;}
    const f=now-a.lastMove>=s.stallMs?`stalled at ${a.pos.toFixed(0)} % (target ${a.target} %)`:now-a.t0>=s.actTimeoutMs?`timeout after ${s.actTimeoutMs} ms at ${a.pos.toFixed(0)} %`:'';
    if(f){a.moving=false;a.fault=f;const t=a.name+' fault: '+f+' · H-bridge off';S.faults.push({t:now,text:t});note(t,now);}}
  let lastT=0;
  // d={mph,thr,brake,gear}; returns outputs + tell-tales
  function tick(d,now){now=now||Date.now();const s=S.set,dt=lastT?Math.min(0.5,(now-lastT)/1000):0;lastT=now;
    stepAct(S.a4,dt,now,()=>{S.wd=S.a4.goal;});
    stepAct(S.ad,dt,now,()=>{S.diff=S.ad.goal;});
    if(S.diff!=='off'&&!S.ad.moving&&!S.ad.fault&&d.mph!=null&&Math.abs(d.mph)>s.diffAutoUnlockMph){S.ad.goal='off';move(S.ad,s.difOffPos,now);note(`Diff auto-unlocked above ${s.diffAutoUnlockMph} mph`,now);}
    const ph=Math.floor(now/1000*s.blinkHz*2)%2===0,L=S.lights,hb=a=>a.moving?(a.target>a.pos?'extend':'retract'):'off';
    const out={headLow:L.low||L.high,headHigh:L.high,tail:L.tail||L.low,brake:(d.brake||0)>s.brakeOnPct,reverse:d.gear==='R',
      turnL:(S.hazard||S.turn==='L')&&ph,turnR:(S.hazard||S.turn==='R')&&ph,bar:L.bar,work:L.work,hb4wd:hb(S.a4),hbDiff:hb(S.ad)};
    return {out,wd:S.wd,wdShifting:S.a4.moving,diff:S.diff,diffShifting:S.ad.moving,pos4:S.a4.pos,posD:S.ad.pos,
      fault4:S.a4.fault,faultD:S.ad.fault,msg:now-S.msgT<6000?S.msg:'',wdWhy:wdBlock(d),diffWhy:{partial:diffBlock(d,'partial'),full:diffBlock(d,'full'),off:diffBlock(d,'off')}};}
  return {S,save,tick,toggle4wd,setDiff,clearFault,wdBlock,diffBlock,
    setLight:(k,v)=>{S.lights[k]=v;if(k==='high'&&v)S.lights.low=true;if(k==='low'&&!v)S.lights.high=false;},
    setTurn:t=>{S.turn=S.turn===t?null:t;},setHazard:v=>{S.hazard=v;},simStall:(which,v)=>{(which==='4wd'?S.a4:S.ad).stall=v;}};
}
g.TeryxControls={create,CTL_DEF,CTL_META,LIGHTS,DIFF};
})(typeof window!=='undefined'?window:globalThis);
