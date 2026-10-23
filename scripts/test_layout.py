# python3 scripts/test_layout.py — Build 23 top-down Teryx layout page checks (Playwright + Chrome).
# 1) check_layout.py: the repo layout PASSES; temp copies with a bogus PARTS id / PINMAP wire id / bad label FAIL.
# 2) tools/layout.html loads with no JS errors; the in-page check (errors, warnings), every run length and the totals
#    equal check_layout.py --json exactly; chips PROPOSED + lengths ESTIMATED.
# 3) rule triggers (same mutation in python and in the page, same warning text): F-MAIN moved from the DC-DC out, F-BATT moved from battery +, metal-mount flag
#    cleared, fuse marker changed, an MSD block added, P01 stretched over 5 ft, an LV run crossing HV at 45°, HV block in the cab.
# 4) scale to a measured reference: entering a wheelbase changes k, the chip flips to SCALED, lengths scale by k, Clear restores.
# 5) drag a block: it snaps to 0.1 in, its children move with it and run ends follow; edits persist across reload;
#    Export JSON of an unedited layout == the repo file; cut-list CSV has every run + totals; Reset (confirm) drops edits.
# 6) Window select (L->R, blocks fully inside) vs crossing (R->L, touching), then stretch moves the picked set.
# 7) iPhone portrait (touch): no sideways scroll, every control >= 44 px.
# Optional: SHOT=/path.png saves a desktop screenshot; ISHOT=/path.png an iPhone one.
import os, sys, json, copy, pathlib, subprocess, tempfile, importlib.util
from playwright.sync_api import sync_playwright
repo = pathlib.Path(__file__).resolve().parent.parent
LF = repo / 'docs' / 'layout' / 'teryx.layout.json'
REPO = json.loads(LF.read_text())
spec = importlib.util.spec_from_file_location('cl', repo / 'scripts' / 'check_layout.py'); CL = importlib.util.module_from_spec(spec); spec.loader.exec_module(CL)
PARTS, WIRES = CL.load_refs()
n = f = 0
def ok(c, m):
    global n, f
    n += 1; f += (not c); print(('PASS ' if c else 'FAIL ') + 'layout ' + m, flush=True)
def cl(*a):
    r = subprocess.run([sys.executable, str(repo / 'scripts' / 'check_layout.py'), *map(str, a)], capture_output=True, text=True); return r.returncode, r.stdout + r.stderr
# ---- 1) python check
rc, out = cl(); ok(rc == 0 and 'PASS' in out, 'check_layout.py: repo layout passes ' + out.strip().splitlines()[-1][:120])
td = pathlib.Path(tempfile.mkdtemp())
def variant(name, fn):
    L = copy.deepcopy(REPO); fn(L); p = td / (name + '.layout.json'); p.write_text(json.dumps(L, indent=1)); return cl(p)
def b1(L): L['blocks'][0]['part'] = 'bogus_part_xyz'
def b2(L): L['runs'][0]['wires'] = ['ZZ99']
def b3(L): L['blocks'][1]['label'] = 'GUESS'
def b4(L): L['runs'][1]['to'] = 'nope.x'
for nm, fn, s in [('bogus part', b1, 'bogus_part_xyz'), ('bogus wire', b2, 'ZZ99'), ('bad label', b3, 'GUESS'), ('bad terminal', b4, 'nope.x')]:
    rc, out = variant(nm.replace(' ', '_'), fn); ok(rc != 0 and s in out, f'check_layout.py: a {nm} fails the check')
PY = CL.check(REPO, PARTS, WIRES)
ok(not PY[0], 'repo layout: 0 errors'); ok(all(r['type'] in ('HV', 'LV') and r.get('gauge') for r in REPO['runs']), 'every run is typed HV/LV and has a gauge')
ok(all(b['label'] in ('PROPOSED', 'ASSUMED') for b in REPO['blocks']), 'every block position is PROPOSED or ASSUMED')
ok(all(d['label'] for d in REPO['dims'].values()) and REPO['scale']['measured_in'] is None, 'every dimension labelled; scale not yet measured (OPEN)')
ok(not any(b.get('part') == 'msd' for b in REPO['blocks']), 'no MSD block in the build')
def mut_py(code):
    L = copy.deepcopy(REPO); exec(code, {'L': L, 'B': {b['id']: b for b in L['blocks']}, 'R': {r['id']: r for r in L['runs']}}); return CL.check(L, PARTS, WIRES)[1]
