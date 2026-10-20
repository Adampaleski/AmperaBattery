/* Teryx dash component — shared by tools/dash.html (in-vehicle) and the sim Cluster tab.
   Input: one status object (STATUS_SCHEMA v1 + additive fields, see docs/DASH.md).
   Rule: a reading with no source is shown as "—" with an amber dot. Never invents values. */
(function(g){
const CSS=`
.tdash{--bg:#000;--tx:#f5f5f7;--mut:#a1a1a8;--dim:#5a5a60;--amb:#d6a33a;--red:#e82127;--grn:#30d158;--blu:#3e6ae1;--card:#0e0e10;
 position:relative;background:var(--bg);color:var(--tx);font:18px/1.3 system-ui,-apple-system,"Inter","Segoe UI",Roboto,sans-serif;-webkit-font-smoothing:antialiased;
 border-radius:18px;padding:18px 26px 20px;overflow:hidden;user-select:none;aspect-ratio:16/7;min-height:420px;display:flex;flex-direction:column;gap:12px}
.tdash.full{border-radius:0;aspect-ratio:auto;height:100vh;min-height:0;padding:1.5vh 2vw}
.tdash.full .xl{font-size:min(150px,24vh)}.tdash.full .lg{font-size:min(64px,11vh)}.tdash.full .md{font-size:min(34px,6vh)}.tdash.full .ana svg{max-height:42vh}.tdash.full .cell .v{font-size:min(24px,4.2vh)}.tdash.full .main{gap:1vh 20px}.tdash.full .col{gap:1.2vh}
.tdash.night{--tx:#c9c9cf;--mut:#76767c;--dim:#3a3a3e;--card:#060607;filter:brightness(.78)}
.tdash .top{display:flex;align-items:center;gap:10px;min-height:40px}
.tdash .tt{display:flex;gap:10px;flex:1;flex-wrap:wrap}.tdash .tt i{width:34px;height:34px;display:inline-grid;place-items:center;opacity:.16;transition:opacity .2s}
.tdash .tt i.on{opacity:1}.tdash .tt svg{width:30px;height:30px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.tdash .tt i.r{color:var(--red)}.tdash .tt i.a{color:var(--amb)}.tdash .tt i.g{color:var(--grn)}.tdash .tt i.b{color:var(--blu)}.tdash .tt i.w{color:var(--tx)}
.tdash .tbtn{background:none;border:0;color:var(--mut);font:inherit;font-size:16px;padding:6px 10px;border-radius:8px;cursor:pointer;min-height:40px}.tdash .tbtn:hover{background:#1c1c1e}
.tdash .srcb{font-size:14px;letter-spacing:.08em;text-transform:uppercase;color:var(--amb);border:1px solid var(--amb);border-radius:6px;padding:2px 8px}
.tdash .ban{display:none;background:var(--red);color:#fff;border-radius:12px;padding:10px 16px;align-items:center;gap:14px;font-weight:500}.tdash .ban.on{display:flex}
.tdash .ban span{flex:1}.tdash .ban button{min-height:44px;border:0;border-radius:10px;background:#fff;color:#000;font:inherit;font-weight:600;padding:0 18px;cursor:pointer}
.tdash .main{flex:1;display:grid;grid-template-columns:1fr 1.25fr 1fr;gap:20px;align-items:center;min-height:0}
.tdash .col{display:flex;flex-direction:column;gap:14px;min-width:0}
.tdash .k{font-size:15px;color:var(--mut);letter-spacing:.04em}
.tdash .v{font-weight:300;font-variant-numeric:tabular-nums;letter-spacing:-.02em;line-height:1}
.tdash .u{font-size:.42em;color:var(--mut);margin-left:4px;letter-spacing:0}
.tdash .xl{font-size:150px;font-weight:200}.tdash .lg{font-size:64px}.tdash .md{font-size:34px}.tdash .sm{font-size:24px}
.tdash .na{color:var(--dim)}.tdash .na::after{content:"";display:inline-block;width:10px;height:10px;border-radius:50%;background:var(--amb);margin-left:8px;vertical-align:super}
.tdash .tag{font-size:12px;color:var(--amb);letter-spacing:.06em;text-transform:uppercase;margin-left:6px;vertical-align:middle}
.tdash .ctr{text-align:center}
.tdash .gear{display:flex;gap:18px;justify-content:center;font-size:30px;font-weight:500;color:var(--dim)}.tdash .gear .on{color:var(--tx)}
.tdash .pbar{position:relative;height:12px;border-radius:6px;background:#1c1c1e;margin:16px 0 6px}
.tdash .pbar i{position:absolute;top:0;bottom:0;border-radius:6px;transition:left .15s,width .15s}.tdash .pbar b{position:absolute;left:20%;top:-5px;bottom:-5px;width:2px;background:var(--mut)}
.tdash .pbl{display:flex;justify-content:space-between;font-size:13px;color:var(--mut)}
.tdash .batt{height:10px;border-radius:5px;background:#1c1c1e;overflow:hidden;margin-top:8px}.tdash .batt i{display:block;height:100%;background:var(--tx);transition:width .3s}
.tdash .bot{display:grid;grid-template-columns:repeat(7,1fr);gap:10px}
.tdash .cell{background:var(--card);border-radius:12px;padding:10px 12px;min-width:0}.tdash .cell .v{font-size:24px;margin-top:4px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.tdash .trip{grid-column:span 2;display:grid;grid-template-columns:repeat(3,1fr);gap:6px}.tdash .trip .k{grid-column:1/-1}
.tdash .seats{display:flex;align-items:center;gap:16px}.tdash .seats .sp{flex:1;text-align:center;font-size:14px;color:var(--mut)}
.tdash .seatb{display:flex;align-items:center;gap:8px;background:var(--card);border:0;border-radius:14px;padding:6px 14px;min-height:56px;color:var(--tx);font:inherit;font-size:15px;cursor:pointer;touch-action:manipulation}
.tdash .seatb svg{width:46px;height:46px;fill:none;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}.tdash .seatb .st{stroke:var(--tx)}.tdash .seatb .wv{stroke:#3a3a3e;transition:stroke .15s}.tdash .seatb .wv.on{stroke:var(--red)}
.tdash .seatb .lv{min-width:38px;text-align:left;color:var(--mut)}
.tdash.minimal .seats{display:none}.tdash.full .seatb{min-height:44px;padding:2px 12px}.tdash.full .seatb svg{width:38px;height:38px}.tdash.full .bot{gap:8px}.tdash.full .cell{padding:6px 10px}
.tdash .ana{display:none}.tdash.analog .ana{display:block}.tdash.analog .dig{display:none}
.tdash .ana svg{width:100%;max-height:300px}.tdash .ana .tick{stroke:var(--mut);stroke-width:2}.tdash .ana .lbl{fill:var(--mut);font-size:16px;text-anchor:middle}
.tdash .ana .ndl{stroke:var(--red);stroke-width:4;stroke-linecap:round;transition:transform .15s}.tdash .ana .hub{fill:var(--tx)}
.tdash .ana .rd{fill:var(--tx);font-size:54px;font-weight:200;text-anchor:middle}.tdash .ana .ru{fill:var(--mut);font-size:16px;text-anchor:middle}
.tdash.minimal .bot,.tdash.minimal .hide-min{display:none}.tdash.minimal .xl{font-size:190px}
.tdash .menu{position:absolute;right:20px;top:58px;background:#1c1c1e;border-radius:14px;padding:12px;display:none;z-index:3;width:330px;box-shadow:0 10px 40px #000}
.tdash .menu.on{display:block}.tdash .menu .row{display:flex;gap:6px;flex-wrap:wrap;margin:6px 0 12px}
.tdash .menu button{min-height:40px;border:0;border-radius:8px;background:#2c2c2e;color:var(--tx);font:inherit;font-size:15px;padding:0 12px;cursor:pointer}.tdash .menu button.on{background:var(--tx);color:#000}
.tdash .menu input{width:100%;min-height:40px;background:#2c2c2e;border:0;border-radius:8px;color:var(--tx);font:inherit;font-size:15px;padding:0 10px}
@media(max-width:900px){.tdash{aspect-ratio:auto}.tdash .main{grid-template-columns:1fr}.tdash .bot{grid-template-columns:repeat(2,1fr)}.tdash .xl{font-size:110px}}
`;
const IC={ // simple line icons
 fault:'<path d="M12 3 2 21h20L12 3z"/><path d="M12 10v5M12 18v.5"/>',
 batt:'<rect x="3" y="7" width="16" height="11" rx="2"/><path d="M21 11v3M7 12h3M15 12h0M14 10.5v3"/>',
 temp:'<path d="M12 3a2 2 0 0 0-2 2v9a4 4 0 1 0 4 0V5a2 2 0 0 0-2-2z"/><path d="M12 9v7"/>',
 limp:'<path d="M3 17h18M5 17c0-5 3-8 7-8s7 3 7 8M9 9l-1-3M15 9l1-3"/>',
 regen:'<path d="M4 12a8 8 0 0 1 14-5l2-2v6h-6l2-2a5 5 0 1 0 1 5"/>',
 hv:'<path d="M13 2 4 14h7l-1 8 9-12h-7l1-8z"/>',
 link:'<path d="M9 15l6-6M7 11l-2 2a3 3 0 0 0 4 4l2-2M17 13l2-2a3 3 0 0 0-4-4l-2 2"/><path d="M3 3l18 18"/>',
 imb:'<path d="M12 3v18M5 7h14M5 7l-3 7h6zM19 7l-3 7h6z"/>',
 lights:'<path d="M10 6c5 0 8 2.5 8 6s-3 6-8 6c-1.5 0-2-2.7-2-6s.5-6 2-6z"/><path d="M2 8h4M2 12h4M2 16h4"/>',
 fan:'<circle cx="12" cy="12" r="2"/><path d="M12 10c0-4 1-7 4-7s2 5-2 7M14 12c4 0 7 1 7 4s-5 2-7-2M12 14c0 4-1 7-4 7s-2-5 2-7M10 12c-4 0-7-1-7-4s5-2 7 2"/>',
 seat:'<path d="M7 3v10h9l2 8M7 13l-2 8M9 8h6"/><path d="M14 2c1 1-1 2 0 3M17 2c1 1-1 2 0 3"/>',
 pump:'<circle cx="12" cy="12" r="8"/><path d="M12 4v4M8 12h8M12 12l3 4"/>',
 coolant:'<path d="M12 3s-5 6-5 10a5 5 0 0 0 10 0c0-4-5-10-5-10z"/><path d="M4 21c2-1 4 1 6 0s4 1 6 0 4 1 4 0"/>',
 alarm:'<path d="M6 16V11a6 6 0 0 1 12 0v5l2 2H4z"/><path d="M10 21h4"/>'};
const TT=[['fault','r','BMS fault'],['hv','a','HV contactors open'],['batt','a','Low cell'],['temp','r','Pack hot'],['limp','a','Limp / derate'],['regen','a','Regen off'],['imb','a','LEM imbalance'],['link','a','No data'],['lights','g','Lights'],['fan','b','Cooling fan'],['seat','a','Seat heater'],['pump','b','Kelly coolant pump on'],['coolant','r','Coolant warning (no live source yet)'],['alarm','r','Alarm']];
const STY=['digital','analog','minimal','night'];
const ASSUMED_WHMI=350;           // PROPOSED fallback, docs/DASH.md
const ASSUMED_PACK_KWH=36*3.6*180/1000; // SIM-ASSUMED 180 Ah (4 × 45 Ah) × 36 × 3.6 V
const RPM_PER_MPH=6000/55;        // ESTIMATE from plan doc: ~6000 RPM ≈ 55 mph on 27" tires
const N_MI=5;                     // rolling window for "last N mi" average
const num=x=>typeof x==='number'&&isFinite(x);
function arc(cx,cy,r,a0,a1){const p=a=>[cx+r*Math.cos(a),cy+r*Math.sin(a)];const[x0,y0]=p(a0),[x1,y1]=p(a1);return `M${x0} ${y0} A${r} ${r} 0 ${a1-a0>Math.PI?1:0} 1 ${x1} ${y1}`;}
function dial(id,min,max,step,unit,lab,zero){ // 240° sweep
  const A0=Math.PI*5/6,A1=Math.PI*13/6,ang=v=>A0+(v-min)/(max-min)*(A1-A0);let s=`<svg viewBox="0 0 300 260"><path d="${arc(150,150,120,A0,A1)}" stroke="#2c2c2e" stroke-width="10" fill="none"/>`;
  if(zero!=null)s+=`<path id="${id}rg" d="${arc(150,150,120,A0,ang(zero))}" stroke="#30d158" stroke-width="10" fill="none" opacity=".7"/>`;
  for(let v=min;v<=max+1e-9;v+=step){const a=ang(v),c=Math.cos(a),n=Math.sin(a);s+=`<line class="tick" x1="${150+108*c}" y1="${150+108*n}" x2="${150+98*c}" y2="${150+98*n}"/><text class="lbl" x="${150+80*c}" y="${156+80*n}">${v}</text>`;}
  s+=`<line id="${id}n" class="ndl" x1="150" y1="150" x2="150" y2="44" style="transform-origin:150px 150px"/><circle class="hub" cx="150" cy="150" r="7"/><text id="${id}r" class="rd" x="150" y="215">—</text><text class="ru" x="150" y="240">${unit}</text></svg>`;
  return {svg:s,deg:v=>(ang(Math.max(min,Math.min(max,v)))+Math.PI/2)*180/Math.PI};
}
function create(root,opt){
  opt=opt||{};if(!document.getElementById('tdash-css')){const st=document.createElement('style');st.id='tdash-css';st.textContent=CSS;document.head.appendChild(st);}
  const spd=dial('ds',0,60,10,'mph'),pw=dial('dp',-20,100,20,'kW',0,0),bt=dial('db',0,100,25,'% battery');
  root.innerHTML=`<div class="tdash${opt.full?' full':''}">
   <div class="top"><div class="tt">${TT.map(([k,c,t])=>`<i data-k="${k}" class="${c}" title="${t}"><svg viewBox="0 0 24 24">${IC[k]}</svg></i>`).join('')}</div>
    <span class="srcb" data-f="srcb"></span><button class="tbtn" data-f="mbtn">Style ▾</button></div>
   <div class="ban" data-f="ban"><span data-f="banT"></span><button data-f="ack">Acknowledge</button></div>
   <div class="main">
    <div class="col">
      <div class="dig"><div class="k">Power <span class="tag" data-f="pwTag"></span></div><div class="v lg" data-f="kw">—</div>
        <div class="pbar"><b></b><i data-f="pbar"></i></div><div class="pbl"><span>regen</span><span>0</span><span>power →</span></div></div>
      <div class="ana">${pw.svg}</div>
      <div class="hide-min"><div class="k">Now</div><div class="v md" data-f="inst">—</div></div>
      <div class="hide-min"><div class="k">RPM <span class="tag" data-f="rpmTag"></span></div><div class="v sm" data-f="rpm">—</div></div>
    </div>
    <div class="col ctr">
      <div class="dig"><div class="v xl" data-f="spd">—</div><div class="k" style="margin-top:4px">mph <span class="tag" data-f="spdTag"></span></div></div>
      <div class="ana">${spd.svg}</div>
      <div class="gear" data-f="gear"></div>
    </div>
    <div class="col" style="text-align:right">
      <div class="dig"><div class="k">Range <span class="tag" data-f="rgTag"></span></div><div class="v lg" data-f="rng">—</div>
        <div class="k" style="margin-top:12px">Battery <span class="tag" data-f="socTag"></span></div><div class="v md" data-f="soc">—</div><div class="batt"><i data-f="sbar" style="width:0"></i></div></div>
      <div class="ana">${bt.svg}<div class="k">Range <span class="tag" data-f="rgTag2"></span></div><div class="v md" data-f="rng2">—</div></div>
    </div>
   </div>
   <div class="bot">
    <div class="cell trip"><div class="k">Trip <button class="tbtn" data-f="trst" style="min-height:0;padding:0 6px;font-size:13px">reset</button></div>
      <div><div class="k">mi</div><div class="v" data-f="tmi">—</div></div><div><div class="k">kWh used</div><div class="v" data-f="tkwh">—</div></div><div><div class="k">kWh regen</div><div class="v" data-f="treg" style="color:var(--grn)">—</div></div></div>
    <div class="cell"><div class="k">Avg trip · last ${N_MI} mi</div><div class="v" data-f="avg">—</div></div>
    <div class="cell"><div class="k">Pack</div><div class="v" data-f="pack">—</div></div>
    <div class="cell"><div class="k">Cell min / max</div><div class="v" data-f="cell">—</div></div>
    <div class="cell"><div class="k">Pack · Kelly · motor °C</div><div class="v" data-f="tmp">—</div></div>
    <div class="cell"><div class="k">Coolant pump <span class="tag">proposed</span></div><div class="v" data-f="pump">—</div></div>
   </div>
   <div class="seats">${['driver','passenger'].map(k=>`<button class="seatb" data-seat="${k}" title="Tap: Off→Low→Med→High · hold: off"><svg viewBox="0 0 40 40"><path class="st" d="M8 8v16h12l3 10M8 24l-3 10M11 16h7"/><path class="wv" data-w="1" d="M25 3c3 3-3 5 0 8s-3 5 0 8"/><path class="wv" data-w="2" d="M30 3c3 3-3 5 0 8s-3 5 0 8"/><path class="wv" data-w="3" d="M35 3c3 3-3 5 0 8s-3 5 0 8"/></svg><span><span class="k">${k==='driver'?'Driver':'Passenger'}</span><br><span class="lv" data-f="lv_${k}">Off</span></span></button>`).join('<span class="sp" data-f="l12"></span>')}</div>
   <div class="menu" data-f="menu"><div class="k">Style</div><div class="row" data-f="sty">${STY.map(s=>`<button data-s="${s}">${s[0].toUpperCase()+s.slice(1)}</button>`).join('')}</div>
    ${opt.sourceUI?`<div class="k">Data source</div><div class="row" data-f="srcs"><button data-src="sim">Simulator</button><button data-src="http">HTTP poll</button><button data-src="ws">WebSocket</button><button data-src="none">None</button></div>
    <input data-f="url" placeholder="http://teryx-hub.local/api/status or ws://…"><div class="row"><button data-f="apply">Apply</button></div>`:''}
   </div></div>`;
  const D=root.firstElementChild,F=n=>D.querySelector(`[data-f="${n}"]`);
  let style=localStorage.getItem('teryxDashStyle')||'digital',ack=null,lastT=0,trip=JSON.parse(localStorage.getItem('teryxDashTrip')||'null')||{mi:0,kwh:0,reg:0},win=[];
  const setStyle=s=>{style=s;STY.forEach(x=>D.classList.toggle(x,x===s));D.querySelectorAll('[data-s]').forEach(b=>b.classList.toggle('on',b.dataset.s===s));localStorage.setItem('teryxDashStyle',s);};
  setStyle(STY.includes(style)?style:'digital');
  F('mbtn').onclick=()=>F('menu').classList.toggle('on');
  D.querySelectorAll('[data-s]').forEach(b=>b.onclick=()=>setStyle(b.dataset.s));
  F('trst').onclick=()=>{trip={mi:0,kwh:0,reg:0};win=[];localStorage.setItem('teryxDashTrip',JSON.stringify(trip));};
  F('ack').onclick=()=>{ack=curFaultKey;F('ban').classList.remove('on');};
  let curFaultKey='',seat={driver:0,passenger:0},seatSrc=false;const LV=['Off','Low','Med','High'];
  const seatShow=()=>D.querySelectorAll('[data-seat]').forEach(b=>{const k=b.dataset.seat,l=seat[k];b.querySelectorAll('.wv').forEach(w=>w.classList.toggle('on',+w.dataset.w<=l));F('lv_'+k).textContent=LV[l]+(l?' '+[0,33,66,100][l]+'%':'');});
  D.querySelectorAll('[data-seat]').forEach(b=>{let tm=null,held=false;const k=b.dataset.seat;
    const set=l=>{seat[k]=l;seatShow();if(opt.onSeat)opt.onSeat(k,l);};
    b.onpointerdown=()=>{held=false;tm=setTimeout(()=>{held=true;set(0);},600);};
    b.onpointerup=()=>{clearTimeout(tm);if(!held)set((seat[k]+1)%4);};b.onpointerleave=()=>clearTimeout(tm);});
  seatShow();
  const put=(n,val,txt)=>{const e=F(n);if(!e)return;if(val==null){e.textContent='—';e.classList.add('na');}else{e.innerHTML=txt;e.classList.remove('na');}};
  const tag=(n,t)=>{const e=F(n);if(e)e.textContent=t||'';};
  function update(s,meta){
    meta=meta||{};const now=performance.now(),dt=lastT?Math.min(2,(now-lastT)/1000):0;lastT=now;
    const live=!!s&&!meta.stale,B=(s&&s.bms)||{},Dr=(s&&s.drive)||{},E=(s&&s.energy)||{},C=(s&&s.contactors)||{},A=(s&&s.aux12v)||{},L=A.loads||{};
    const packV=live&&B.wired&&num(B.packV)?B.packV:null,packA=live&&B.wired&&num(B.packA)?B.packA:null;
    const kw=packV!=null&&packA!=null?packV*packA/1000:null;
    const mph=live&&Dr.wired!==false&&num(Dr.speedMph)?Math.abs(Dr.speedMph):null;
    F('srcb').textContent=!s?'no data source':meta.stale?'data stale':(s.src==='sim'?'simulator data':'live');
    // power
    put('kw',kw,kw!=null?`${kw.toFixed(0)}<span class="u">kW</span>`:'');
    const pb=F('pbar');if(kw==null){pb.style.width=0;}else if(kw>=0){pb.style.left='20%';pb.style.width=Math.min(80,kw/100*80)+'%';pb.style.background='var(--tx)';}else{const w=Math.min(20,-kw/20*20);pb.style.left=(20-w)+'%';pb.style.width=w+'%';pb.style.background='var(--grn)';}
    tag('pwTag',packA!=null&&B.currentVerified===false?'LEM uncalibrated':'');
    // trip integration (only from real numbers)
    if(dt>0&&kw!=null){const e=kw*dt/3600;if(e>=0)trip.kwh+=e;else trip.reg+=-e;}
    if(dt>0&&mph!=null){const d=mph*dt/3600;trip.mi+=d;win.push([d,kw!=null?kw*dt/3600:0]);let m=win.reduce((a,x)=>a+x[0],0);while(win.length&&m-win[0][0]>N_MI){m-=win[0][0];win.shift();}}
    if(dt>0&&Math.random()<.05)localStorage.setItem('teryxDashTrip',JSON.stringify(trip));
    const inst=kw!=null&&mph!=null&&mph>2?kw*1000/mph:null;
    put('inst',inst,inst!=null?`${inst.toFixed(0)}<span class="u">Wh/mi</span>`:'');
    const tripAvg=trip.mi>0.2?(trip.kwh-trip.reg)*1000/trip.mi:null;
    const wm=win.reduce((a,x)=>a+x[0],0),we=win.reduce((a,x)=>a+x[1],0),lastAvg=wm>0.2?we*1000/wm:null;
    put('avg',tripAvg??lastAvg,`${tripAvg!=null?tripAvg.toFixed(0):'—'} · ${lastAvg!=null?lastAvg.toFixed(0):'—'}<span class="u">Wh/mi</span>`);
    put('tmi',(live||trip.mi)?1:null,trip.mi.toFixed(1));put('tkwh',1,trip.kwh.toFixed(2));put('treg',1,trip.reg.toFixed(2));
    // speed / rpm
    put('spd',mph,mph!=null?mph.toFixed(0):'');tag('spdTag',mph!=null&&Dr.speedSource&&Dr.speedSource!=='kelly'?Dr.speedSource:'');
    let rpm=live&&Dr.wired&&num(Dr.rpm)?Dr.rpm:null,rt='';if(rpm==null&&mph!=null){rpm=mph*RPM_PER_MPH;rt='estimate from speed';}
    put('rpm',rpm,rpm!=null?rpm.toFixed(0):'');tag('rpmTag',rt);
    // gear
    const gr=live?(Dr.gear||null):null,closed=C.mainPositive==='closed'&&C.mainNegative==='closed';
    F('gear').innerHTML=['P','R','L','M','H'].map(x=>`<span class="${gr===x||(x==='P'&&live&&!closed)?'on':''}">${x}</span>`).join('')+(gr==null&&live?'<span class="na" style="font-size:16px"> gear not wired</span>':'');
    // battery / range
    const soc=live&&B.wired&&num(B.socPct)?B.socPct:null;put('soc',soc,soc!=null?`${soc.toFixed(0)}<span class="u">%</span>`:'');F('sbar').style.width=(soc||0)+'%';
    tag('socTag',soc!=null&&B.socState?B.socState:(live&&soc==null?(B.socWhy||'unknown'):''));
    let rng=live&&num(E.rangeMiEst)?E.rangeMiEst:null,rgt=rng!=null&&!E.calibrated?'estimate':'';
    if(rng==null&&soc!=null){const pk=num(E.packKWh)?E.packKWh:ASSUMED_PACK_KWH,wh=lastAvg!=null&&lastAvg>50?lastAvg:(tripAvg!=null&&tripAvg>50?tripAvg:null);
      rng=soc/100*pk*1000/(wh||ASSUMED_WHMI);rgt=(wh?`recent ${wh.toFixed(0)} Wh/mi`:`ASSUMED ${ASSUMED_WHMI} Wh/mi`)+(num(E.packKWh)?'':` · ASSUMED ${pk.toFixed(1)} kWh`);}
    put('rng',rng,rng!=null?`${rng.toFixed(0)}<span class="u">mi</span>`:'');put('rng2',rng,rng!=null?`${rng.toFixed(0)}<span class="u">mi</span>`:'');tag('rgTag',rgt);tag('rgTag2',rgt);
    // bottom
    put('pack',packV??packA,`${packV!=null?packV.toFixed(0):'—'}<span class="u">V</span> ${packA!=null?packA.toFixed(0):'—'}<span class="u">A</span>`);
    const cmn=live&&B.wired&&num(B.cellMinV)?B.cellMinV:null,cmx=live&&B.wired&&num(B.cellMaxV)?B.cellMaxV:null;
    put('cell',cmn,`${cmn!=null?cmn.toFixed(3):'—'} / ${cmx!=null?cmx.toFixed(3):'—'}`);
    const tP=live&&num(B.tempMaxC)&&B.tempsVerified!==false?B.tempMaxC:null,tK=live&&Dr.wired&&num(Dr.controllerTempC)?Dr.controllerTempC:null,tM=live&&Dr.wired&&num(Dr.motorTempC)?Dr.motorTempC:null;
    put('tmp',tP??tK??tM,[tP,tK,tM].map(x=>x!=null?x.toFixed(0):'—').join(' · '));
    const Co=(s&&s.coolant)||{};put('pump',live&&(L.pump!=null||Co.pumpOn!=null)?1:null,(Co.pumpOn??L.pump)?'ON':'off'+(Co.wired?'':' <span class="tag">cmd only · no flow sensor</span>'));
    if(live&&L.seatDriver!=null){seat.driver=L.seatDriver|0;seat.passenger=(L.seatPassenger|0);seatShow();}
    F('l12').innerHTML=live&&num(A.loadA)?`12 V loads ≈ ${A.loadA.toFixed(1)} A <span class="tag">${A.loadAEstimate?'estimate':''}</span><br><span class="tag">seat PWM 33/66/100 % PROPOSED · kit stages UNVERIFIED</span>`:`<span class="na">12 V load</span><br><span class="tag">seat buttons: ${opt.onSeat?'sim':'display only, no output wired (PROPOSED)'}</span>`;
    // analog needles
    const nd=(id,dl,v)=>{const n=D.querySelector('#'+id+'n');n.style.transform=`rotate(${dl.deg(v==null?-1e9:v)}deg)`;n.style.opacity=v==null?.25:1;D.querySelector('#'+id+'r').textContent=v==null?'—':Math.round(v);};
    nd('ds',spd,mph);nd('dp',pw,kw);nd('db',bt,soc);
    // tell-tales
    const fl=(s&&s.faults)||[],faultTxt=fl.filter(f=>f.level==='fault').map(f=>f.text);
    const on={fault:live&&faultTxt.length>0,hv:live&&C.wired!==false&&!closed,batt:cmn!=null&&cmn<3.2,temp:tP!=null&&tP>=50,limp:live&&(!!Dr.limp||(num(Dr.powerAllowedPct)&&Dr.powerAllowedPct<100)),
      regen:live&&Dr.regenOk===false&&closed,imb:live&&!!B.lemImbalance,link:!live,lights:!!L.lights,fan:!!L.fan,seat:!!L.seatHeat,pump:!!L.pump,coolant:live&&((s.coolant||{}).flowOk===false||num((s.coolant||{}).tempC)&&s.coolant.tempC>=60),alarm:!!L.alarm||(live&&faultTxt.length>0)};
    D.querySelectorAll('.tt i').forEach(i=>i.classList.toggle('on',!!on[i.dataset.k]));
    curFaultKey=faultTxt.join('|');if(!curFaultKey)ack=null;
    F('ban').classList.toggle('on',!!curFaultKey&&ack!==curFaultKey);F('banT').textContent='⚠ '+faultTxt.join(' · ');
  }
  update(null);
  return {update,root:D,setStyle,F};
}
g.TeryxDash={create,ASSUMED_WHMI,ASSUMED_PACK_KWH,RPM_PER_MPH};
})(window);
