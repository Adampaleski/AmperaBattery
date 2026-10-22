// Vehicle model for tools/bms_sim.html (Cluster tab), drive cycles and scripts/check_bms_sim_wasm.js.
// SIM ONLY. Every default below is ASSUMED or PROPOSED (tagged in VEH_SETS), not measured on the Teryx.
// Point-mass longitudinal model: motor torque (Kelly phase-amp limit x ASSUMED Kt) and Kelly battery-amp
// limit -> wheel force through 16/32 sprockets (SOURCED) x 4.375 diff (SOURCED, 2008 owner's manual); minus rolling, aero, grade, friction brake
// and regen. DEFAULT (crew 2026-10-21, first drive): Kelly SWITCH regen at a fixed BRK_SW Brk level when the 12 V
// brake switch closes. Brake_AN analog is DEFERRED (needs Kelly config readback + Boost OFF). Optional model: analog hall pedal 0-5 V on Kelly
// Brake_AN pin 2 (Brake Type = 2, Boost OFF). Regen is PROPORTIONAL between Brake Dead Low (20 %) and Brake Dead
// High (80 %), max = BRK_AD Brk strength (0-50 %, default 0 = off). Kelly exits regen below Brk_Speed Limit RPM.
// The 12 V brake switch (Kelly pin 25 + brake light + Teensy) cuts the throttle here (Kelly-side: Brake H-Pedal).
// Hydraulic friction is NOT blended: it always follows the pedal; regen adds on top. Regen is still capped by the
// BMS (regenOk: cells full / pack cold) and a PROPOSED taper regenAllowPct (SOC + cold, from the sim page).
const VEH_DEF={tireIn:24,sprFront:16,sprRear:32,diffRatio:4.375,massKg:900,crr:0.030,cdA:1.20,gradePct:0,
  maxRegenA:60,regenBlendPct:30,brkDeadLo:20,brkDeadHi:80,brkAdPct:30,brkSwPct:3,brkMinRpm:100,analogBrake:false,brkSwRegenPct:10,liftRegen:false,ntlBrkPct:0,liftRegenA:20,eta:0.88,ktNmA:0.26,phaseA:550,battA:550,
  frictionG:0.6,dtDragN:60,parkG:0.25,lowCapRpm:1225,fadeLoMph:1.5,fadeHiMph:4.5};
