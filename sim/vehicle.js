// Vehicle model for tools/bms_sim.html (Cluster tab), drive cycles and scripts/check_bms_sim_wasm.js.
// SIM ONLY. Every default below is ASSUMED or PROPOSED (tagged in VEH_SETS), not measured on the Teryx.
// Point-mass longitudinal model: motor torque (Kelly phase-amp limit x ASSUMED Kt) and Kelly battery-amp
// limit -> wheel force through 16/32 sprockets (SOURCED) x 4.375 diff (SOURCED, 2008 owner's manual); minus rolling, aero, grade, friction brake
// and regen. Brake pedal = regen first (Kelly Brake_AN analog regen input, pin per the brief, UNVERIFIED on
// Adam's harness), friction tops up; regen fades out below ~3 mph and friction takes over.
const VEH_DEF={tireIn:24,sprFront:16,sprRear:32,diffRatio:4.375,massKg:900,crr:0.030,cdA:1.20,gradePct:0,
  maxRegenA:60,regenBlendPct:30,liftRegen:false,liftRegenA:20,eta:0.88,ktNmA:0.26,phaseA:550,battA:550,
  frictionG:0.6,fadeLoMph:1.5,fadeHiMph:4.5};
// [key, label, unit, tag, min, max, step]
const VEH_SETS=[['tireIn','Tire diameter','in','SOURCED: Adam (stock tires, 24")',18,40,0.5],['sprFront','Drive sprocket','teeth','SOURCED: cash ledger (#50 chain, 16T)',8,40,1],
  ['sprRear','Driven sprocket','teeth','SOURCED: cash ledger (#50 chain, 32T → 2.0:1)',16,80,1],['diffRatio','Diff ratio','× : 1','SOURCED: 2008 Teryx owner\'s manual 4.375:1 (35/8, front = rear) · total 2.0 × 4.375 = 8.75',1,12,0.005],
  ['massKg','Mass (with driver + pack)','kg','ASSUMED',400,1600,10],['crr','Rolling resistance Crr','','ASSUMED (dirt / ATV tires)',0.005,0.15,0.001],
  ['cdA','Drag area CdA','m²','ASSUMED',0.3,3,0.05],['gradePct','Grade','%','SIM input',-30,30,0.5],
  ['maxRegenA','Max regen current','A','PROPOSED 60 A',0,200,5],['regenBlendPct','Regen brake blend','% pedal','PROPOSED: regen-only up to this pedal, then friction adds',0,100,5],
  ['liftRegen','Throttle lift-off regen','on/off','PROPOSED off by default',0,1,1],['liftRegenA','Lift-off regen current','A','PROPOSED',0,100,5],
  ['ktNmA','Motor torque constant','Nm/A','PLACEHOLDER: Zero Z75-7 / 75-7R IPM (cash ledger); kW, torque, rpm, voltage MISSING',0.05,1,0.01],['eta','Drive efficiency','','ASSUMED 0.88',0.5,1,0.01],
  ['phaseA','Kelly phase current limit','A','SOURCED: Adam 550 A (Kelly config not yet read back)',50,800,10],['battA','Kelly battery current limit','A','SOURCED: Adam 550 A',50,800,10]];
// Kelly speed modes. sc = throttle scale, rpm = motor RPM cap. PROPOSED placeholders, not Kelly settings.
const VEH_GEARS={L:{n:'Low',sc:.45,rpm:2700},M:{n:'Medium',sc:.70,rpm:4400},H:{n:'High',sc:1,rpm:6000},R:{n:'Reverse',sc:.25,rpm:1100}};
const MPS_PER_MPH=0.44704,G=9.81,RHO=1.2;
function vehRatio(p){return p.sprRear/p.sprFront*p.diffRatio;}
// Saved settings from before the rename: 'gearbox' was the diff ratio all along -> diffRatio.
function vehMigrate(o){o=o||{};if(o.tireIn===27)o.tireIn=24;  // old ASSUMED 27" default -> Adam's stock 24"
  if(o.gearbox!=null){if(o.diffRatio==null)o.diffRatio=o.gearbox;delete o.gearbox;}return o;}