# mutations written once, run in both python (exec) and JS (same syntax subset)
MUTS = [
    ('F-MAIN 10 in from the DC-DC out', "B['fmain']['x'] = B['fmain']['x'] + 10", 'F-MAIN: fmain is'),
    ('F-BATT 10 in from the battery +', "B['fbatt']['y'] = B['fbatt']['y'] + 10", 'F-BATT: fbatt is'),
    ('P01 over 5 ft (DC-DC moved away)', "R['P01']['pts'] = [[88.6, 38.75], [20, 38.75], [20, 39]]", 'P01: run P01 is'),
    ('precharge resistor without the metal-mount flag', "B['hv_pre']['flags'] = {}", 'precharge resistor hv_pre has no metal-mount flag'),
    ('string fuses without the marker', "B['hv_fuses']['marker'] = 'under the seat'", "string fuses hv_fuses must carry the marker"),
    ('an MSD block', "L['blocks'].append({'id': 'msd1', 'name': 'MSD', 'part': 'msd', 'v': 'HV', 'x': 95, 'y': 30, 'w': 2, 'h': 2, 'label': 'PROPOSED'})", 'MSD: block msd1'),
    ('P01 over 5 ft', "R['P01']['pts'] = [[88.6, 38.75], [30, 38.75], [30, 42], [88.6, 42]]", 'P01: run P01 is'),
    ('an LV run crossing HV off 90°', "R['MAIN']['pts'] = [[86.8, 34], [94.8, 34]]; R['KEY']['pts'] = [[43.3, 34], [88.8, 30], [92.8, 38], [43.3, 31]]", 'HV/LV crossing: MAIN and KEY cross at'),
    ('HV block in the cab', "B['kelly']['x'] = 60; B['kelly']['y'] = 30", 'HV in cab: block kelly'),
]
# ---- 2..7) page
with sync_playwright() as p:
    b = p.chromium.launch(executable_path=os.environ.get('CHROME', '/usr/bin/google-chrome'), args=['--no-sandbox'])
    ctx = b.new_context(viewport={'width': 1500, 'height': 950}, accept_downloads=True); pg = ctx.new_page(); errs = []
    pg.on('pageerror', lambda e: errs.append(str(e))); pg.on('console', lambda m: m.type == 'error' and errs.append(m.text))
    url = (repo / 'tools' / 'layout.html').as_uri()
    pg.goto(url); pg.wait_for_timeout(500); J = pg.evaluate
    if os.environ.get('SHOT'): pg.screenshot(path=os.environ['SHOT'])
    ok(not errs, 'layout.html loads with no JS errors ' + repr(errs[:3]))
    ok(J("document.querySelector('#tnav a.on').textContent") == 'Layout', 'Layout tab is active in the top nav')
    r = J('LY.check()'); ok(r['E'] == PY[0] and r['W'] == PY[1], f'in-page check == check_layout.py ({len(r["W"])} warnings, same text)')
    PJ = json.loads(cl('--json')[1]); jl = J('LY.lengths()')
    ok(len(jl) == len(PJ['lengths']) and all(a['id'] == c['id'] and abs(a['len'] - c['len']) < 1e-9 and abs(a['total'] - c['total']) < 1e-9 for a, c in zip(jl, PJ['lengths'])), f'every run length + cut length matches python ({len(jl)} runs)')
    jt = J('LY.totals()'); ok(all(abs(jt[t]['cut'] - PJ['totals'][t]['cut']) < 1e-9 for t in ('HV', 'LV')), 'HV / LV totals match python')
    ok('PROPOSED' in J("document.getElementById('st').textContent") and J("document.getElementById('sc').textContent") == 'lengths ESTIMATED', 'chips: PROPOSED, lengths ESTIMATED')
    ok(J("document.getElementById('rtab').textContent").count('EST') >= len(jl) or 'ESTIMATED' in J("document.getElementById('runs').textContent"), 'run table marks lengths ESTIMATED')
    # ---- 3) rule triggers
    for nm, code, want in MUTS:
        pw = mut_py(code)
        jw = J('c=>{const L=JSON.parse(JSON.stringify(LY.L)),B=Object.fromEntries(L.blocks.map(b=>[b.id,b])),R=Object.fromEntries(L.runs.map(r=>[r.id,r]));L.blocks.append=L.blocks.push;(new Function("L","B","R",c))(L,B,R);delete L.blocks.append;return LY.checkL(L).W}', code)
        ok(any(w.startswith(want) for w in pw) and jw == pw, f'rule: {nm} -> "{next((w for w in pw if w.startswith(want)), "MISSING")[:90]}" (python == page)')
    # ---- 4) scale
    k0 = J('LY.k()'); wb = REPO['dims']['wheelbase']['v']
    pg.select_option('#sref', 'wheelbase'); pg.fill('#sval', str(round(wb * 1.05, 2))); pg.select_option('#sunit', 'in'); pg.click('#sapp'); pg.wait_for_timeout(200)
    k1 = J('LY.k()'); jl1 = J('LY.lengths()')
    ok(k0 == 1 and abs(k1 - 1.05) < 1e-3, f'scale: wheelbase {wb * 1.05:.2f} in -> k {k1:.4f}')
    ok(J("document.getElementById('sc').textContent") == 'lengths SCALED' and J('LY.L.scale.label') == 'SOURCED', 'scale: chip flips to lengths SCALED, scale labelled SOURCED (tape)')
    ok(all(abs(a['len'] - c['len'] * k1) < 1e-6 for a, c in zip(jl1, jl)), 'scale: every length multiplied by k')
    pg.fill('#sval', str(round(wb * 2.54, 2))); pg.select_option('#sunit', 'cm'); pg.click('#sapp'); pg.wait_for_timeout(200)
    ok(abs(J('LY.k()') - 1) < 1e-3, 'scale: cm input converts (k back to ~1)')
    pg.click('#sclr'); pg.wait_for_timeout(200); ok(J('LY.k()') == 1 and J("document.getElementById('sc').textContent") == 'lengths ESTIMATED', 'scale: Clear restores ESTIMATED')
    J('localStorage.removeItem(LY.LSK)'); pg.reload(); pg.wait_for_timeout(400)
    # ---- 5) drag a block with children
    def scr(x, y): return J('([x,y])=>{const m=document.getElementById("sv").getScreenCTM();return [m.a*x+m.e,m.d*y+m.f]}', [x, y])
    def grab(bid):  # a point on screen whose topmost element belongs to block bid
        return J('''id=>{const b=LY.L.blocks.find(b=>b.id===id),m=document.getElementById("sv").getScreenCTM();
          for(let i=1;i<20;i++)for(let j=1;j<20;j++){const x=b.x+b.w*i/20,y=b.y+b.h*j/20,sx=m.a*x+m.e,sy=m.d*y+m.f,e=document.elementFromPoint(sx,sy),g=e&&e.closest("[data-blk]");
          if(g&&g.dataset.blk===id&&!e.closest("[data-run]"))return [sx,sy]}return null}''', bid)
    hb0 = J('JSON.stringify(LY.L.blocks.filter(b=>b.id==="hv_box"||b.in==="hv_box").map(b=>[b.id,b.x,b.y]))')
    me0 = J('LY.termXY("hv_cont.out")'); g0 = grab('hv_box'); ok(g0 is not None, 'hv_box has a clickable spot')
    sc = J('document.getElementById("sv").getScreenCTM().a')
    pg.mouse.move(*g0); pg.mouse.down(); pg.mouse.move(g0[0] + 1.53 * sc, g0[1] + 0.5 * sc, steps=6); pg.mouse.move(g0[0] + 2.03 * sc, g0[1] + 1.02 * sc, steps=4); pg.mouse.up(); pg.wait_for_timeout(200)
    hb1 = json.loads(J('JSON.stringify(LY.L.blocks.filter(b=>b.id==="hv_box"||b.in==="hv_box").map(b=>[b.id,b.x,b.y]))')); hb0 = json.loads(hb0)
    dx = round(hb1[0][1] - hb0[0][1], 3); dy = round(hb1[0][2] - hb0[0][2], 3)
    ok(abs(dx - 2.0) < 0.11 and abs(dy - 1.0) < 0.11 and abs(dx * 10 - round(dx * 10)) < 1e-6, f'drag hv_box: moved ({dx}, {dy}) in, snapped to 0.1 in')
    ok(len(hb1) > 1 and all(abs((a[1] - c[1]) - dx) < 1e-6 and abs((a[2] - c[2]) - dy) < 1e-6 for a, c in zip(hb1, hb0)), f'drag hv_box: its {len(hb1) - 1} child blocks move with it')
    me1 = J('LY.termXY("hv_cont.out")'); ok(abs(me1[0] - me0[0] - dx) < 1e-6 and abs(me1[1] - me0[1] - dy) < 1e-6, 'drag hv_box: the MAIN run end (hv_cont.out) follows')
    pg.mouse.click(*grab('hv_box')); pg.wait_for_timeout(150)
    ok(J('LY.sel&&LY.sel.id') == 'hv_box' and 'hv_box' in J("document.getElementById('selcard')?document.getElementById('selcard').textContent:document.getElementById('selb').textContent"), 'clicking a block selects it and shows its card')
    card = J("document.getElementById('selb').textContent"); pid = next(b['part'] for b in REPO['blocks'] if b['id'] == 'hv_box')
    ok(pid in card, f'block card reads the PARTS id ({pid}) from parts.json')
    pg.reload(); pg.wait_for_timeout(400); ok(J('LY.L.blocks.find(b=>b.id==="hv_box").x') == hb1[0][1] and J("!document.getElementById('lc').hidden"), 'edits persist across reload (localStorage) and show "local edits"')
    pg.on('dialog', lambda d: d.accept()); pg.click('#rst'); pg.wait_for_timeout(300)
    ok(J('LY.L.blocks.find(b=>b.id==="hv_box").x') == hb0[0][1] and J('localStorage.getItem(LY.LSK)') is None, 'Reset (confirm) drops the local edits')
    with pg.expect_download() as d: pg.click('#exp')
    ok(json.loads(pathlib.Path(d.value.path()).read_text()) == REPO and d.value.suggested_filename == 'teryx.layout.json', 'Export JSON of the unedited layout == docs/layout/teryx.layout.json')
    with pg.expect_download() as d: pg.click('#csv')
    rows = pathlib.Path(d.value.path()).read_text().splitlines()
    ok(len(rows) == 1 + len(REPO['runs']) + 2 + 1 and rows[0].startswith('run,name,wires,type,gauge') and any(r.startswith('TOTAL HV') for r in rows) and 'ESTIMATED' in rows[1], f'cut-list CSV: header + {len(REPO["runs"])} runs + HV/LV totals + slack, lengths ESTIMATED')
    # ---- 6) window vs crossing select, stretch
    pg.click('#mBox'); s4 = next(b for b in REPO['blocks'] if b['id'] == 'S4'); kl = next(b for b in REPO['blocks'] if b['id'] == 'kelly')
    a = scr(s4['x'] - 1, s4['y'] - 1); c = scr(s4['x'] + s4['w'] + 1, s4['y'] + s4['h'] + 1)
    pg.mouse.move(*a); pg.mouse.down(); pg.mouse.move(*c, steps=8); pg.mouse.up(); pg.wait_for_timeout(150)
    m = J('LY.ms'); ok(m and m['kind'] == 'window' and 'S4' in m['blocks'], 'Window (left→right) picks the block fully inside ' + repr(m and m['blocks']))
    a = scr(kl['x'] + kl['w'] - 1, kl['y'] - 1); c = scr(kl['x'] + 1, kl['y'] + 1)  # right->left, only touching kelly's corner
    pg.mouse.move(*a); pg.mouse.down(); pg.mouse.move(*c, steps=8); pg.mouse.up(); pg.wait_for_timeout(150)
    m = J('LY.ms'); ok(m and m['kind'] == 'crossing' and 'kelly' in m['blocks'], 'Crossing (right→left) picks a block it only touches ' + repr(m and m['blocks']))
    # pick S4 + kelly together (window over S4, shift-window over kelly) then stretch by dragging kelly
    pg.keyboard.press('Escape')
    a = scr(s4['x'] - 1, s4['y'] - 1); c = scr(s4['x'] + s4['w'] + 1, s4['y'] + s4['h'] + 1); pg.mouse.move(*a); pg.mouse.down(); pg.mouse.move(*c, steps=8); pg.mouse.up()
    a = scr(kl['x'] - 0.5, kl['y'] - 0.5); c = scr(kl['x'] + kl['w'] + 0.5, kl['y'] + kl['h'] + 0.5); pg.keyboard.down('Shift'); pg.mouse.move(*a); pg.mouse.down(); pg.mouse.move(*c, steps=8); pg.mouse.up(); pg.keyboard.up('Shift')
    m = J('LY.ms'); ok(m and {'S4', 'kelly'} <= set(m['blocks']), 'Shift adds a second window pick ' + repr(m and m['blocks']))
    p0 = J('LY.L.blocks.filter(b=>b.id==="S4"||b.id==="kelly").map(b=>[b.id,b.x,b.y])'); g = grab('kelly')
    pg.mouse.move(*g); pg.mouse.down(); pg.mouse.move(g[0] + 3 * sc, g[1], steps=8); pg.mouse.up(); pg.wait_for_timeout(150)
    p1 = J('LY.L.blocks.filter(b=>b.id==="S4"||b.id==="kelly").map(b=>[b.id,b.x,b.y])')
    ok(all(abs((c[1] - a_[1]) - 3) < 0.11 and abs(c[2] - a_[2]) < 0.11 for a_, c in zip(p0, p1)), 'stretch: dragging one picked block moves the whole pick ' + repr(p1))
    pg.keyboard.press('Escape'); ok(J('LY.ms') is None, 'Esc clears the pick')
    J('localStorage.removeItem(LY.LSK)')
    ok(not errs, 'no JS errors during the session ' + repr(errs[:3]))
    ctx.close()
    # ---- 7) iPhone
    ic = b.new_context(viewport={'width': 390, 'height': 844}, device_scale_factor=3, is_mobile=True, has_touch=True); ip = ic.new_page(); ie = []
    ip.on('pageerror', lambda e: ie.append(str(e))); ip.goto(url); ip.wait_for_timeout(600)
    if os.environ.get('ISHOT'): ip.screenshot(path=os.environ['ISHOT'])
    r = ip.evaluate('''()=>({sw:document.documentElement.scrollWidth,small:[...document.querySelectorAll("button,select,input:not([type=hidden])")].filter(e=>{const r=e.getBoundingClientRect();return r.width&&r.height&&getComputedStyle(e).visibility!=="hidden"&&(r.height<43.5||(e.tagName!=="INPUT"&&r.width<43.5))}).map(e=>e.id||e.textContent.trim().slice(0,12))})''')
    ok(not ie and r['sw'] <= 391, f'iPhone portrait: loads, no sideways scroll ({r["sw"]} px)')
    ok(not r['small'], 'iPhone portrait: every control at least 44 px ' + repr(r['small'][:6]))
    ic.close(); b.close()
print(f'{n - f}/{n} layout'); sys.exit(1 if f else 0)