// [key, label, unit, tag, min, max, step]
const VEH_SETS=[['tireIn','Tire diameter','in','SOURCED: Adam (stock tires, 24")',18,40,0.5],['sprFront','Drive sprocket','teeth','SOURCED: cash ledger (#50 chain, 16T)',8,40,1],
  ['sprRear','Driven sprocket','teeth','SOURCED: cash ledger (#50 chain, 32T → 2.0:1)',16,80,1],['diffRatio','Diff ratio','× : 1','SOURCED: 2008 Teryx owner\'s manual 4.375:1 (35/8, front = rear) · total 2.0 × 4.375 = 8.75',1,12,0.005],
  ['massKg','Mass (with driver + pack)','kg','ASSUMED',400,1600,10],['crr','Rolling resistance Crr','','ASSUMED (dirt / ATV tires)',0.005,0.15,0.001],
  ['cdA','Drag area CdA','m²','ASSUMED',0.3,3,0.05],['gradePct','Grade (+ = nose uphill)','%','SIM input',-30,30,0.5],['dtDragN','Drivetrain drag','N','ASSUMED (chain + diffs + motor iron loss)',0,500,5],
  ['maxRegenA','Max regen current','A','PROPOSED 60 A',0,200,5],['analogBrake','Analog brake pedal on Kelly Brake_AN (DEFERRED)','on/off','DEFERRED for first drive (crew): needs Kelly config readback + Boost OFF confirmed. Default off = brake-switch regen at a fixed level',0,1,1],['brkSwRegenPct','Kelly BRK_SW Brk (switch regen strength)','% (0-50)','SOURCED range 0-50 %, default 10 %; amps mapping UNVERIFIED (× maxRegenA / 50)',0,50,1],['brkDeadLo','Kelly Brake Dead Low','% pedal','SOURCED default 20 % (=1 V)',5,40,1],['brkDeadHi','Kelly Brake Dead High','% pedal','SOURCED default 80 % (=4 V)',60,95,1],['brkAdPct','Kelly BRK_AD Brk (pedal regen strength)','% (0-50)','SOURCED range 0-50 %, default 0; 30 PROPOSED; mapping to amps UNVERIFIED (maxRegenA scales it)',0,50,1],['brkSwPct','Brake switch closes above','% pedal','PROPOSED (switch position on pedal UNVERIFIED)',0,20,1],['brkMinRpm','Kelly Brk_Speed Limit','RPM','SOURCED range 0-500, default 0; 100 PROPOSED',0,500,10],
  ['ntlBrkPct','Regen in Neutral (Kelly NTL Brk)','%','SOURCED: Kelly manual item 45 NTL Brk, 0–50 %, default 0 = no regen in neutral',0,50,1],['liftRegen','Throttle lift-off regen','on/off','PROPOSED off by default',0,1,1],['liftRegenA','Lift-off regen current','A','PROPOSED',0,100,5],
  ['lowCapRpm','Kelly Low speed cap (ECO pin 22)','RPM','PROPOSED 1225 RPM ≈ 10 mph (LowSpeed Forw ≈ 20 % of 6000); fail-safe relay, ECO pin + relay UNVERIFIED',300,6000,25],['ktNmA','Motor torque constant','Nm/A','PLACEHOLDER: Zero Z75-7 / 75-7R IPM (cash ledger); kW, torque, rpm, voltage MISSING',0.05,1,0.01],['eta','Drive efficiency','','ASSUMED 0.88',0.5,1,0.01],
  ['phaseA','Kelly phase current limit','A','SOURCED: Adam 550 A (Kelly config not yet read back)',50,800,10],['battA','Kelly battery current limit','A','SOURCED: Adam 550 A',50,800,10]];
// Drive modes = pedal-map profiles (Adam approved 2026-10-21). Mirror of include/DriveProfile.h kDefaults
// (parity-checked against the wasm build). ALL values PROPOSED, editable in Settings. Reduce-only trim:
// expo (0 linear .. 1 soft), cur = pedal cap % (current-limit proxy), ramp A/s (vs 550 A), top mph, regen %.
// rpm = Kelly 6000 RPM cap (all forward modes). Reverse keeps its own PROPOSED profile.
const VEH_MODE_DEF={H:{n:'High',expo:0,cur:100,ramp:2000,top:49,regen:100,rpm:6000},M:{n:'Medium',expo:.3,cur:70,ramp:1000,top:36,regen:80,rpm:6000},
  L:{n:'Low',expo:.5,cur:45,ramp:600,top:22,regen:60,rpm:6000},SNOW:{n:'Snow',expo:.8,cur:40,ramp:150,top:25,regen:25,rpm:6000},
  MUD:{n:'Mud',expo:0,cur:90,ramp:2500,top:30,regen:20,rpm:6000},SAND:{n:'Sand',expo:0,cur:100,ramp:3000,top:35,regen:15,rpm:6000},
  KID:{n:'Kid',expo:.7,cur:20,ramp:60,top:10,regen:30,rpm:6000},R:{n:'Reverse',expo:.3,cur:25,ramp:500,top:9,regen:30,rpm:1100}};