function vehWheelR(p){return p.tireIn*0.0254/2;}
function vehRpm(p,mph){return Math.abs(mph)*MPS_PER_MPH/vehWheelR(p)*vehRatio(p)*60/(2*Math.PI);}
function vehMphAtRpm(p,rpm){return rpm*2*Math.PI/60/vehRatio(p)*vehWheelR(p)/MPS_PER_MPH;}
function vehFade(p,mph){const v=Math.abs(mph);return v<=p.fadeLoMph?0:v>=p.fadeHiMph?1:(v-p.fadeLoMph)/(p.fadeHiMph-p.fadeLoMph);}
// One step. st={mph} (signed, + forward). inp={thr 0..100, brake 0..100, gear, closed, allowPct, regenOk, driveLocked, packV}
// Returns new mph, motor rpm, pack amps (+ discharge, - regen), forces and why-strings.
function vehStep(p,st,inp,dt){
  const g=VEH_GEARS[inp.gear]||VEH_GEARS.H,dir=inp.gear==='R'?-1:1,r=vehWheelR(p),k=vehRatio(p);
  const V=Math.max(1,inp.packV||0),m=p.massKg,v=Math.abs(st.mph)*MPS_PER_MPH;
  const live=!!inp.closed&&!inp.driveLocked,rpm=vehRpm(p,st.mph),w=rpm*2*Math.PI/60;
  // drive torque: throttle x gear scale x BMS allowance (Teensy scales the throttle line, never raises it)
  const thr=Math.max(0,Math.min(100,inp.thr))/100,brake=Math.max(0,Math.min(100,inp.brake))/100;
  let T=0,why='';
  if(live&&thr>0&&brake===0){
    T=p.ktNmA*p.phaseA*thr*g.sc*Math.max(0,inp.allowPct)/100;               // phase-amp torque limit
    const Pmax=p.eta*V*p.battA;if(w>1&&T*w>Pmax){T=Pmax/w;why='Kelly battery limit';}   // battery-amp limit
    if(rpm>=g.rpm*0.95){T*=Math.max(0,(g.rpm-rpm)/(g.rpm*0.05));why='speed-mode RPM cap';}
  }
  const driveA=T*w/(p.eta*V);
  // regen request: brake pedal (regen-first blend) or optional lift-off; faded at low speed; BMS can veto
  let regReq=0,rwhy='';
  if(brake>0)regReq=p.maxRegenA*Math.min(1,p.regenBlendPct>0?brake/(p.regenBlendPct/100):1);
  else if(p.liftRegen&&thr===0)regReq=p.liftRegenA;
  const fade=vehFade(p,st.mph);let regA=regReq*fade;
  if(regReq>0&&!live){regA=0;rwhy=inp.driveLocked?'drive locked':'contactors open';}
  else if(regReq>0&&!inp.regenOk){regA=0;rwhy='BMS: regen not allowed';}
  else if(regReq>0&&fade<1)rwhy=fade>0?'fading (low speed)':'below ~3 mph: friction only';
  // forces along travel (positive = forward along travel direction)
  const Fdrive=T*k/r;
  const Ftarget=brake*p.frictionG*m*G;
  // regen force from the amps request, capped by motor torque and (on the pedal) by the driver's brake demand;
  // the pack current is then what that force really returns (low speed = little current).
  let Fregen=v>0.05?Math.min(regA*V*p.eta/v,p.ktNmA*p.phaseA*k/r):0;
  if(brake>0)Fregen=Math.min(Fregen,Ftarget);
  regA=Fregen*v/(V*p.eta);
  const Ffric=brake>0?Math.max(0,Ftarget-Fregen):0;
  const th=Math.atan((p.gradePct||0)/100),Fgrade=m*G*Math.sin(th)*dir;   // uphill opposes travel in the gear direction
  const Froll=p.crr*m*G*Math.cos(th),Faero=0.5*RHO*p.cdA*v*v;
  let s=dir*st.mph>=0?v:-v;                                              // speed along the gear direction
  const sgn=s>0?1:s<0?-1:0;
  let Fnet=Fdrive-Fgrade-sgn*(Froll+Faero+Ffric+Fregen);
  if(sgn===0){const resist=Froll+Ffric;if(Math.abs(Fdrive-Fgrade)<=resist)Fnet=0;else Fnet=(Fdrive-Fgrade)-Math.sign(Fdrive-Fgrade)*resist;}
  let s1=s+Fnet/m*dt;
  if(sgn!==0&&Math.sign(s1)!==sgn&&(brake>0||Fdrive===0&&Math.abs(Fgrade)<=Froll))s1=0;  // brakes/rolling stop, no reversal
  const mph=dir*s1/MPS_PER_MPH;
  const packA=live?driveA-regA:0;
  return {mph,rpm:vehRpm(p,mph),packA,driveA,regenA:regA,regenReqA:regReq,fade,Fdrive,Fregen,Ffric,Froll,Faero,Fgrade,why,regenWhy:rwhy};
}
