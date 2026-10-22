// Drive modes = PROPOSED pedal-map profiles (sim/vehicle.js VEH_GEARS, include/DriveProfile.h)
const MODE_NAME={H:'High',M:'Medium',L:'Low',SNOW:'Snow',MUD:'Mud',SAND:'Sand',KID:'Kid',R:'Reverse',P:'Park'};
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
.tdash.night{--tx:#c9c9cf;--mut:#76767c;--dim:#3a3a3e;--card:#060607}
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
.tdash .chg{display:none;flex:1;min-height:0;grid-template-columns:1.1fr 1fr;gap:18px 28px;align-items:center}.tdash.charging .chg{display:grid}
.tdash.charging .main,.tdash.charging .bot,.tdash.charging .seats{display:none}
.tdash .cbat{position:relative;height:120px;border:3px solid #3a3a3e;border-radius:22px;overflow:hidden;background:#0b0b0c}.tdash .cbat::after{content:"";position:absolute;right:-14px;top:38px;width:10px;height:44px;border-radius:0 6px 6px 0;background:#3a3a3e}
.tdash .cbat i{position:absolute;left:0;top:0;bottom:0;background:linear-gradient(90deg,#1f8f3a,#30d158);transition:width .6s}
.tdash .cbat i::after{content:"";position:absolute;inset:0;background:linear-gradient(90deg,transparent 0,rgba(255,255,255,.55) 50%,transparent 100%);background-size:40% 100%;background-repeat:no-repeat;animation:tdpulse 2.2s linear infinite}
.tdash .cbat.idle i::after{animation:none;opacity:0}.tdash .cbat b{position:absolute;top:-6px;bottom:-6px;width:3px;background:var(--tx);opacity:.8}
@keyframes tdpulse{0%{background-position:-60% 0}100%{background-position:160% 0}}
.tdash .csoc{font-size:110px;font-weight:200;line-height:1}.tdash .cst{font-size:28px;margin-top:6px}.tdash .cst.ok{color:var(--grn)}.tdash .cst.bad{color:var(--red)}.tdash .cst.wait{color:var(--amb)}
.tdash .cgrid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}.tdash .cgrid .cell .v{font-size:26px}
.tdash .cctl{display:flex;flex-wrap:wrap;gap:12px;align-items:center;grid-column:1/-1}.tdash .cctl input[type=range]{flex:1;min-width:200px;accent-color:#30d158}
.tdash .cctl button,.tdash .cctl input[type=number]{min-height:48px;border:0;border-radius:12px;background:#2c2c2e;color:var(--tx);font:inherit;padding:0 16px}.tdash .cctl input[type=number]{width:90px}
.tdash .cctl button.go{background:var(--grn);color:#000;font-weight:600}.tdash .cctl button.stop{background:var(--red);color:#fff;font-weight:600}
.tdash .slp{display:none;position:absolute;inset:0;background:#000;z-index:5;place-items:center;cursor:pointer}.tdash.asleep .slp{display:grid}
.tdash .slp svg{width:min(260px,40vh);height:min(260px,40vh)}.tdash .slp .rg{stroke:#30d158;filter:drop-shadow(0 0 10px #30d158);animation:tdglow 4s ease-in-out infinite}
@keyframes tdglow{0%,100%{opacity:.35}50%{opacity:.8}}
.tdash .ctl{display:none;position:absolute;inset:0;background:var(--bg);z-index:4;padding:18px 26px;overflow:auto;flex-direction:column;gap:14px}.tdash.ctlon .ctl{display:flex}
.tdash .ctl h3{margin:0;font-weight:500;font-size:22px;display:flex;align-items:center;gap:12px}.tdash .ctl h3 span{flex:1}
.tdash .cgr{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px}
.tdash .ctb{min-height:64px;border:0;border-radius:14px;background:#1c1c1e;color:var(--tx);font:inherit;font-size:16px;cursor:pointer;padding:6px 10px;touch-action:manipulation}
.tdash .ctb.on{background:var(--tx);color:#000;font-weight:600}.tdash .ctb.amb.on{background:var(--amb)}.tdash .ctb.blu.on{background:var(--blu);color:#fff}
.tdash .ctb.dis{opacity:.38}.tdash .ctb small{display:block;font-size:12px;color:var(--mut);margin-top:2px;font-weight:400}.tdash .ctb.on small{color:#333}
.tdash .ctb.auto{cursor:default}.tdash .cmsg{min-height:28px;color:var(--amb);font-size:18px}
.tdash .cset{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:8px 18px;font-size:14px;color:var(--mut)}.tdash .cset label{display:flex;gap:8px;align-items:center}.tdash .cset input[type=number]{width:70px;background:#2c2c2e;color:var(--tx);border:0;border-radius:8px;padding:6px}
.tdash .tt i.blink.on{animation:none}
@media(max-width:900px){.tdash{aspect-ratio:auto}.tdash .main{grid-template-columns:1fr}.tdash .bot{grid-template-columns:repeat(2,1fr)}.tdash .xl{font-size:110px}}
.tdash .stage{position:relative;flex:1;min-height:0;display:flex;flex-direction:column;gap:12px}
.tdash .home{flex:1;display:grid;grid-template-columns:1.3fr 1fr;gap:18px;min-height:0}
.tdash .clu{background:var(--card);border-radius:18px;padding:16px 22px;display:flex;flex-direction:column;justify-content:space-between;gap:10px;min-width:0}
.tdash .crow{display:grid;grid-template-columns:repeat(3,1fr);gap:12px}.tdash .crow>div{min-width:0}
.tdash .spdw{display:flex;align-items:flex-end;gap:16px}.tdash .spdw .xl{min-width:3.2ch;text-align:right}
.tdash .ana .dials{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.tdash .info{display:flex;flex-direction:column;gap:12px;min-width:0;min-height:0}
.tdash .lim{background:var(--card);border-radius:14px;padding:12px 16px;font-size:20px}.tdash .lim b{font-size:30px;font-weight:300;font-variant-numeric:tabular-nums}
.tdash .lim.cut{box-shadow:inset 4px 0 0 var(--amb)}.tdash .lim.off{box-shadow:inset 4px 0 0 var(--red)}
.tdash .info .bot{grid-template-columns:repeat(2,1fr)}.tdash .info .trip{grid-column:span 2}
.tdash .dock{display:flex;gap:10px;justify-content:space-between}
.tdash .qb,.tdash .dock .seatb{flex:1;min-width:0;min-height:76px;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:2px;border:0;border-radius:16px;background:#1c1c1e;color:var(--tx);font:inherit;font-size:18px;cursor:pointer;touch-action:manipulation;padding:6px 4px}
.tdash .qb svg{width:30px;height:30px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.tdash .qb small,.tdash .dock .seatb .lv{font-size:18px;color:var(--mut);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:100%}
.tdash .qb.on{background:var(--tx);color:#000}.tdash .qb.on small{color:#333}.tdash .qb.amb.on{background:var(--amb)}.tdash .qb.blu.on{background:var(--blu);color:#fff}.tdash .qb.blu.on small{color:#dde}
.tdash .qb.dis{opacity:.45}.tdash .dock .seatb svg{width:34px;height:34px}.tdash .dock .seatb>span{text-align:center}.tdash .dock .seatb .k{font-size:18px}
.tdash .toast{min-height:26px;color:var(--amb);font-size:18px;text-align:center}
.tdash .mnu{display:none;position:absolute;inset:0;z-index:4;background:var(--bg);grid-template-columns:260px 1fr;gap:18px;font-size:20px}.tdash .mnu.on{display:grid}
.tdash .mnu nav{display:flex;flex-direction:column;gap:6px;overflow:auto}
.tdash .mnu nav button{display:flex;align-items:center;gap:12px;min-height:56px;border:0;border-radius:12px;background:none;color:var(--mut);font:inherit;font-size:20px;text-align:left;padding:0 14px;cursor:pointer}
.tdash .mnu nav button.on{background:#1c1c1e;color:var(--tx)}.tdash .mnu nav svg{width:26px;height:26px;flex:none;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.tdash .mnu .mc{overflow:auto;min-height:0;padding-right:6px}.tdash .mnu section{display:none;flex-direction:column;gap:14px}.tdash .mnu section.on{display:flex}
.tdash .mnu h3{margin:0;font-size:26px;font-weight:500}.tdash .mnu .k,.tdash .mnu .tag,.tdash .mnu small,.tdash .mnu label,.tdash .mnu td,.tdash .mnu th{font-size:18px}
.tdash .mnu .tag{letter-spacing:.02em;text-transform:none}.tdash .mnu .ctb{min-height:72px;font-size:20px}.tdash .mnu .ctb small{font-size:18px}
.tdash .mnu button.mb{min-height:52px;border:0;border-radius:12px;background:#2c2c2e;color:var(--tx);font:inherit;font-size:20px;padding:0 18px;cursor:pointer}.tdash .mnu button.mb.on{background:var(--tx);color:#000}
.tdash .mnu .row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.tdash .mnu input[type=number],.tdash .mnu input:not([type]){min-height:48px;background:#2c2c2e;border:0;border-radius:10px;color:var(--tx);font:inherit;font-size:20px;padding:0 10px;width:110px}.tdash .mnu input:not([type]){width:100%}
.tdash .mnu input[type=range]{flex:1;min-width:220px;accent-color:#30d158}.tdash .mnu input[type=checkbox]{width:26px;height:26px}
.tdash .mnu .cset{font-size:18px}.tdash .mnu .cset input[type=number]{width:90px;min-height:44px}
.tdash .kv{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:10px}.tdash .kv .cell .v{font-size:28px}
.tdash .mt{width:100%;border-collapse:collapse}.tdash .mt td,.tdash .mt th{padding:10px 8px;border-bottom:1px solid #1c1c1e;text-align:left;vertical-align:top}.tdash .mt th{color:var(--mut);font-weight:500}
.tdash .cg{display:grid;grid-template-columns:repeat(12,1fr);gap:4px}.tdash .cg i{font-style:normal;font-size:18px;text-align:center;padding:6px 0;border-radius:6px;background:#1c1c1e;font-variant-numeric:tabular-nums}
.tdash .cg i.lo{background:#3a1416;color:#ff8a8f}.tdash .cg i.hi{background:#10331c;color:#7ee2a0}.tdash .cg i.mn{outline:2px solid var(--red)}.tdash .cg i.mx{outline:2px solid var(--grn)}
.tdash .crv{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:12px}.tdash .crv svg{width:100%;background:var(--card);border-radius:12px}
.tdash .crv .act{outline:2px solid var(--amb);border-radius:12px}
.tdash .lst{display:flex;flex-direction:column;gap:4px;font-size:18px}.tdash .lst div{padding:8px 10px;background:var(--card);border-radius:8px}.tdash .lst .f{color:#ff8a8f}
.tdash .note{color:var(--amb);font-size:18px}
.tdash .simp{border:2px dashed var(--amb);border-radius:14px;padding:8px 12px;display:flex;flex-direction:column;gap:8px;font-size:18px}
.tdash .simp .sh{display:flex;align-items:center;gap:12px;color:var(--amb);font-weight:600;letter-spacing:.06em}.tdash .simp .sh span{flex:1}
.tdash .simp.col .sb{display:none}.tdash .simp .sr{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.tdash .simp button{min-height:44px;border:0;border-radius:10px;background:#2c2c2e;color:var(--tx);font:inherit;font-size:18px;padding:0 12px;cursor:pointer;touch-action:manipulation}
.tdash .simp button.on{background:var(--tx);color:#000}.tdash .simp button.go{background:var(--grn);color:#000}.tdash .simp button.red{background:#5a1a1d}
.tdash .simp label{display:flex;align-items:center;gap:6px;color:var(--mut)}.tdash .simp input[type=range]{width:170px}.tdash .simp b{min-width:56px;color:var(--tx);font-variant-numeric:tabular-nums;font-weight:500}
.tdash .simp .ro{font-variant-numeric:tabular-nums;color:var(--tx);display:flex;flex-wrap:wrap;gap:4px 18px}.tdash .simp .ro span{color:var(--mut)}
.tdash.charging:not(.splitOn) .home{display:none}
.tdash,.tdash *{box-sizing:border-box}.tdash .dock,.tdash .toast{flex:none}.tdash.full .stage{overflow:auto}
.tdash .split{display:none;flex:1;min-height:0;grid-template-columns:1fr 1fr;gap:14px}.tdash.splitOn .split{display:grid}
.tdash.splitOn .stage>.home,.tdash.splitOn .stage>.chg,.tdash.splitOn .mnu{display:none!important}
.tdash .pane{display:flex;flex-direction:column;gap:8px;min-width:0;min-height:0;background:#0b0b0c;border-radius:16px;padding:10px 12px}
.tdash .pane select{min-height:48px;background:#1c1c1e;color:var(--tx);border:0;border-radius:10px;font:inherit;font-size:20px;padding:0 12px}
.tdash .pane .pb{flex:1;min-height:0;overflow:auto;font-size:20px}.tdash .pane .pb section{display:flex;flex-direction:column;gap:12px}
.tdash .pane .pb section h3{display:none}.tdash .pane .k,.tdash .pane .tag,.tdash .pane small,.tdash .pane label,.tdash .pane td,.tdash .pane th{font-size:18px}.tdash .pane .tag{text-transform:none;letter-spacing:.02em}
.tdash .pane .home{grid-template-columns:1fr}.tdash .pane .home .xl{font-size:110px}.tdash .pane .kv{grid-template-columns:repeat(auto-fill,minmax(170px,1fr))}
.tdash .pane .cg{grid-template-columns:repeat(auto-fill,minmax(66px,1fr))}.tdash .pane .cgr{grid-template-columns:repeat(auto-fill,minmax(140px,1fr))}
.tdash .pane input[type=number]{min-height:44px;background:#2c2c2e;border:0;border-radius:10px;color:var(--tx);font:inherit;font-size:20px;padding:0 10px;width:100px}
.tdash .pane button.mb{min-height:48px;border:0;border-radius:12px;background:#2c2c2e;color:var(--tx);font:inherit;font-size:18px;padding:0 14px;cursor:pointer}.tdash .pane button.mb.on{background:var(--tx);color:#000}
.tdash .pane .row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}.tdash .pane input[type=range]{flex:1;min-width:160px;accent-color:#30d158}
.tdash .pane .ctb{min-height:68px;font-size:18px}.tdash .pane .ctb small{font-size:18px}
@media(max-width:1000px){.tdash .split{grid-template-columns:1fr;grid-template-rows:1fr 1fr}}
@media(max-width:900px){.tdash .home{grid-template-columns:1fr}.tdash .dock{flex-wrap:wrap}.tdash .qb{flex:1 0 22%}.tdash .mnu{grid-template-columns:1fr}}
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
 d4wd:'<circle cx="6" cy="6" r="2.5"/><circle cx="18" cy="6" r="2.5"/><circle cx="6" cy="18" r="2.5"/><circle cx="18" cy="18" r="2.5"/><path d="M8.5 6h7M8.5 18h7M12 6v12"/>',
 diff:'<circle cx="12" cy="12" r="5"/><path d="M2 12h5M17 12h5M12 7v10M9 9l6 6"/>',
 turnL:'<path d="M3 12l7-7v4h11v6H10v4z"/>',turnR:'<path d="M21 12l-7-7v4H3v6h11v4z"/>',
 high:'<path d="M10 6c5 0 8 2.5 8 6s-3 6-8 6c-1.5 0-2-2.7-2-6s.5-6 2-6z"/><path d="M2 7h5M2 10h5M2 13h5M2 16h5"/>',
 hazard:'<path d="M12 3 2 20h20L12 3z"/><path d="M12 8l-5 9h10z"/>',
 alarm:'<path d="M6 16V11a6 6 0 0 1 12 0v5l2 2H4z"/><path d="M10 21h4"/>',
 menu:'<path d="M4 6h16M4 12h16M4 18h16"/>',mode:'<path d="M5 18a8 8 0 1 1 14 0"/><path d="M12 14l4-5"/>',plug:'<path d="M9 3v5M15 3v5M7 8h10v4a5 5 0 0 1-10 0zM12 17v4"/>',
 car:'<path d="M3 16v-4l2-5h14l2 5v4zM3 16v2M21 16v2"/><circle cx="7.5" cy="13" r="1"/><circle cx="16.5" cy="13" r="1"/>',gear:'<circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M5 5l2 2M17 17l2 2M5 19l2-2M17 7l2-2"/>',
 disp:'<rect x="3" y="4" width="18" height="12" rx="2"/><path d="M8 20h8M12 16v4"/>',wire:'<path d="M4 6h6v4h4v4h6M4 18h4"/><circle cx="4" cy="6" r="1.5"/><circle cx="20" cy="14" r="1.5"/>',info:'<circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7.5v.5"/>',list:'<path d="M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01"/>'};
const TT=[['turnL','g','Left turn'],['d4wd','a','4WD engaged (status input UNVERIFIED)'],['diff','r','Diff locked (actuator UNVERIFIED)'],['high','b','High beam'],['hazard','r','Hazard'],['fault','r','BMS fault'],['hv','a','HV contactors open'],['batt','a','Low cell'],['temp','r','Pack hot'],['limp','a','Limp / derate'],['regen','a','Regen off'],['imb','a','LEM imbalance'],['link','a','No data'],['lights','g','Lights'],['fan','b','Cooling fan'],['seat','a','Seat heater'],['pump','b','Kelly coolant pump on'],['coolant','r','Coolant warning (no live source yet)'],['alarm','r','Alarm'],['turnR','g','Right turn']];
const STY=['digital','analog','minimal'];   // Night is a separate toggle + dim level (Adam, Build 2)
const ASSUMED_WHMI=350;           // PROPOSED fallback, docs/DASH.md
const ASSUMED_PACK_KWH=36*3.6*180/1000; // SIM-ASSUMED 180 Ah (4 × 45 Ah) × 36 × 3.6 V
const RPM_PER_MPH=8.75*1056/(Math.PI*24/12); // ≈122.5 RPM/mph: 16/32 chain × 4.375 diff = 8.75 (SOURCED) on 24" stock tires (SOURCED: Adam)
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
const TeryxDash_SEC=[['batt','Battery & cells','batt'],['chg','Charging','plug'],['drive','Drive & Kelly','mode'],['lights','Lights','lights'],['wd','4WD & diff','d4wd'],
  ['flt','Faults & log','fault'],['cool','Cooling','fan'],['veh','Vehicle','car'],['disp','Display','disp'],['diag','Diagnostics','wire'],['about','About','info']];
// Pin map shown in Menu › Diagnostics (moved from the sim Cluster tab). Status column is the honest state of each signal.
const IOMAP=[['Coolant pump','PROPOSED: stock Teryx radiator + Bosch 12 V pump → Kelly W3-GT2 heatsink; on whenever key is on; Teensy B FET; no flow/coolant-temp sensor yet','PROPOSED, pin per include/Config.h Pins:: reservation'],
 ['Radiator fan','PROPOSED: 12 V relay driven by Teensy, on at ~40 °C from Kelly temp (CAN 0x0CF11F05) or a coolant sensor','PROPOSED, pin per Pins:: reservation'],
 ['Seat heaters (driver + passenger)','Likely aftermarket kit (single/two-stage UNVERIFIED). PROPOSED MOSFET PWM 33/66/100 % for Low/Med/High; ~4–5 A per seat, 10 A fuse (ESTIMATE)','PROPOSED'],
 ['Lights','74HC595 shift register + FETs on their own pins (not the throttle SPI bus)','PROPOSED, not wired'],
 ['4WD / diff actuators','2 × 12 V linear actuators with pot feedback, one BTS7960 H-bridge each (enable held off at boot)','PROPOSED, nothing bought'],
 ['Throttle pedal','Hall pedal 0–5 V → Kelly throttle input (direct). Teensy reads it back from Kelly CAN 0x0CF11F05 b0','Kelly pin UNVERIFIED'],
 ['Brake','Brake pressure → Kelly Brake_AN (analog regen; per brief, UNVERIFIED on harness), regen up to PROPOSED 60 A; PROPOSED parallel tap to Teensy A GPIO','PROPOSED'],
 ['Plug-in / proximity','Charger-present input wakes the BMS in CHARGE without key-on (J1772 PP or Elcon AC-present, part not chosen)','PROPOSED; firmware input stays false'],
 ['Gear H/M/L','Kelly speed-switch inputs (3-speed, UNVERIFIED); Teensy OUT3 pin 20 drives limp (low-speed)','Kelly pins UNVERIFIED'],
 ['Reverse','Kelly reverse switch input; PROPOSED Teensy tap to show R and inhibit regen/charge','PROPOSED'],
 ['Speed','Kelly CAN 0x0CF11E05 RPM ÷ (16/32 sprockets × 4.375 diff = 8.75, both SOURCED) × 24" stock tire (SOURCED: Adam)','needs BMS_CAP_KELLY_RX=1 after candump'],
 ['Pack V','BICM cell sums (CAN1/CAN2 @125k); Kelly bus V as cross-check','BICM wired; Kelly RX off'],
 ['Pack A','LEM A (A0) + LEM B (A1), 5 V ref A11; sign flip + zero cal TODO','CURRENT_SENSE_ENABLE=0'],
 ['Cell min/max','Volt BICMs M2/M3/M4 over CAN1 (22/23) and CAN2 (0/1), 4 buses','live on bench'],
 ['Pack temps','BICM thermistors','TEMPS_VERIFIED=0'],
 ['Kelly / motor temp','Kelly CAN 0x0CF11F05 b1/b2 (offset TBD)','Kelly RX off'],
 ['SoC / range','BmsCore::Soc coulomb count on LEM A + B, full-charge anchor; range = SoC × ASSUMED kWh ÷ Wh/mi','sim only · capAh UNVERIFIED'],
 ['Cluster display','Teensy A Serial4 → ESP32 (STATUS_SCHEMA v1) → Wi-Fi → head unit','schema exists']];

// ---- PIN pad (modal) + drive-settings lock (Build 4, PROPOSED). Shared by the sim page and dash.html.
const TeryxPad=(()=>{let ov=null;
  function css(){if(document.getElementById('tpad-css'))return;const st=document.createElement('style');st.id='tpad-css';st.textContent=`.tpad-ov{position:fixed;inset:0;background:rgba(0,0,0,.72);z-index:9999;display:flex;align-items:center;justify-content:center}
.tpad{background:#111214;border:1px solid #2c2c30;border-radius:18px;padding:18px;width:min(340px,92vw);color:#f5f5f7;font:16px system-ui,-apple-system,sans-serif;text-align:center}
.tpad .tpt{font-size:17px;font-weight:600;margin-bottom:8px}.tpad .tpd{font-size:30px;letter-spacing:10px;margin:6px 0}.tpad .tpm{min-height:22px;color:#a1a1a8;font-size:14px}.tpad .tpm.bad{color:#ff6b6f;font-weight:600}
.tpad .tpk{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin:12px 0}.tpad button{min-height:56px;border-radius:12px;border:0;background:#1f2024;color:#f5f5f7;font-size:22px}
.tpad .tpb{display:flex;gap:10px}.tpad .tpb button{flex:1;font-size:17px}.tpad .tpb .go{background:#3e6ae1}@keyframes tpsh{0%,100%{transform:none}25%{transform:translateX(-10px)}75%{transform:translateX(10px)}}.tpad.shake{animation:tpsh .3s}
.tlock-toast{position:fixed;left:50%;bottom:28px;transform:translateX(-50%);background:#1f2024;color:#f5f5f7;border:1px solid #3a3a3e;border-radius:12px;padding:10px 16px;z-index:10000;font:15px system-ui;max-width:90vw}
body.drive-locked [data-drive-lock]{opacity:.55;position:relative}body.drive-locked [data-drive-lock]::before{content:"🔒 " attr(data-lock-msg);display:block;font:600 13px system-ui;color:#d6a33a;margin:2px 0 4px}`;document.head.appendChild(st);}
  function close(){if(ov){ov.remove();ov=null;document.removeEventListener('keydown',kd,true);}}
  let kd=null;
  function open(o){css();close();ov=document.createElement('div');ov.className='tpad-ov';ov.setAttribute('data-f','pinpad');
    ov.innerHTML=`<div class="tpad" role="dialog" aria-modal="true" aria-label="${o.title}"><div class="tpt">${o.title}</div><div class="tpd" data-p="d"></div><div class="tpm" data-p="m">${o.note||''}</div><div class="tpk">${[1,2,3,4,5,6,7,8,9,'⌫',0,'C'].map(k=>`<button data-k="${k}">${k}</button>`).join('')}</div><div class="tpb"><button data-a="cancel">Cancel</button><button data-a="enter" class="go">Enter</button></div></div>`;
    document.body.appendChild(ov);let pin='';const P=ov.querySelector('.tpad'),Dd=ov.querySelector('[data-p=d]'),M=ov.querySelector('[data-p=m]');
    const paint=()=>{Dd.textContent='●'.repeat(pin.length)+'○'.repeat(Math.max(0,4-pin.length));};paint();
    const fail=m=>{M.textContent=m;M.className='tpm bad';pin='';paint();P.classList.remove('shake');void P.offsetWidth;P.classList.add('shake');};
    const enter=async()=>{if(pin.length!==4){fail('Enter 4 digits');return;}let r=o.onEnter(pin);if(r&&r.then)r=await r;if(r)fail(r);else close();};
    const key=k=>{if(/^[0-9]$/.test(k)){if(pin.length<4)pin+=k;M.className='tpm';M.textContent=o.note||'';paint();if(pin.length===4&&o.autoEnter)enter();}else if(k==='⌫'||k==='Backspace'){pin=pin.slice(0,-1);paint();}else if(k==='C'){pin='';paint();}};
    ov.querySelectorAll('[data-k]').forEach(b=>b.onclick=e=>{e.stopPropagation();key(String(b.dataset.k));});
    ov.querySelector('[data-a=enter]').onclick=e=>{e.stopPropagation();enter();};ov.querySelector('[data-a=cancel]').onclick=e=>{e.stopPropagation();close();if(o.onCancel)o.onCancel();};
    kd=e=>{if(!ov)return;if(/^[0-9]$/.test(e.key)||e.key==='Backspace'){key(e.key);e.preventDefault();e.stopPropagation();}else if(e.key==='Enter'){enter();e.preventDefault();e.stopPropagation();}else if(e.key==='Escape'){close();if(o.onCancel)o.onCancel();}};
    document.addEventListener('keydown',kd,true);return ov;}
  let tt=null;function toast(m){css();let t=document.querySelector('.tlock-toast');if(!t){t=document.createElement('div');t.className='tlock-toast';document.body.appendChild(t);}t.textContent=m;t.style.display='';clearTimeout(tt);tt=setTimeout(()=>{t.style.display='none';},3500);}
  return {open,close,toast,isOpen:()=>!!ov};})();
// Drive settings (modes, thresholds, limits) lock. PROPOSED phone-as-key: key OFF = locked; key ON + no phone key = PIN pad first.
// Display-only settings (night, dim, style) are never marked data-drive-lock.
const TeryxLock={keyOn:false,phone:true,unlocked:false,verify:null,hasPin:null,setPin:null,
  locked(){return !this.keyOn||(!this.phone&&!this.unlocked);},
  msg(){return !this.keyOn?'Drive settings locked — key is off':'Drive settings locked — no phone key: tap to enter PIN';},
  set(o){const was=this.keyOn;Object.assign(this,o);if(!this.keyOn||this.phone)this.unlocked=false;if(typeof document!=='undefined'){document.body.classList.toggle('drive-locked',this.locked());document.querySelectorAll('[data-drive-lock]').forEach(e=>e.setAttribute('data-lock-msg',this.msg()));}},
  prompt(){if(!this.keyOn){TeryxPad.toast(this.msg());return;}if(!this.verify){TeryxPad.toast('No PIN available');return;}
    if(this.hasPin&&!this.hasPin()){TeryxPad.toast('Set a PIN first (enter Kid mode once to set it)');return;}
    TeryxPad.open({title:'Enter PIN to change drive settings',note:'No phone key detected (PROPOSED)',onEnter:p=>{const r=this.verify(p);const ok=r&&r.then?r.then(x=>x):r;const fin=x=>{if(x){this.unlocked=true;this.set({});TeryxPad.toast('Drive settings unlocked until key off');return '';}return 'Wrong PIN — try again';};return ok&&ok.then?ok.then(fin):fin(ok);}});}};
if(typeof document!=='undefined'){const guard=e=>{const t=e.target&&e.target.closest&&e.target.closest('[data-drive-lock]');if(!t||!TeryxLock.locked()||TeryxPad.isOpen())return;e.preventDefault();e.stopImmediatePropagation();if(e.type==='click')TeryxLock.prompt();if(e.type==='focusin'&&e.target.blur)e.target.blur();};
  ['pointerdown','mousedown','touchstart','click','focusin','keydown','input','change'].forEach(t=>document.addEventListener(t,guard,{capture:true,passive:false}));}
if(typeof window!=='undefined'){window.TeryxPad=TeryxPad;window.TeryxLock=TeryxLock;}
const DASH_BUILD='@@BUILD@@';
function create(root,opt){
  opt=opt||{};if(!document.getElementById('tdash-css')){const st=document.createElement('style');st.id='tdash-css';st.textContent=CSS;document.head.appendChild(st);}
  const spd=dial('ds',0,60,10,'mph'),pw=dial('dp',-20,100,20,'kW',0,0),bt=dial('db',0,100,25,'% battery');
  const SEC=TeryxDash_SEC;
  const sim=opt.simPanel||null;
  const simHtml=!sim?'':`<div class="simp${localStorage.getItem('teryxSimPanelCol')==='1'?' col':''}" data-f="simp"><div class="sh"><span>SIMULATOR · fake inputs, not the Teryx</span><button data-f="simcol">${localStorage.getItem('teryxSimPanelCol')==='1'?'Show ▾':'Hide ▴'}</button></div>
    <div class="sb"><div class="sr">${sim.hostKey?'':'<button data-sc="key" data-f="skey">Key: OFF</button><button data-sc="plug" data-f="splug">Plug in</button>'}
      <button data-sc="park" data-f="spark" class="pkb" title="Parking brake (2 contacts + Kelly Foot_SW interlock): Kelly disabled while set">Ⓟ Park brake</button><button data-sc="neut" data-f="sneut" title="F/N/R selector">F/N/R</button>
      <label>Throttle <input type="range" min="0" max="100" value="0" data-sv="thr"><b data-f="sv_thr">0 %</b></label>
      <label>Brake <input type="range" min="0" max="100" value="0" data-sv="brk"><b data-f="sv_brk">0 %</b></label>
      <label>Grade <input type="range" min="-20" max="20" step="0.5" value="0" data-sv="grade"><b data-f="sv_grade">0 %</b></label>
      <span class="sr" data-f="sgear" data-drive-lock>${['H','M','L','SNOW','MUD','SAND','KID','R'].map(g=>`<button data-sg="${g}">${MODE_NAME[g]}</button>`).join('')}</span></div>
     <div class="sr">${[['launch','Hard launch'],['regen','Regen stop'],['coast','Coast'],['climb','Climb'],['lowcell','Low cell'],['hotpack','Hot pack'],['lemimb','LEM imbalance'],['weld','Contactor weld'],['lemdrop','LEM dropout'],['chglim','Charge to limit'],['pcycle','Power cycle'],['wake','Wake'],['reset','Reset…']].map(([k,t])=>`<button data-sc="${k}"${k==='reset'||k==='weld'?' class="red"':''}>${t}</button>`).join('')}</div>
     <div class="sr"><span>Drive cycle</span><span class="sr" data-f="scyc"></span><button data-sc="cycle" data-f="scgo" class="go">Start</button><button data-sc="speed20" data-f="s20">20× speed</button><span data-f="scst" class="k"></span></div>
     <div class="ro" data-f="sro"></div></div></div>`;
  const qb=(k,ic,t,c)=>`<button class="qb${c?' '+c:''}" data-q="${k}"><svg viewBox="0 0 24 24">${IC[ic]}</svg><span>${t}</span><small data-f="q_${k}">—</small></button>`;
  const seatBtn=k=>`<button class="seatb" data-seat="${k}" title="Tap: Off→Low→Med→High · hold: off"><svg viewBox="0 0 40 40"><path class="st" d="M8 8v16h12l3 10M8 24l-3 10M11 16h7"/><path class="wv" data-w="1" d="M25 3c3 3-3 5 0 8s-3 5 0 8"/><path class="wv" data-w="2" d="M30 3c3 3-3 5 0 8s-3 5 0 8"/><path class="wv" data-w="3" d="M35 3c3 3-3 5 0 8s-3 5 0 8"/></svg><span><span class="k">${k==='driver'?'Seat L':'Seat R'}</span> <span class="lv" data-f="lv_${k}">Off</span></span></button>`;
  root.innerHTML=`<div class="tdash${opt.full?' full':''}">${simHtml}
   <div class="top"><div class="tt">${TT.map(([k,c,t])=>`<i data-k="${k}" class="${c}" title="${t}"><svg viewBox="0 0 24 24">${IC[k]}</svg></i>`).join('')}</div>
    <span class="srcb" data-f="srcb"></span></div>
   <div class="ban" data-f="ban"><span data-f="banT"></span><button data-f="ack">Acknowledge</button></div>
   <div class="stage">
   <div class="chg" data-f="chg">
    <div><div class="cbat" data-f="cbat"><i data-f="cfill"></i><b data-f="clim"></b></div>
      <div style="display:flex;align-items:baseline;gap:18px;margin-top:14px"><div class="csoc" data-f="csoc">—</div><div><div class="cst" data-f="cst">—</div><div class="k" data-f="cwhy"></div></div></div></div>
    <div class="cgrid">
      <div class="cell"><div class="k">Power in</div><div class="v" data-f="ckw">—</div></div><div class="cell"><div class="k">Current</div><div class="v" data-f="ca">—</div></div><div class="cell"><div class="k">Voltage</div><div class="v" data-f="cv">—</div></div>
      <div class="cell"><div class="k">Time to limit <span class="tag">ESTIMATE</span></div><div class="v" data-f="ctime">—</div></div><div class="cell"><div class="k">Range added <span class="tag">ESTIMATE</span></div><div class="v" data-f="cradd">—</div></div><div class="cell"><div class="k">Limited by</div><div class="v" data-f="clmt" style="font-size:20px">—</div></div>
      <div class="cell"><div class="k">Cell max</div><div class="v" data-f="ccmx">—</div></div><div class="cell"><div class="k">Cell temps</div><div class="v" data-f="ctmp">—</div></div><div class="cell"><div class="k">Charger <span class="tag" data-f="cchT"></span></div><div class="v" data-f="cch" style="font-size:18px;white-space:normal">—</div></div>
    </div>
    <div class="cctl"><span class="k">Charge limit</span><input type="range" min="50" max="100" step="1" data-f="cpct"><b data-f="cpctv">—</b>
      <span class="k">Amps limit</span><input type="number" min="1" max="100" step="0.5" data-f="camp"><span class="k">A</span>
      <span class="k">Sleep</span><input type="number" min="10" max="3600" step="10" data-f="cslp"><span class="k">s <span class="tag">PROPOSED</span></span>
      <button data-f="cbtn" class="stop">Stop charging</button><button data-f="cmenu">Menu</button><span class="tag" data-f="cnote"></span></div>
   </div>
   <div class="home">
    <div class="clu">
     <div class="dig">
      <div class="spdw"><div class="v xl" data-f="spd">—</div><div><div class="k">mph <span class="tag" data-f="spdTag"></span></div><div class="gear" data-f="gear" style="justify-content:flex-start"></div></div></div>
      <div><div class="k">Power <span class="tag" data-f="pwTag"></span> <span class="v md" data-f="kw" style="margin-left:8px">—</span></div>
        <div class="pbar"><b></b><i data-f="pbar"></i></div><div class="pbl"><span data-f="rgl">regen</span><span>0</span><span>power →</span></div></div>
     </div>
     <div class="ana"><div class="dials"><div>${spd.svg}</div><div>${pw.svg}</div><div>${bt.svg}</div></div></div>
     <div class="crow">
      <div><div class="k">Battery <span class="tag" data-f="socTag"></span></div><div class="v md" data-f="soc">—</div><div class="batt"><i data-f="sbar" style="width:0"></i></div></div>
      <div><div class="k">Range <span class="tag" data-f="rgTag"></span></div><div class="v md" data-f="rng">—</div><span data-f="rng2" hidden></span><span data-f="rgTag2" hidden></span></div>
      <div class="hide-min"><div class="k">Now</div><div class="v md" data-f="inst">—</div><div class="k" style="margin-top:6px">RPM <span class="tag" data-f="rpmTag"></span> <span class="v" data-f="rpm" style="font-size:24px">—</span></div></div>
     </div>
    </div>
    <div class="info">
     <div class="lim" data-f="lim"><div class="k">Power allowed</div><b data-f="limP">—</b> <span data-f="limW"></span></div>
     <div class="bot">
      <div class="cell trip"><div class="k">Trip <button class="tbtn" data-f="trst" style="min-height:0;padding:0 6px;font-size:18px">reset</button></div>
       <div><div class="k">mi</div><div class="v" data-f="tmi">—</div></div><div><div class="k">kWh used</div><div class="v" data-f="tkwh">—</div></div><div><div class="k">kWh regen</div><div class="v" data-f="treg" style="color:var(--grn)">—</div></div></div>
      <div class="cell"><div class="k">Avg trip · last ${N_MI} mi</div><div class="v" data-f="avg">—</div></div>
      <div class="cell"><div class="k">Pack</div><div class="v" data-f="pack">—</div></div>
      <div class="cell"><div class="k">Cell min / max</div><div class="v" data-f="cell">—</div></div>
      <div class="cell"><div class="k">Pack · Kelly · motor °C</div><div class="v" data-f="tmp">—</div></div>
      <div class="cell"><div class="k">Coolant pump <span class="tag">proposed</span></div><div class="v" data-f="pump">—</div></div>
      <div class="cell"><div class="k">12 V</div><div class="v" data-f="l12" style="font-size:18px;white-space:normal">—</div></div>
     </div>
    </div>
   </div>
   <div class="split" data-f="split">${['L','R'].map(p=>`<div class="pane" data-pane="${p}"><select data-f="pick${p}" aria-label="${p==='L'?'Left':'Right'} pane">${[['home','Cluster'],...SEC.map(x=>[x[0],x[1]])].map(([k,t])=>`<option value="${k}">${t}</option>`).join('')}</select><div class="pb" data-f="pb${p}"></div></div>`).join('')}</div>
   <div class="mnu" data-f="menu"><nav>${SEC.map(([k,t,ic])=>`<button data-sec="${k}"><svg viewBox="0 0 24 24">${IC[ic]}</svg>${t}</button>`).join('')}<button data-f="mclose"><svg viewBox="0 0 24 24"><path d="M6 6l12 12M18 6 6 18"/></svg>Close</button></nav>
    <div class="mc">
     <section data-s2="batt"><h3>Battery &amp; cells</h3><div class="kv" data-f="mbk"></div><div class="k" data-f="mbcap"></div><div data-f="mcells"></div></section>
     <section data-s2="chg"><h3>Charging</h3><div class="kv" data-f="mck"></div>
      <div class="row"><span class="k">Charge limit</span><input type="range" min="50" max="100" step="1" data-f="mlim"><b data-f="mlimv">—</b></div>
      <div class="row"><span class="k">Amps limit</span><input type="number" min="1" max="100" step="0.5" data-f="mamp"><span class="k">A</span><span class="tag" data-f="mampN"></span></div>
      <div class="k">The charging screen opens by itself when the charger is plugged in. Drive stays locked while plugged in.</div></section>
     <section data-s2="drive"><h3>Drive &amp; Kelly</h3><div class="kv" data-f="mdk"></div><div class="k">Kelly signals <span class="tag">PRINT-ONLY · BMS_CAP_KELLY_LIMIT_DRV=0</span></div><div class="kv" data-f="mds"></div>
      <div class="k">Cutback curves (dot = now, outlined = the limiter in charge)</div><div class="crv" data-f="mcrv"></div></section>
     <section data-s2="lights"><h3>Lights</h3><div class="k">Outputs <span class="tag">PROPOSED · not wired</span></div>
      <div class="cgr">${TeryxControls.LIGHTS.map(([k,t])=>`<button class="ctb${k==='high'?' blu':''}" data-lt="${k}">${t}<small data-f="ls_${k}">off</small></button>`).join('')}
       <button class="ctb amb" data-turn="L">◀ Turn left<small>tap to cancel</small></button><button class="ctb amb" data-f="haz">Hazard<small>both turns blink</small></button><button class="ctb amb" data-turn="R">Turn right ▶<small>tap to cancel</small></button>
       <button class="ctb auto" data-f="lbrk">Brake light<small>automatic · brake pedal</small></button><button class="ctb auto" data-f="lrev">Reverse light<small>automatic · gear R</small></button></div></section>
     <section data-s2="wd"><h3>4WD &amp; diff lock</h3><div class="k">Conditions PROPOSED · actuators UNVERIFIED, nothing bought</div>
      <div class="cgr"><button class="ctb amb" data-f="b4wd">2WD rear<small data-f="w4wd"></small></button>${TeryxControls.DIFF.map(k=>`<button class="ctb ${k==='off'?'':'amb'}" data-dif="${k}">Diff ${k==='off'?'open':k}<small data-f="wd_${k}"></small></button>`).join('')}</div>
      <div class="k" data-f="apos"></div><div class="note">Steering angle not sensed (UNAVAILABLE): keep the wheels straight before locking the diff.</div>
      <div class="row"><button class="mb" data-f="aclr" style="display:none">Clear actuator fault</button>${opt.simControls?'<label class="k"><input type="checkbox" data-f="st4"> sim: stall 4WD actuator</label> <label class="k"><input type="checkbox" data-f="std"> sim: stall diff actuator</label>':''}</div>
      <div class="note" data-f="cmsg"></div>
      <div class="k">Settings · all PROPOSED · each change asks first</div><div class="cset" data-f="cset" data-drive-lock></div></section>
     <section data-s2="flt"><h3>Faults &amp; log</h3><div class="k">Active now</div><div class="lst" data-f="mfa"></div><div class="k">Power cutback (derate)</div><div class="kv" data-f="mfd"></div><div class="k">Seen on this screen</div><div class="lst" data-f="mfh"></div><div class="k">Event log <span class="tag" data-f="mflT"></span></div><div class="lst" data-f="mfl"></div></section>
     <section data-s2="cool"><h3>Cooling</h3><div class="kv" data-f="mco"></div><div class="row"><button class="mb" data-f="mfan">Fan override</button><span class="tag" data-f="mfanN"></span></div>
      <div class="k">Pump: PROPOSED Bosch 12 V pump on the stock radiator, on whenever the key is on. Fan: PROPOSED relay, auto at ~40 °C. No flow or coolant-temp sensor yet, so those read —.</div></section>
     <section data-s2="veh" data-drive-lock><h3>Vehicle settings</h3><div class="note">Tire 24 in (SOURCED: Adam, stock tires). At the PROPOSED 6000 RPM High cap the top speed is ≈49 mph; 55 mph would need ≈6740 RPM. Top speed falls as the pack drains (SOURCED: Sparky, back-EMF 0.026 V/RPM): base speed ≈ pack V ÷ 0.026 = ≈5680 RPM (≈46 mph) at 147.6 V full, ≈5120 RPM (≈42 mph) at 133 V nominal, ≈4570 RPM (≈37 mph) at the 118.8 V low cut; above base speed the Kelly must field-weaken, up to the 6000 RPM cap (≈49 mph). mph from the 24in tire and 8.75 ratio; sag under load lowers it further (not modelled). Total ratio 8.75 = sprockets 16T/32T 2.0:1 (SOURCED: cash ledger) × diff 4.375:1 (SOURCED: 2008 owner's manual).</div><div data-f="mveh"></div></section>
     <section data-s2="disp"><h3>Display</h3><div class="k">Cluster style</div><div class="row" data-f="sty">${STY.map(s=>`<button class="mb" data-s="${s}">${s[0].toUpperCase()+s.slice(1)}</button>`).join('')}</div>
      <div class="row"><button class="mb" data-f="nightb">Night mode: off</button><span class="k">Dim</span><input type="range" min="20" max="100" step="5" data-f="dimr" aria-label="Night dim level" style="width:160px"><b data-f="dimv">60 %</b><span class="k"><span class="tag">screen brightness while Night is on</span></span></div>
      <div class="row"><span class="k">Sleep while charging after</span><input type="number" min="10" max="3600" step="10" data-f="msl"><span class="k">s <span class="tag">PROPOSED 120 s</span></span></div>
      <div class="k">Wakes on tap, brake, key, door/button input, charge fault or charge complete.</div>
      ${opt.sourceUI?`<div class="k">Data source</div><div class="row" data-f="srcs"><button class="mb" data-src="sim">Simulator</button><button class="mb" data-src="http">HTTP poll</button><button class="mb" data-src="ws">WebSocket</button><button class="mb" data-src="none">None</button></div>
      <input data-f="url" placeholder="http://teryx-hub.local/api/status or ws://…"><div class="row"><button class="mb" data-f="apply">Apply</button></div>`:''}</section>
     <section data-s2="diag"><h3>Diagnostics &amp; wiring</h3><div class="kv" data-f="mdg"></div><div class="k">Where each value comes from</div><table class="mt" data-f="msrc"></table><div class="k">Pin map</div><table class="mt" data-f="mpin"></table></section>
     <section data-s2="about"><h3>About</h3><div class="kv" data-f="mab"></div><div class="k">Rule: a value with no source shows “—” with an amber dot. ASSUMED / PROPOSED / ESTIMATE tags mark anything not measured on the Teryx.</div></section>
    </div></div>
   </div>
   <div class="toast" data-f="toast"></div>
   <div class="dock seats" data-f="dock">${qb('lights','lights','Lights')}${qb('4wd','d4wd','4WD','amb')}${qb('diff','diff','Diff lock','amb')}${seatBtn('driver')}${seatBtn('passenger')}${qb('mode','mode','Drive mode')}${qb('chg','plug','Charge')}${qb('fan','fan','Fan','blu')}${qb('split','disp','Split')}${qb('menu','menu','Menu')}</div>
   <div class="slp" data-f="slp"><svg viewBox="0 0 200 200"><circle cx="100" cy="100" r="80" stroke="#1c1c1e" stroke-width="10" fill="none"/><circle class="rg" data-f="sring" cx="100" cy="100" r="80" stroke-width="10" fill="none" stroke-linecap="round" transform="rotate(-90 100 100)" stroke-dasharray="0 503"/>
     <text data-f="ssoc" x="100" y="112" text-anchor="middle" fill="#8e8e93" font-size="40" font-weight="200">—</text></svg></div>
   </div>`;
  const D=root.firstElementChild,F=n=>D.querySelector(`[data-f="${n}"]`);
  let style=localStorage.getItem('teryxDashStyle')||'digital',ack=null,lastT=0,trip=JSON.parse(localStorage.getItem('teryxDashTrip')||'null')||{mi:0,kwh:0,reg:0},win=[];
  const setStyle=s=>{style=s;STY.forEach(x=>D.classList.toggle(x,x===s));D.querySelectorAll('[data-s]').forEach(b=>b.classList.toggle('on',b.dataset.s===s));localStorage.setItem('teryxDashStyle',s);};
  let night=localStorage.getItem('teryxDashNight')==='1'||style==='night',dimPct=+localStorage.getItem('teryxDashDim')||60;
  const setNight=(on,pct)=>{night=!!on;if(pct!=null)dimPct=Math.max(20,Math.min(100,+pct));D.classList.toggle('night',night);D.style.filter=night?`brightness(${dimPct/100})`:'';
    localStorage.setItem('teryxDashNight',night?'1':'0');localStorage.setItem('teryxDashDim',String(dimPct));
    const b=F('nightb');if(b){b.textContent='Night mode: '+(night?'ON':'off');b.classList.toggle('on',night);F('dimr').value=dimPct;F('dimv').textContent=dimPct+' %';}};
  setStyle(STY.includes(style)?style:'digital');setNight(night);
  F('nightb').onclick=()=>setNight(!night);F('dimr').oninput=e=>setNight(true,e.target.value);
  D.querySelectorAll('[data-s]').forEach(b=>b.onclick=()=>setStyle(b.dataset.s));
  F('trst').onclick=()=>{trip={mi:0,kwh:0,reg:0};win=[];localStorage.setItem('teryxDashTrip',JSON.stringify(trip));};
  F('ack').onclick=()=>{ack=curFaultKey;F('ban').classList.remove('on');};

  // ---- lights / 2WD-4WD / diff lock (Adam 2026-10-21). Logic in sim/controls.js, all thresholds PROPOSED.
  const CT=opt.controls||TeryxControls.create();let lastDrv={mph:null,thr:0,brake:0,gear:null};
  const ctlEv=(t,r)=>{if(opt.onControl)opt.onControl(t,r);ctlPaint();};
  D.querySelectorAll('[data-lt]').forEach(b=>b.onclick=()=>{const k=b.dataset.lt;CT.setLight(k,!CT.S.lights[k]);ctlEv('light '+k+' '+(CT.S.lights[k]?'on':'off'));});
  D.querySelectorAll('[data-turn]').forEach(b=>b.onclick=()=>{CT.setTurn(b.dataset.turn);ctlEv('turn '+(CT.S.turn||'off'));});
  F('haz').onclick=()=>{CT.setHazard(!CT.S.hazard);ctlEv('hazard '+(CT.S.hazard?'on':'off'));};
  F('b4wd').onclick=()=>{CT.toggle4wd(lastDrv);ctlPaint();};
  D.querySelectorAll('[data-dif]').forEach(b=>b.onclick=()=>{CT.setDiff(lastDrv,b.dataset.dif);ctlPaint();});
  F('aclr').onclick=()=>{CT.clearFault();ctlEv('actuator fault cleared');};
  if(opt.simControls){F('st4').onchange=e=>CT.simStall('4wd',e.target.checked);F('std').onchange=e=>CT.simStall('diff',e.target.checked);}
  CT.S.onLog=t=>{if(opt.onControl)opt.onControl(t);};
  F('cset').innerHTML=TeryxControls.CTL_META.map(([k,t,u,n,mn,mx,st])=>`<label title="${n}">${u==='bool'?`<input type="checkbox" data-cs="${k}">`:`<input type="number" data-cs="${k}" min="${mn}" max="${mx}" step="${st}">${u}`} ${t} <span class="tag">${n}</span></label>`).join('');
  D.querySelectorAll('[data-cs]').forEach(e=>{const k=e.dataset.cs;if(e.type==='checkbox')e.checked=!!CT.S.set[k];else e.value=CT.S.set[k];
    e.onchange=()=>{const nv=e.type==='checkbox'?e.checked:(+e.value);if(!confirm('Change "'+(TeryxControls.CTL_META.find(m=>m[0]===k)||[,k])[1]+'" to '+nv+'?')){if(e.type==='checkbox')e.checked=!!CT.S.set[k];else e.value=CT.S.set[k];return;}CT.S.set[k]=e.type==='checkbox'?e.checked:(+e.value);try{CT.save();}catch(x){}ctlEv('setting '+k+' = '+CT.S.set[k]);};});
  let ctlLast=null,mp=0;
  function ctlPaint(){const r=CT.tick(lastDrv);ctlLast=r;const o=r.out;
    D.querySelectorAll('[data-lt]').forEach(b=>{const k=b.dataset.lt,v={low:o.headLow,high:o.headHigh,tail:o.tail,bar:o.bar,work:o.work}[k];b.classList.toggle('on',!!v);F('ls_'+k).textContent=v?(CT.S.lights[k]?'on':'on (with '+(k==='low'?'high beam':'headlights')+')'):'off';});
    D.querySelectorAll('[data-turn]').forEach(b=>b.classList.toggle('on',CT.S.turn===b.dataset.turn&&(b.dataset.turn==='L'?o.turnL:o.turnR)));
    F('haz').classList.toggle('on',CT.S.hazard&&o.turnL);F('lbrk').classList.toggle('on',o.brake);F('lrev').classList.toggle('on',o.reverse);
    const b4=F('b4wd');b4.firstChild.textContent=r.fault4?'4WD actuator FAULT':r.wdShifting?'Shifting → '+CT.S.a4.goal+'…':(r.wd==='4WD'?'4WD':'2WD rear');b4.classList.toggle('on',r.wd==='4WD');b4.classList.toggle('dis',!!r.wdWhy);
    F('w4wd').textContent=r.wdWhy||('tap for '+(r.wd==='4WD'?'2WD':'4WD'));
    D.querySelectorAll('[data-dif]').forEach(b=>{const k=b.dataset.dif,w=r.diffWhy[k];b.classList.toggle('on',r.diff===k&&!r.diffShifting);b.classList.toggle('dis',!!w);
      F('wd_'+k).textContent=w||(r.diffShifting&&CT.S.ad.goal===k?'moving…':r.diff===k?(k==='off'?'open':'locked · auto-unlock above '+CT.S.set.diffAutoUnlockMph+' mph'):'tap');});
    F('apos').textContent=`Actuator position (pot): 4WD ${r.pos4.toFixed(0)} % · diff ${r.posD.toFixed(0)} %  ·  H-bridges: ${o.hb4wd} / ${o.hbDiff}`+(r.fault4||r.faultD?'  ·  FAULT: '+[r.fault4,r.faultD].filter(Boolean).join('; '):'');
    F('aclr').style.display=r.fault4||r.faultD?'':'none';
    F('cmsg').textContent=r.msg;return r;}
  setInterval(ctlPaint,200);
  let curFaultKey='',seat={driver:0,passenger:0},seatSrc=false;const LV=['Off','Low','Med','High'];
  const seatShow=()=>D.querySelectorAll('[data-seat]').forEach(b=>{const k=b.dataset.seat,l=seat[k];b.querySelectorAll('.wv').forEach(w=>w.classList.toggle('on',+w.dataset.w<=l));F('lv_'+k).textContent=LV[l]+(l?' '+[0,33,66,100][l]+'%':'');});
  D.querySelectorAll('[data-seat]').forEach(b=>{let tm=null,held=false;const k=b.dataset.seat;
    const set=l=>{seat[k]=l;seatShow();if(opt.onSeat)opt.onSeat(k,l);};
    b.onpointerdown=()=>{held=false;tm=setTimeout(()=>{held=true;set(0);},600);};
    b.onpointerup=()=>{clearTimeout(tm);if(!held)set((seat[k]+1)%4);};b.onpointerleave=()=>clearTimeout(tm);});
  seatShow();
  // ---- charge screen (Adam 2026-10-21): shown while plugged in; sleeps after a PROPOSED 2 min
  let cs={lim:+(localStorage.getItem('teryxChargeLimit')||0)||null,amps:null,stopped:false,lastAct:performance.now(),sleepS:+(localStorage.getItem('teryxDashSleepS')||120),asleep:false,prev:{},kwhIn:0,plugged:false};
  const wake=why=>{cs.lastAct=performance.now();if(cs.asleep){cs.asleep=false;D.classList.remove('asleep');if(opt.onWake)opt.onWake(why);}};
  D.addEventListener('pointerdown',()=>wake('screen tap'),true);
  F('cslp').value=cs.sleepS;F('cslp').onchange=e=>{const v=Math.max(10,+e.target.value||120);cs.sleepS=v;F('msl')&&(F('msl').value=v);localStorage.setItem('teryxDashSleepS',v);};
  F('cpct').oninput=e=>F('cpctv').textContent=e.target.value+' %';
  F('cpct').onchange=e=>{const v=+e.target.value;if(!confirm('Set charge limit to '+v+' %?')){F('cpct').value=cs.lim||80;F('cpctv').textContent=(cs.lim||80)+' %';return;}
    cs.lim=v;localStorage.setItem('teryxChargeLimit',v);if(opt.onChargeLimit)opt.onChargeLimit(v);};
  F('camp').onchange=e=>{const v=+e.target.value;if(v>0){cs.amps=v;if(opt.onAmpsLimit)opt.onAmpsLimit(v);}};
  F('cbtn').onclick=()=>{cs.stopped=!cs.stopped;if(opt.onChargeStop)opt.onChargeStop(cs.stopped);};
  const fmtDur=h=>h==null||!isFinite(h)?null:(h<1?Math.round(h*60)+' min':Math.floor(h)+' h '+Math.round((h%1)*60)+' min');
  function chargeScreen(s,live,B,C,dt){
    const Ch=(s&&s.charge)||{},on=live&&!!Ch.plugged;
    if(on!==cs.plugged){cs.plugged=on;cs.kwhIn=0;D.classList.toggle('charging',on);wake(on?'plug in':'unplug');}
    if(!on){D.classList.remove('asleep');cs.asleep=false;return;}
    const V=num(Ch.chargeV)&&Ch.chargeA>0?Ch.chargeV:(num(B.packV)?B.packV:null),A=num(Ch.chargeA)?Ch.chargeA:null,kw=V!=null&&A!=null?V*A/1000:null;
    if(dt>0&&kw!=null)cs.kwhIn+=kw*dt/3600;
    const soc=num(B.socPct)?B.socPct:null,lim=num(Ch.limitPct)?Ch.limitPct:cs.lim;
    if(cs.lim==null&&lim!=null)cs.lim=lim;if(document.activeElement!==F('cpct')){F('cpct').value=cs.lim||lim||80;F('cpctv').textContent=(cs.lim||lim||80)+' %';}
    if(document.activeElement!==F('camp')&&num(Ch.ampsLimit))F('camp').value=Ch.ampsLimit;
    // state
    const fault=(s.faults||[]).filter(f=>f.level==='fault').map(f=>f.text);let st,cls,why=Ch.why||'';
    if(fault.length){st='Fault';cls='bad';why=fault.join(', ');}
    else if(C.mainPositive!=='closed'){st='Precharging';cls='wait';why='closing B−, precharge, B+';}
    else if(Ch.why==='at charge limit'){st='Charging complete';cls='ok';why='at '+(lim||'—')+' % limit';}
    else if(Ch.why==='stopped by driver'){st='Stopped';cls='wait';}
    else if(A>0&&num(Ch.maxA)&&A<Ch.maxA*0.97&&Ch.why!=='cold: reduced'){st='Tapering';cls='ok';why='cell max near limit';}
    else if(A>0){st='Charging';cls='ok';}
    else{st='Not charging';cls='wait';why=why||'charger off';}
    F('cst').textContent=st;F('cst').className='cst '+cls;F('cwhy').textContent=why;
    const lm=fault.length?'fault':Ch.why==='cold: reduced'?'temperature (cold band)':st==='Tapering'?'cell max (taper)':st==='Charging complete'?'charge limit %':(num(Ch.ampsLimit)&&num(Ch.chargerMaxA)&&Ch.ampsLimit<Ch.chargerMaxA)?'amps limit setting':A>0?'charger max':'—';
    F('clmt').textContent=lm;
    F('cbat').classList.toggle('idle',!(A>0));F('cfill').style.width=(soc||0)+'%';F('clim').style.left=(cs.lim||lim||80)+'%';
    put('csoc',soc,soc!=null?soc.toFixed(0)+'<span class="u">%</span>':'');
    put('ckw',kw,kw!=null?kw.toFixed(2)+'<span class="u">kW</span>':'');put('ca',A,A!=null?A.toFixed(1)+'<span class="u">A</span>':'');put('cv',V,V!=null?V.toFixed(1)+'<span class="u">V</span>':'');
    const capAh=num(Ch.capAh)?Ch.capAh:180,h=soc!=null&&A>0&&lim!=null?Math.max(0,lim-soc)/100*capAh/A:null;
    put('ctime',h,fmtDur(h)+(num(Ch.capAh)&&!Ch.capAssumed?'':' <span class="tag">ASSUMED '+capAh+' Ah</span>'));
    const rmi=cs.kwhIn*1000/ASSUMED_WHMI;put('cradd',1,rmi.toFixed(1)+'<span class="u">mi</span> <span class="tag">@ '+ASSUMED_WHMI+' Wh/mi ASSUMED</span>');
    put('ccmx',num(B.cellMaxV)?1:null,num(B.cellMaxV)?B.cellMaxV.toFixed(3)+'<span class="u">V</span>':'');
    put('ctmp',num(B.tempMinC)?1:null,num(B.tempMinC)?B.tempMinC.toFixed(0)+'–'+B.tempMaxC.toFixed(0)+'<span class="u">°C</span>'+(B.tempsVerified===false?' <span class="tag">unverified</span>':''):'');
    const chg=Ch.charger||{};put('cch',chg.model?1:null,(chg.model||'')+(num(Ch.chargerMaxA)?' · max '+Ch.chargerMaxA+' A':''));F('cchT').textContent=chg.assumed?'ASSUMED':'';
    cs.stopped=Ch.why==='stopped by driver';F('cbtn').textContent=cs.stopped?'Start charging':'Stop charging';F('cbtn').className=cs.stopped?'go':'stop';
    F('cnote').textContent=opt.onChargeStop?'drive stays locked while plugged in':'display only: no output wired';
    // wake sources: brake, key, door/button (PROPOSED pin), charge event (fault / complete)
    const In=s.inputs||{},P=cs.prev;
    if((s.drive||{}).brakePct>0&&!(P.brake>0))wake('brake pedal');
    if(In.key!==undefined&&P.key!==undefined&&In.key!==P.key)wake('key');
    if(In.door&&!P.door)wake('door / button input');
    if((st==='Fault'||st==='Charging complete')&&P.st!==st)wake('charge '+(st==='Fault'?'fault':'complete'));
    cs.prev={brake:(s.drive||{}).brakePct,key:In.key,door:In.door,st};
    if(!cs.asleep&&performance.now()-cs.lastAct>cs.sleepS*1000){cs.asleep=true;D.classList.add('asleep');}
    const pct=soc||0;F('sring').setAttribute('stroke-dasharray',(5.03*pct).toFixed(0)+' 503');F('ssoc').textContent=soc!=null?soc.toFixed(0)+'%':'—';
  }
  const put=(n,val,txt)=>{const e=F(n);if(!e)return;if(val==null){e.textContent='—';e.classList.add('na');}else{e.innerHTML=txt;e.classList.remove('na');}};
  const tag=(n,t)=>{const e=F(n);if(e)e.textContent=t||'';};
  // ---- Home quick buttons, full menu, sim quick panel (Adam 2026-10-21: Tesla-style home + menu)
  let menuSec=localStorage.getItem('teryxDashSec')||'batt',lastS=null,lastMeta={},fhist=[],prevFault='',toastT=0,toastMsg='';
  const toast=t=>{toastMsg=t;toastT=performance.now();F('toast').textContent=t;};
  // ---- split view (Adam 2026-10-21): two panes, each picks any section; saved in localStorage teryxDashSplit
  let split=Object.assign({on:false,L:'home',R:null},(()=>{try{return JSON.parse(localStorage.getItem('teryxDashSplit')||'{}');}catch(e){return {};}})());
  const homeEl=D.querySelector('.stage>.home'),stageEl=D.querySelector('.stage'),mcEl=D.querySelector('.mnu .mc');
  const secEl=k=>k==='home'?homeEl:D.querySelector(`[data-s2="${k}"]`);
  function applySplit(){
    ['L','R'].forEach(p=>[...F('pb'+p).children].forEach(e=>{if(e===homeEl)stageEl.insertBefore(homeEl,F('split'));else mcEl.appendChild(e);}));
    D.querySelectorAll('[data-s2]').forEach(x=>x.classList.toggle('on',x.dataset.s2===menuSec));
    if(split.on){if(!split.L)split.L='home';if(!split.R||split.R===split.L)split.R=menuSec!==split.L?menuSec:(split.L==='home'?'batt':'home');
      ['L','R'].forEach(p=>{const k=split[p],e=secEl(k);F('pb'+p).appendChild(e);if(k!=='home')e.classList.add('on');F('pick'+p).value=k;});}
    D.classList.toggle('splitOn',split.on);try{localStorage.setItem('teryxDashSplit',JSON.stringify(split));}catch(e){}menuPaint();}
  const setSplit=on=>{split.on=on;if(on)F('menu').classList.remove('on');applySplit();};
  ['L','R'].forEach(p=>F('pick'+p).onchange=e=>{const v=e.target.value,o=p==='L'?'R':'L';if(split[o]===v)split[o]=split[p];split[p]=v;if(v!=='home'){menuSec=v;localStorage.setItem('teryxDashSec',v);}applySplit();});
  const openMenu=k=>{if(split.on){if(k&&k!==split.L){split.R=k;menuSec=k;applySplit();return;}if(k)return;setSplit(false);}if(k)menuSec=k;localStorage.setItem('teryxDashSec',menuSec);F('menu').classList.add('on');
    D.querySelectorAll('[data-sec]').forEach(b=>b.classList.toggle('on',b.dataset.sec===menuSec));D.querySelectorAll('[data-s2]').forEach(x=>x.classList.toggle('on',x.dataset.s2===menuSec));menuPaint();};
  D.querySelectorAll('[data-sec]').forEach(b=>b.onclick=()=>openMenu(b.dataset.sec));
  F('mclose').onclick=()=>F('menu').classList.remove('on');F('cmenu').onclick=()=>openMenu('chg');
  const nextDiff={off:'partial',partial:'full',full:'off'};
  D.querySelectorAll('[data-q]').forEach(b=>b.onclick=()=>{const k=b.dataset.q;
    if(k==='menu')openMenu();
    else if(k==='split')setSplit(!split.on);
    else if(k==='lights'){CT.setLight('low',!CT.S.lights.low);ctlEv('light low '+(CT.S.lights.low?'on':'off'));}
    else if(k==='4wd'){const r=CT.toggle4wd(lastDrv);if(!r.ok)toast(r.why);ctlPaint();}
    else if(k==='diff'){const r=CT.setDiff(lastDrv,nextDiff[CT.S.diff]||'off');if(!r.ok)toast(r.why);ctlPaint();}
    else if(k==='mode'){const g=lastDrv.gear,n={H:'M',M:'L',L:'SNOW',SNOW:'MUD',MUD:'SAND',SAND:'H'}[g]||'H';if(g==='KID'){toast('Kid mode: enter the PIN in the mode selector to leave');}else if(opt.onGear){const r=opt.onGear(n);if(r)toast(r);}else toast('Drive mode is the Kelly speed switch: display only here (not wired)');}
    else if(k==='chg')openMenu('chg');
    else if(k==='fan')fanTap();});
  function fanTap(){if(opt.onFan){const on=!fanOv;fanOv=on;opt.onFan(on);toast('Fan override '+(on?'on':'off'));}else toast('Fan override: display only, relay output PROPOSED and not wired');}
  let fanOv=false;F('mfan').onclick=fanTap;
  const setLimit=v=>{if(!confirm('Set charge limit to '+v+' %?'))return false;cs.lim=v;localStorage.setItem('teryxChargeLimit',v);if(opt.onChargeLimit)opt.onChargeLimit(v);return true;};
  F('mlim').oninput=e=>F('mlimv').textContent=e.target.value+' %';
  F('mlim').onchange=e=>{if(!setLimit(+e.target.value)){F('mlim').value=cs.lim||80;F('mlimv').textContent=(cs.lim||80)+' %';}};
  F('mamp').onchange=e=>{const v=+e.target.value;if(v>0&&confirm('Set charge amps limit to '+v+' A?')){cs.amps=v;if(opt.onAmpsLimit)opt.onAmpsLimit(v);}};
  F('msl').value=cs.sleepS;F('msl').onchange=e=>{const v=Math.max(10,+e.target.value||120);cs.sleepS=v;F('cslp').value=v;localStorage.setItem('teryxDashSleepS',v);};
  // vehicle settings (sim/vehicle.js VEH_SETS when loaded; saved in localStorage teryxVehSettings, which the simulator reads)
  const VS=typeof VEH_SETS!=='undefined'?VEH_SETS:null,VD=typeof VEH_DEF!=='undefined'?VEH_DEF:null;
  const veh=opt.veh||(VS&&VD?{get:()=>Object.assign({},VD,vehMigrate(JSON.parse(localStorage.getItem('teryxVehSettings')||'{}'))),set:(k,v)=>{const o=JSON.parse(localStorage.getItem('teryxVehSettings')||'{}');o[k]=v;localStorage.setItem('teryxVehSettings',JSON.stringify(o));}}:null);
  function vehBuild(){const h=F('mveh');if(!veh||!VS){h.innerHTML='<div class="k">Vehicle model not loaded on this screen.</div>';return;}const P=veh.get();
    h.innerHTML='<table class="mt"><tr><th>Setting</th><th>Value</th><th>Status</th></tr>'+VS.filter(r=>r[0]!=='gradePct').map(([k,n,u,t,mn,mx,st])=>`<tr><td>${n}</td><td><input type="number" data-vk="${k}" min="${mn}" max="${mx}" step="${st}" value="${typeof P[k]==='boolean'?+P[k]:P[k]}"> ${u}</td><td class="note">${t}</td></tr>`).join('')+'</table>'+
      (opt.veh?'':'<div class="k">Saved on this device for the simulator. The real Kelly config is not read back.</div>');
    h.querySelectorAll('[data-vk]').forEach(e=>e.onchange=()=>{const k=e.dataset.vk,row=VS.find(r=>r[0]===k),v=+e.value,P2=veh.get();
      if(!confirm(`Change ${row[1]} from ${P2[k]} to ${v} ${row[2]}?`)){e.value=typeof P2[k]==='boolean'?+P2[k]:P2[k];return;}veh.set(k,typeof VD[k]==='boolean'?!!v:v);});}
  vehBuild();
  // sim quick panel: only when the host passes opt.simPanel (inside the sim, or dash.html?src=sim)
  if(sim){const send=(c,v)=>sim.send(c,v);let drag={};
    F('simcol').onclick=()=>{const c=F('simp').classList.toggle('col');localStorage.setItem('teryxSimPanelCol',c?'1':'0');F('simcol').textContent=c?'Show ▾':'Hide ▴';};
    D.querySelectorAll('[data-sv]').forEach(e=>{const k=e.dataset.sv;e.oninput=()=>{drag[k]=performance.now();F('sv_'+k).textContent=e.value+' %';send(k,+e.value);};});
    D.querySelectorAll('[data-sg]').forEach(b=>b.onclick=()=>{const g=b.dataset.sg,I=(lastSim&&lastSim.simInfo)||{},cur=I.gear;
      if(cur==='KID'&&g!=='KID'){TeryxPad.open({title:'Enter Kid PIN to leave Kid',note:'Parked only',onEnter:pin=>{send('gear',g+':'+pin);return new Promise(res=>setTimeout(()=>{const J=(lastSim&&lastSim.simInfo)||{};res(J.gear==='KID'?(J.modeMsg&&!/PIN/i.test(J.modeMsg)?J.modeMsg:'Wrong PIN — try again'):'');},450));}});return;}
      send('gear',g);});
    D.querySelectorAll('[data-sc]').forEach(b=>b.onclick=()=>{const k=b.dataset.sc;if(k==='wake')wake('sim Wake button');if(k==='reset'&&!confirm('Reset the simulator? Cells, faults and the log go back to start.'))return;send(k,k==='cycle'?simCyc:undefined);});
    var lastSim=null,simCyc=null,simPaint=s=>{lastSim=s;const Dr=(s&&s.drive)||{},B=(s&&s.bms)||{},I=(s&&s.simInfo)||{},n=performance.now();
      [['thr',Dr.throttlePct],['brk',Dr.brakePct],['grade',I.gradePct]].forEach(([k,v])=>{const e=D.querySelector(`[data-sv="${k}"]`);if(num(v)&&!(n-(drag[k]||0)<600)){e.value=v;F('sv_'+k).textContent=(+v).toFixed(k==='grade'?1:0)+' %';}});
      D.querySelectorAll('[data-sg]').forEach(b=>b.classList.toggle('on',(I.gear||Dr.gear)===b.dataset.sg));
      if(F('spark')){const pk=!!I.parkBrake;F('spark').textContent=pk?'Ⓟ PARK BRAKE SET · tap to release':'Park brake released · tap to set';F('spark').classList.toggle('red',pk);F('spark').classList.toggle('on',!pk);
        F('sneut').textContent=I.neutral?'F/N/R: Neutral · tap for drive':'F/N/R: Drive · tap for neutral';F('sneut').classList.toggle('on',!I.neutral);}
      if(F('skey')){F('skey').textContent=I.key?'Key: ON · tap for off':'Key: OFF · tap for on';F('skey').classList.toggle('on',!!I.key);F('splug').textContent=I.plug?'Unplug':'Plug in';F('splug').classList.toggle('on',!!I.plug);}
      const cy=I.cycles||[];if(F('scyc').childElementCount!==cy.length){F('scyc').innerHTML=cy.map(([k,t])=>`<button data-cy="${k}">${t}</button>`).join('');F('scyc').querySelectorAll('[data-cy]').forEach(b=>b.onclick=()=>{simCyc=b.dataset.cy;send('cyclesel',simCyc);});}
      F('scyc').querySelectorAll('[data-cy]').forEach(b=>b.classList.toggle('on',b.dataset.cy===I.cycleSel));
      F('scgo').textContent=I.cycleOn?'Stop cycle':'Start cycle';F('scgo').className=I.cycleOn?'red':'go';F('scst').textContent=I.cycleTxt||'';
      F('s20').classList.toggle('on',I.speed===20);F('s20').textContent=I.speed===20?'20× speed (on)':'20× speed';
      D.querySelectorAll('[data-sc="weld"]').forEach(b=>b.classList.toggle('on',!!I.weld));
      const kw=num(B.packV)&&num(B.packA)?B.packV*B.packA/1000:null,f=(v,d)=>num(v)?v.toFixed(d):'—';
      F('sro').innerHTML=s?`<div><span>speed</span> ${f(Dr.speedMph,1)} mph</div><div><span>RPM</span> ${f(Dr.rpm,0)}</div><div><span>pack</span> ${f(B.packA,0)} A</div><div><span>power</span> ${f(kw,1)} kW</div><div><span>regen</span> ${f(Dr.regenA,0)} A${Dr.regenWhy?' · '+Dr.regenWhy:''}</div><div><span>power allowed</span> ${f(Dr.powerAllowedPct,0)} % · ${Dr.powerWhy||'full power'}</div>`:'<div>no sim data yet</div>';};
  }
  const tr=(rows)=>rows.map(([k,v,t])=>`<div class="cell"><div class="k">${k}${t?` <span class="tag">${t}</span>`:''}</div><div class="v${v==null?' na':''}">${v==null?'—':v}</div></div>`).join('');
  const f1=(v,d,u)=>num(v)?v.toFixed(d)+(u?'<span class="u">'+u+'</span>':''):null;
  function curveSvg(c){const W=300,H=150,x0=c.x0,x1=c.x1,X=x=>20+(x-x0)/(x1-x0)*(W-30),Y=y=>H-25-(y/100)*(H-45);
    let p=c.pts.map(([x,y],i)=>(i?'L':'M')+X(x).toFixed(1)+' '+Y(y).toFixed(1)).join(' ');
    const nowY=num(c.now)?c.pts.reduce((a,q)=>Math.abs(q[0]-c.now)<Math.abs(a[0]-c.now)?q:a)[1]:null;
    return `<svg viewBox="0 0 ${W} ${H}" class="${c.act?'act':''}"><text x="12" y="20" fill="#f5f5f7" font-size="18">${c.name}</text><path d="${p}" stroke="#f5f5f7" stroke-width="2" fill="none"/>
      <text x="${W-8}" y="${H-6}" fill="#a1a1a8" font-size="18" text-anchor="end">${c.unit}</text>${num(c.now)?`<circle cx="${X(Math.max(x0,Math.min(x1,c.now)))}" cy="${Y(nowY)}" r="6" fill="#d6a33a"/>`:''}</svg>`;}
  function menuPaint(){(split.on?[split.L,split.R]:F('menu').classList.contains('on')?[menuSec]:[]).forEach(k=>{if(k!=='home')menuPaint1(k);});}
  function menuPaint1(menuSec){const s=lastS,live=!!s&&!lastMeta.stale,B=(live&&s.bms)||{},Dr=(live&&s.drive)||{},Ch=(live&&s.charge)||{},Co=(live&&s.coolant)||{},A=(live&&s.aux12v)||{},L=A.loads||{};
    if(menuSec==='batt'){const st=Array.isArray(B.strings)?B.strings:null,all=st?st.flat():[];
      const mn=all.length?Math.min(...all):null,mx=all.length?Math.max(...all):null,av=all.length?all.reduce((a,v)=>a+v,0)/all.length:null,sd=all.length?Math.sqrt(all.reduce((a,v)=>a+(v-av)*(v-av),0)/all.length):null;
      F('mbk').innerHTML=tr([['Lowest cell',f1(mn??B.cellMinV,3,'V')],['Highest cell',f1(mx??B.cellMaxV,3,'V')],['Average',f1(av,3,'V')],['Spread',num(mn)?((mx-mn)*1000).toFixed(0)+'<span class="u">mV</span>':null],['Std deviation',num(sd)?(sd*1000).toFixed(1)+'<span class="u">mV</span>':null],
        ['Pack',f1(B.packV,1,'V')],['Temps min / max',num(B.tempMinC)?B.tempMinC.toFixed(0)+' / '+B.tempMaxC.toFixed(0)+'<span class="u">°C</span>':null,B.tempsVerified===false?'UNVERIFIED':''],
        ['State of charge',f1(B.socPct,1,'%'),B.socState||''],['SoC detail',B.socWhy||null],['Used since anchor',f1(B.socUsedAh,1,'Ah')],['Capacity',f1(B.capAh,0,'Ah'),'4 strings SOURCED (Adam: 36S × 4P) × 45 Ah/group '+(B.capAssumed?'ASSUMED':'')]]);
      F('mbcap').textContent=st?`${st.length} strings × ${st[0].length} cells · red = more than 10 mV below average, green = above · outlined = min / max`:'';
      F('mcells').innerHTML=st?st.map((r,i)=>`<div class="k" style="margin:8px 0 4px">String ${i+1}</div><div class="cg">${r.map(v=>`<i class="${v===mn?'mn ':''}${v===mx?'mx ':''}${v<av-0.01?'lo':v>av+0.01?'hi':''}">${v.toFixed(3)}</i>`).join('')}</div>`).join(''):'<div class="na k">Per-cell voltages are not in this data source</div>';}
    else if(menuSec==='chg'){F('mck').innerHTML=tr([['Plugged in',live?(Ch.plugged?'yes':'no'):null],['Current',f1(Ch.chargeA,1,'A')],['Voltage',f1(Ch.chargeV,1,'V')],['Status',Ch.why||null],['Charger',Ch.charger&&Ch.charger.model||null,Ch.charger&&Ch.charger.assumed?'ASSUMED':''],['Charger max',f1(Ch.chargerMaxA,1,'A'),Ch.charger&&Ch.charger.assumed?'ASSUMED':'']]);
      if(document.activeElement!==F('mlim')){const l=cs.lim||Ch.limitPct||80;F('mlim').value=l;F('mlimv').textContent=l+' %';}if(document.activeElement!==F('mamp')&&num(Ch.ampsLimit))F('mamp').value=Ch.ampsLimit;F('mampN').textContent=opt.onAmpsLimit?'':'display only: no output wired';}
    else if(menuSec==='drive'){const Lm=Dr.limits||{};
      F('mdk').innerHTML=tr([['Power allowed',f1(Dr.powerAllowedPct,0,'%')],['Limited by',live?(Dr.powerWhy||'nothing: full power'):null],['Regen',live?(Dr.regenOk?'allowed':'off'+(Dr.regenOkWhy?' · '+Dr.regenOkWhy:'')):null],['Gear / speed mode',Dr.gear||null],
        ['Kelly battery limit',f1(Lm.battA,0,'A'),'SOURCED: Adam'],['Kelly phase limit',f1(Lm.phaseA,0,'A'),'SOURCED: Adam'],['Max regen',f1(Lm.maxRegenA,0,'A'),'PROPOSED'],['BMS trip',num(Lm.overA)?Lm.overA+' A · '+Lm.overMs+' ms':null],['Instant trip',f1(Lm.instantTripA,0,'A')],['Over-temp open',f1(Lm.overTempC,0,'°C')]]);
      F('mds').innerHTML=tr([['Limp (OUT3 pin 20)',live&&Dr.limp!=null?(Dr.limp?'ON':'off'):null],['Derate PWM (OUT5 pin 2)',num(Dr.powerAllowedPct)?Math.round(Dr.powerAllowedPct*2.55)+' / 255':null],['Regen allowed (OUT6 pin 3)',live&&Dr.regenOk!=null?(Dr.regenOk?'HIGH':'LOW'):null],['Controller temp',f1(Dr.controllerTempC,0,'°C')],['Throttle',f1(Dr.throttlePct,0,'%')],['Brake',f1(Dr.brakePct,0,'%')]]);
      F('mcrv').innerHTML=Array.isArray(Dr.curves)&&Dr.curves.length?Dr.curves.map(curveSvg).join(''):'<div class="na k">Cutback curves are not in this data source</div>';}
    else if(menuSec==='flt'){const fl=(live&&s.faults)||[];const hasD=live&&Dr.lastDerate!=null;F('mfd').innerHTML=tr([['Derate now',hasD?(Dr.derateNow?Dr.derateNow+' · '+Math.round(Dr.powerAllowedPct)+' %':'none'):null],['Last derate',hasD?(Dr.lastDerate?Dr.lastDerate+' · lowest '+Math.round(Dr.lastDerateMinPct)+' %':'none this boot'):null],['Derates this boot',hasD&&num(Dr.derateEvents)?Dr.derateEvents:null]]);F('mfa').innerHTML=fl.length?fl.map(f=>`<div class="f">${f.level==='fault'?'FAULT':f.level} · ${f.text}</div>`).join(''):'<div>No active faults</div>';
      F('mfh').innerHTML=fhist.length?fhist.slice(-30).reverse().map(h=>`<div class="${h.on?'f':''}">${new Date(h.t).toLocaleTimeString()} · ${h.on?'set':'cleared'} · ${h.text}</div>`).join(''):'<div>Nothing yet</div>';
      const lg=(live&&Array.isArray(s.log))?s.log:null,cl=CT.S.log.slice(-15).reverse();F('mflT').textContent=lg?'from '+(s.src==='sim'?'the simulator':'the hub'):'controls only';
      F('mfl').innerHTML=(lg?lg.map(e=>`<div class="${/FAULT|latched|WELD|Lockout|fail/i.test(e.text)?'f':''}">${e.t} · ${String(e.text).replace(/</g,'&lt;')}</div>`).join(''):'')+cl.map(t=>`<div>controls · ${t}</div>`).join('')||'<div>Empty</div>';}
    else if(menuSec==='cool'){F('mco').innerHTML=tr([['Pump',live&&(L.pump!=null||Co.pumpOn!=null)?((Co.pumpOn??L.pump)?'ON':'off'):null,'cmd only'],['Fan',live&&L.fan!=null?(L.fan?'ON':'off'):null,fanOv?'OVERRIDE':''],['Kelly temp',f1(Dr.controllerTempC,0,'°C')],['Motor temp',f1(Dr.motorTempC,0,'°C')],['Coolant temp',f1(Co.tempC,0,'°C'),'no sensor yet'],['Coolant flow',Co.flowOk==null?null:(Co.flowOk?'ok':'LOW')],['Pack max temp',f1(B.tempMaxC,0,'°C'),B.tempsVerified===false?'UNVERIFIED':'']]);
      F('mfan').textContent=fanOv?'Fan override: ON':'Fan override: off';F('mfan').classList.toggle('on',fanOv);F('mfanN').textContent=opt.onFan?'sim':'display only, relay PROPOSED';}
    else if(menuSec==='diag'){F('mdg').innerHTML=tr([['Data source',!s?'none':lastMeta.stale?'stale':(s.src==='sim'?'simulator':'live')],['Schema',s?String(s.schema):null],['Uptime',s&&num(s.uptimeSec)?s.uptimeSec+' s':null],['Contactors',live&&s.contactors?`B− ${s.contactors.mainNegative} · pre ${s.contactors.precharge} · B+ ${s.contactors.mainPositive}`:null]]);
      F('msrc').innerHTML='<tr><th>Value</th><th>Source</th></tr>'+[['Speed',Dr.speedSource||'—'],['RPM',Dr.rpmSource||(num(Dr.speedMph)?'estimate from speed':'—')],['Pack current',num(B.packA)?(B.currentVerified===false?'LEM, uncalibrated':'LEM'):'—'],['SoC',B.socState?B.socState+' · '+(B.socWhy||''):'—'],['Temps',B.tempsVerified===false?'BICM thermistors, UNVERIFIED':num(B.tempMaxC)?'BICM thermistors':'—'],['Range',`SoC × ${num((s&&s.energy||{}).packKWh)?'pack kWh':'ASSUMED '+ASSUMED_PACK_KWH.toFixed(1)+' kWh'} ÷ recent or ASSUMED ${ASSUMED_WHMI} Wh/mi`]].map(r=>`<tr><td>${r[0]}</td><td>${r[1]}</td></tr>`).join('');
      if(!F('mpin').childElementCount)F('mpin').innerHTML='<tr><th>Signal</th><th>Source / wiring</th><th>Status</th></tr>'+IOMAP.map(r=>`<tr><td>${r[0]}</td><td>${r[1]}</td><td class="note">${r[2]}</td></tr>`).join('');}
    else if(menuSec==='about'){F('mab').innerHTML=tr([['Firmware',s&&s.fw?s.fw:null,s&&s.fw?'':'not reported'],['Dash build',DASH_BUILD],['Units','mph · °C · V · A · kW · Wh/mi'],['Status schema','v1'],['Wh/mi fallback',ASSUMED_WHMI+'','PROPOSED'],['Pack layout','36S × 4P · ≈133 V nominal · 147.6 V full · 118.8 V low cut','SOURCED: Adam · 3.7 V nominal ASSUMED'],['Pack energy',ASSUMED_PACK_KWH.toFixed(1)+' kWh','180 Ah = 4 × 45 Ah ASSUMED/group'],['Pack modules','2014 Volt Gen1, 13×12S + 4×6S = 180S; 144S used, 36S spare','SOURCED: cash ledger'],['BICMs','8 = 2 per string (24S M2/M3 + M4 18S) on 4 CAN buses; 6 owned, 2 × M4 to buy','SOURCED: Adam · P/N UNVERIFIED'],['Charger','Elcon HK-LF-144-46 · CAN3865 · SN23100137B','SOURCED: label photo'],['Charger output','100–202 V DC · 46 A max · ~6.6 kW @ 144 V','SOURCED: label · CAN UNVERIFIED'],['Charger input','90–264 VAC · 45–65 Hz · 32 A','SOURCED: label'],['DC-DC','Elcon TDC-JH-144-12','SOURCED: on the shelf (crew 2026-10-22); rated output UNVERIFIED'],['HV cable','TEMCo 3/0','SOURCED: cash ledger'],['Precharge resistor','50 Ω 100 W (came with Kelly)','label UNCONFIRMED'],['Motor','Zero Z75-7 / 75-7R IPM','kW/torque/rpm/V MISSING'],['Seat heaters, lights','—','MISSING in ledger'],['RPM per mph',RPM_PER_MPH.toFixed(0),'ESTIMATE']]);}
  }
  function homePaint(s,live){const Dr=(live&&s.drive)||{},r=ctlLast||{},o=r.out||{},L=((live&&s.aux12v)||{}).loads||{};
    const q=(k,t,on,dis)=>{F('q_'+k).textContent=t;const b=D.querySelector(`[data-q="${k}"]`);b.classList.toggle('on',!!on);b.classList.toggle('dis',!!dis);};
    q('split',split.on?'on':'off',split.on);
    q('lights',o.headHigh?'high':o.headLow?'low':'off',o.headLow);
    q('4wd',r.fault4?'FAULT':r.wdShifting?'shifting…':r.wd||'2WD',r.wd==='4WD',!!r.wdWhy);
    q('diff',r.faultD?'FAULT':r.diffShifting?'moving…':r.diff||'off',r.diff&&r.diff!=='off',!!(r.diffWhy&&r.diffWhy[nextDiff[r.diff]||'off']));
    const g=live?Dr.gear:null;q('mode',g?(MODE_NAME[g]||g):'—',false,!opt.onGear);
    const Ch=(live&&s.charge)||{};q('chg',(cs.lim||Ch.limitPct)?'limit '+(cs.lim||Ch.limitPct)+' %':'limit —',false);
    q('fan',L.fan!=null?(L.fan?(fanOv?'override':'on'):'off'):'—',!!L.fan,!opt.onFan);
    const P=Dr.powerAllowedPct,lim=F('lim');if(num(P)){F('limP').textContent=P.toFixed(0)+' %';F('limW').textContent=P>=99.5?'full power':'limited by '+(Dr.powerWhy||(Dr.limp?'limp':'BMS'));lim.classList.toggle('cut',P<99.5&&P>0);lim.classList.toggle('off',P<=0);}
    else{F('limP').innerHTML='<span class="na">—</span>';F('limW').textContent='';lim.classList.remove('cut','off');}
    if(r.msg&&r.msg!==toastMsg&&performance.now()-toastT>300)toast(r.msg);if(performance.now()-toastT>6000&&F('toast').textContent)F('toast').textContent='';
    if(sim)simPaint(s);}
  function update(s,meta){
    {const I=(s&&s.simInfo)||{},B=(s&&s.bms)||{};const k=I.key!=null?!!I.key:(s&&s.keyOn!=null?!!s.keyOn:/Closed|Precharge|MainClose/.test(String(B.state||'')));
     TeryxLock.set({keyOn:k,phone:I.phoneKey!=null?!!I.phoneKey:false});
     if(!opt.hostLock&&!TeryxLock.verify){TeryxLock.verify=p=>{try{const h=(JSON.parse(localStorage.getItem('teryx.modes.v1')||'{}').hash)>>>0;return !!h&&typeof vehHashPin==='function'&&vehHashPin(p)===h;}catch(_){return false;}};TeryxLock.hasPin=()=>{try{return !!(JSON.parse(localStorage.getItem('teryx.modes.v1')||'{}').hash>>>0);}catch(_){return false;}};}}
    meta=meta||{};const now=performance.now(),dt=lastT?Math.min(2,(now-lastT)/1000):0;lastT=now;
    const live=!!s&&!meta.stale,B=(s&&s.bms)||{},Dr=(s&&s.drive)||{},E=(s&&s.energy)||{},C=(s&&s.contactors)||{},A=(s&&s.aux12v)||{},L=A.loads||{};
    const packV=live&&B.wired&&num(B.packV)?B.packV:null,packA=live&&B.wired&&num(B.packA)?B.packA:null;
    const kw=packV!=null&&packA!=null?packV*packA/1000:null;
    const mph=live&&Dr.wired!==false&&num(Dr.speedMph)?Math.abs(Dr.speedMph):null;
    F('srcb').textContent=!s?'no data source':meta.stale?'data stale':(s.src==='sim'?'simulator data':'live');
    // power
    put('kw',kw,kw!=null?`${kw.toFixed(0)}<span class="u">kW</span>`:'');
    const pb=F('pbar');if(kw==null){pb.style.width=0;}else if(kw>=0){pb.style.left='20%';pb.style.width=Math.min(80,kw/100*80)+'%';pb.style.background='var(--tx)';}else{const w=Math.min(20,-kw/20*20);pb.style.left=(20-w)+'%';pb.style.width=w+'%';pb.style.background='var(--grn)';}
    {const ra=live&&num(Dr.regenA)&&Dr.regenA>0.5?Dr.regenA:null;F('rgl').innerHTML=ra!=null?`<b style="color:var(--grn)">regen ${ra.toFixed(0)} A</b>`:(live&&Dr.regenWhy?'regen · '+Dr.regenWhy:'regen');}
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
    let rpm=live&&Dr.wired&&num(Dr.rpm)?Dr.rpm:null,rt=rpm!=null&&Dr.rpmSource?Dr.rpmSource:'';if(rpm==null&&mph!=null){rpm=mph*RPM_PER_MPH;rt='estimate from speed';}
    put('rpm',rpm,rpm!=null?rpm.toFixed(0):'');tag('rpmTag',rt);
    // gear
    const gr=live?(Dr.gear||null):null,closed=C.mainPositive==='closed'&&C.mainNegative==='closed';
    const pkd=!!(Dr.park&&Dr.park.parked),neu=Dr.fnr==='N';F('gear').innerHTML=['P','R','N','L','M','H'].map(x=>`<span class="${(x==='P'&&live&&(!closed||pkd))||(x==='N'&&live&&neu&&!pkd)||(!neu&&!pkd&&gr===x)?'on':''}">${x}</span>`).join('')+(gr&&!neu&&!pkd&&!['P','R','N','L','M','H'].includes(gr)?`<span class="on">${MODE_NAME[gr]||gr}</span>`:'')+(gr==null&&live?'<span class="na" style="font-size:16px"> gear not wired</span>':'');
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
    chargeScreen(s,live,B,C,dt);
    lastDrv={mph:mph,thr:live&&num(Dr.throttlePct)?Dr.throttlePct:0,brake:live&&num(Dr.brakePct)?Dr.brakePct:0,gear:live?(Dr.gear||null):null};
    const cr=ctlPaint(),co=cr.out;
    // tell-tales
    const fl=(s&&s.faults)||[],faultTxt=fl.filter(f=>f.level==='fault').map(f=>f.text);
    const on={fault:live&&faultTxt.length>0,hv:live&&C.wired!==false&&!closed,batt:cmn!=null&&cmn<3.2,temp:tP!=null&&tP>=50,limp:live&&(!!Dr.limp||(num(Dr.powerAllowedPct)&&Dr.powerAllowedPct<100)),
      regen:live&&Dr.regenOk===false&&closed,imb:live&&!!B.lemImbalance,link:!live,lights:!!L.lights||co.headLow,fan:!!L.fan,seat:!!L.seatHeat,pump:!!L.pump,coolant:live&&((s.coolant||{}).flowOk===false||num((s.coolant||{}).tempC)&&s.coolant.tempC>=60),turnL:co.turnL,turnR:co.turnR,hazard:CT.S.hazard,high:co.headHigh,d4wd:cr.wd==='4WD',diff:cr.diff!=='off'||!!cr.faultD,alarm:!!L.alarm||(live&&faultTxt.length>0)};
    D.querySelectorAll('.tt i').forEach(i=>i.classList.toggle('on',!!on[i.dataset.k]));
    curFaultKey=faultTxt.join('|');if(!curFaultKey)ack=null;
    F('ban').classList.toggle('on',!!curFaultKey&&ack!==curFaultKey);F('banT').textContent='⚠ '+faultTxt.join(' · ');
    if(curFaultKey!==prevFault){const a=prevFault?prevFault.split('|'):[],b=faultTxt;b.filter(x=>!a.includes(x)).forEach(x=>fhist.push({t:Date.now(),text:x,on:true}));a.filter(x=>!b.includes(x)).forEach(x=>fhist.push({t:Date.now(),text:x,on:false}));prevFault=curFaultKey;}
    lastS=s;lastMeta=meta;homePaint(s,live);if((split.on||F('menu').classList.contains('on'))&&++mp%3===0)menuPaint();
  }
  update(null);if(split.on)applySplit();
  return {toast,update,controls:CT,root:D,setStyle,F,wake,openMenu,setSplit,getSplit:()=>Object.assign({},split),isAsleep:()=>cs.asleep};
}
g.TeryxDash={TeryxPad,TeryxLock,create,IOMAP,SEC:TeryxDash_SEC,ASSUMED_WHMI,ASSUMED_PACK_KWH,RPM_PER_MPH};
})(window);