const VEH_GEARS=JSON.parse(JSON.stringify(VEH_MODE_DEF));   // live (edited) profiles
const VEH_MODE_ORDER=['H','M','L','SNOW','MUD','SAND','KID'];
const VEH_MODE_FIELDS=[['expo','Pedal curve (expo)','0–1',0,1,0.05],['cur','Current limit','% pedal',0,100,1],['ramp','Ramp','A/s',10,5000,10],['top','Top speed','mph',1,60,1],['regen','Regen strength','%',0,100,5]];
function vehModeSet(k,f,v){const g=VEH_GEARS[k],d=VEH_MODE_FIELDS.find(x=>x[0]===f);if(!g||!d||!(v>=d[3]&&v<=d[4]))return false;g[f]=+v;return true;}
// Kid PIN (mirror of DriveProfile::Selector): Kid can't be entered until a PIN is set; leaving it needs the PIN.
// PIN kept only as hash (mirror of DriveProfile::hashPin: salted FNV-1a x1000); 0 = no PIN.
const VEH_KID={hash:0};
function vehHashPin(pin){pin=(+pin)&0xFFFF;if(!pin)return 0;let h=2166136261>>>0;const salt='TeryxKid1';
  for(let it=0;it<1000;it++){for(let i=0;i<salt.length;i++){h^=salt.charCodeAt(i);h=Math.imul(h,16777619)>>>0;}h^=pin&0xFF;h=Math.imul(h,16777619)>>>0;h^=pin>>8;h=Math.imul(h,16777619)>>>0;}
  return h>>>0||1;}
function vehSelectMode(cur,next,pin){if(!VEH_GEARS[next])return 'unknown mode';if(next==='KID'&&cur!=='KID'&&!VEH_KID.hash)return 'Set a Kid PIN first (Settings)';
  if(cur==='KID'&&next!=='KID'){if(!pin)return 'PIN required to leave Kid';if(!/^[0-9]{1,4}$/.test(String(pin))||vehHashPin(pin)!==VEH_KID.hash)return 'Wrong PIN';}return '';}
function vehCurve(x,e){x=Math.max(0,Math.min(1,x));e=Math.max(0,Math.min(1,e));return (1-e)*x+e*x*x*x;}
// Reduce-only trim step (mirror of DriveProfile::Trim). tr={filt,out} persists between calls; dtMs step.
function vehTrim(g,tr,pedal,mph,allow,ok,dtMs){const raw=Math.max(0,Math.min(100,pedal));if(!ok){tr.out=0;tr.filt=raw;return 0;}
  const steps=Math.max(1,Math.round(dtMs));for(let i=0;i<steps;i++){tr.filt+=(raw-tr.filt)*(1/(5+1));
    let t=vehCurve(tr.filt/100,g.expo)*100*Math.max(0,Math.min(100,g.cur))/100*Math.max(0,Math.min(100,allow))/100;
    const over=Math.abs(mph)-(g.top-2);if(over>0)t*=Math.max(0,Math.min(1,1-over/2));
    const rise=g.ramp/550*100/1000;tr.out=t>tr.out?Math.min(t,tr.out+rise):t;if(tr.out>tr.filt)tr.out=tr.filt;if(tr.out>raw)tr.out=raw;}
  return tr.out;}
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
  // Synthetic neutral (Build 4): Kelly 'Three Gears Switch: Forward, Neutral, Reverse' (manual Rev 2.1.1 item 90, SOURCED)
  // gives zero drive torque in N; how the Teensy makes N (FWD + REV pins both open) is UNVERIFIED. No mechanical
  // decoupling: the motor still turns with the wheels, so back-EMF and the overspeed guard below still apply.
  const ntl=!!inp.neutral,thr=ntl?0:Math.max(0,Math.min(100,inp.thr))/100,brake=Math.max(0,Math.min(100,inp.brake))/100;
  let T=0,why='';
  const brkSw=brake*100>(p.brkSwPct==null?0:p.brkSwPct);                 // 12 V brake switch: overrides throttle
  if(live&&thr>0&&!brkSw&&!inp.parkBrake){
    const tr=st.trim||{filt:thr*100,out:thr*100*g.cur/100};                  // no state passed: steady state
    // both Teensys dead: clamp-less pedal-direct fallback (docs/THROTTLE_CLAMP.md) = raw pedal, no trim/BMS scale
    const k100=inp.teensyDead?thr:vehTrim(g,tr,thr*100,st.mph,inp.allowPct,true,dt*1000)/100;st.trimOut=k100*100;
    T=p.ktNmA*p.phaseA*k100;                                                  // trim output (curve, cap, ramp, top, BMS allowance)
    if(k100<thr*g.cur/100*Math.max(0,inp.allowPct)/100-0.5e-2&&Math.abs(st.mph)>=g.top-2)why='mode top speed';
    const Pmax=p.eta*V*p.battA;if(w>1&&T*w>Pmax){T=Pmax/w;why='Kelly battery limit';}   // battery-amp limit
    const capRpm=inp.kellyLowCap?Math.min(g.rpm,p.lowCapRpm):g.rpm;
    if(inp.kellyLowCap&&rpm>=capRpm*0.95){T*=Math.max(0,(capRpm-rpm)/(capRpm*0.05));why='Kelly Low cap (ECO relay'+(inp.teensyDead?', Teensys dead':'')+')';}
    else if(rpm>=g.rpm*0.95){T*=Math.max(0,(g.rpm-rpm)/(g.rpm*0.05));why=inp.gear==='R'?'reverse RPM cap':'Kelly 6000 RPM cap';}
  }
  const driveA=T*w/(p.eta*V);
  // regen request: brake pedal (regen-first blend) or optional lift-off; faded at low speed; BMS can veto
  let regReq=0,rwhy='';
  let brkFrac=0;
  if(p.analogBrake){const lo=p.brkDeadLo/100,hi=p.brkDeadHi/100;brkFrac=Math.max(0,Math.min(1,(brake-lo)/Math.max(0.01,hi-lo)));
    if(brkFrac>0)regReq=p.maxRegenA*brkFrac*(p.brkAdPct/50);}                // proportional to pedal (Brake_AN)
  else if(brkSw)regReq=p.maxRegenA*(p.brkSwRegenPct/50);                    // DEFAULT: Kelly switch regen, fixed level (Brake Type 0)
  if(regReq===0&&!brkSw&&p.liftRegen&&thr===0)regReq=p.liftRegenA;          // Kelly RLS_TPS Brk (0-50 %, SOURCED)
  if(ntl){regReq=(p.ntlBrkPct||0)>0&&brkSw?p.maxRegenA*p.ntlBrkPct/50:0;if(!regReq)rwhy='Neutral: freewheel, no regen (Kelly NTL Brk 0 %, SOURCED default)';}
  regReq*=(g.regen==null?100:g.regen)/100;                                   // mode regen strength (PROPOSED)
  const allow=inp.regenAllowPct==null?100:Math.max(0,Math.min(100,inp.regenAllowPct));const regReq0=regReq;regReq*=allow/100;
  const fade=vehFade(p,st.mph);let regA=regReq*fade;
  if(regReq>0&&!live){regA=0;rwhy=inp.driveLocked?'drive locked':'contactors open';}
  else if(regReq>0&&!inp.regenOk){regA=0;rwhy='BMS: regen not allowed';}
  else if(regReq0>0&&allow<100)rwhy=allow>0?'BMS taper '+allow.toFixed(0)+' % (SOC / cold)':'BMS taper: 0 %';
  else if(p.analogBrake&&regReq>0&&rpm<p.brkMinRpm){regA=0;rwhy='below Kelly Brk_Speed Limit';}
  else if(regReq>0&&fade<1)rwhy=fade>0?'fading (low speed)':'below ~3 mph: friction only';
  // forces along travel (positive = forward along travel direction)
  const Fdrive=T*k/r;
  const Ftarget=brake*p.frictionG*m*G+(inp.parkBrake?p.parkG*m*G:0);         // park brake = rear friction (sim)
  // regen force from the amps request, capped by motor torque and (on the pedal) by the driver's brake demand;
  // the pack current is then what that force really returns (low speed = little current).
  let Fregen=v>0.05?Math.min(regA*V*p.eta/v,p.ktNmA*p.phaseA*k/r):0;
  regA=Fregen*v/(V*p.eta);
  const Ffric=Ftarget;                                                       // hydraulic friction always follows the pedal; regen adds
  const th=Math.atan((p.gradePct||0)/100),Fgrade=m*G*Math.sin(th)*dir;   // uphill opposes travel in the gear direction
  const Froll=p.crr*m*G*Math.cos(th),Faero=0.5*RHO*p.cdA*v*v;
  let s=dir*st.mph>=0?v:-v;                                              // speed along the gear direction
  const sgn=s>0?1:s<0?-1:0;
  let Fnet=Fdrive-Fgrade-sgn*(Froll+Faero+Ffric+Fregen);
  if(sgn===0){const resist=Froll+Ffric;if(Math.abs(Fdrive-Fgrade)<=resist)Fnet=0;else Fnet=(Fdrive-Fgrade)-Math.sign(Fdrive-Fgrade)*resist;}
  // Overspeed hold (PROPOSED, Kelly behaviour UNVERIFIED): with contactors closed the Kelly regen-brakes any travel
  // above its RPM cap (gear cap forward, reverse cap when rolling the other way), up to the phase-current torque.
  // Grade sign: + = nose uphill. Fgrade>0 opposes travel in the gear direction, so uphill + no throttle decelerates
  // and then rolls BACK; downhill accelerates until rolling + aero + drivetrain drag + regen balance it.
  const Fdt=p.dtDragN==null?60:p.dtDragN;                                   // ASSUMED drivetrain drag (chain, diffs, motor iron loss)
  const capFwd=(inp.kellyLowCap?Math.min(g.rpm,p.lowCapRpm):g.rpm),capBack=(VEH_GEARS.R||{rpm:1100}).rpm;
  const capOver=sgn>=0?capFwd:Math.min(capBack,capFwd);let Fover=0,overWhy='';
  if(sgn!==0&&rpm>capOver*0.95){
    const f=Math.min(1,(rpm-capOver*0.95)/(capOver*0.05)),Fmax=p.ktNmA*p.phaseA*k/r;
    if(live&&inp.regenOk!==false){Fover=f*Fmax;overWhy='Kelly overspeed regen ('+capOver+' RPM cap)';}
  }
  Fnet-=sgn*(Fdt+Fover);if(sgn===0&&Fnet!==0){const a0=Math.abs(Fnet);Fnet=a0<=Fdt?0:Fnet-Math.sign(Fnet)*Fdt;}
  let s1=s+Fnet/m*dt;
  if(sgn!==0&&Math.sign(s1)!==sgn&&Fover>0)s1=0;
  const vMax=vehMphAtRpm(p,6000)*MPS_PER_MPH;                                // hard guard: never above the 6000 RPM motor/Kelly cap
  if(Math.abs(s1)>vMax)s1=Math.sign(s1)*vMax;
  if(Fover>0){const add=Fover*v/(V*p.eta);regA+=add;if(!rwhy)rwhy=overWhy;}
  if(sgn!==0&&Math.sign(s1)!==sgn&&(brake>0||inp.parkBrake||Fdrive===0&&Math.abs(Fgrade)<=Froll))s1=0;  // brakes/rolling stop, no reversal
  const mph=dir*s1/MPS_PER_MPH;
  const packA=live?driveA-regA:0;if(overWhy&&!why)why=overWhy;
  const bemfWarn=ntl&&rpm>(V/0.026)?'Neutral but motor spinning above base speed: back-EMF ≈ '+Math.round(rpm*0.026)+' V vs pack '+Math.round(V)+' V (0.026 V/RPM SOURCED Sparky)':'';
  return {neutral:ntl,bemfWarn,Fover,mph,rpm:vehRpm(p,mph),packA,brakeSwitch:brkSw,brakeFrac:brkFrac,driveA,regenA:regA,regenReqA:regReq,fade,Fdrive,Fregen,Ffric,Froll,Faero,Fgrade,why,regenWhy:rwhy};
}
