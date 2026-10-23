# python3 scripts/test_perfboard.py — perf-board designer checks (Playwright + Chrome).
# 1) check_boards.py: the repo layouts PASS; a temp copy with a bogus PARTS id FAILS (and so do an open net and a short).
# 2) tools/perfboard.html?board=coil_driver loads: 15 parts, 23 wires, check chip PASS, PROPOSED chip, notes + off-board 1N4007.
# 3) the in-page check catches a bogus PARTS id (via localStorage edits) and Reset drops it.
# 4) drag a part (snaps to holes), Rotate, edits persist across reload, Export JSON of an unedited board == the repo file.
# 5) wiring.html precharge: clicking the drv node opens perfboard.html?board=coil_driver.
# 6) iPhone portrait (touch): tap selects a part, pinch-free fit, no sideways scroll.
# Optional: SHOT=/path.png saves a screenshot of the coil_driver layout.
import os, sys, json, pathlib, subprocess, tempfile, copy
from playwright.sync_api import sync_playwright
repo = pathlib.Path(__file__).resolve().parent.parent
tools, boards = repo / 'tools', repo / 'docs' / 'boards'
n = f = 0
def ok(c, m):
    global n, f
    n += 1; f += (not c); print(('PASS ' if c else 'FAIL ') + 'perfboard ' + m, flush=True)
def cb(*a):
    r = subprocess.run([sys.executable, str(repo / 'scripts' / 'check_boards.py'), *map(str, a)], capture_output=True, text=True); return r.returncode, r.stdout + r.stderr
REPO = json.loads((boards / 'coil_driver.layout.json').read_text())
# ---- 1) python check
rc, out = cb(); ok(rc == 0 and 'PASS' in out, 'check_boards.py: repo layouts pass')
td = pathlib.Path(tempfile.mkdtemp())
def variant(name, fn):
    L = copy.deepcopy(REPO); fn(L); p = td / (name + '.layout.json'); p.write_text(json.dumps(L, indent=1)); return cb(p)
def bogus(L): L['parts'][0]['part'] = 'bogus_part_xyz'
rc, out = variant('bogus', bogus); ok(rc != 0 and 'bogus_part_xyz' in out, 'check_boards.py: a bogus PARTS id fails the check')
def opn(L): L['wires'] = [w for w in L['wires'] if w['net'] != 'GATE_B+'][:]
rc, out = variant('open', opn); ok(rc != 0 and 'GATE_B+' in out, 'check_boards.py: removing the GATE_B+ wires flags the net as not connected')
def short(L):
    w = next(w for w in L['wires'] if w['net'] == 'GATE_B+'); L['wires'].append({'id': 'wx', 'net': 'GATE_B+', 'side': 'top', 'pts': [w['pts'][0], next(x for x in L['wires'] if x['net'] == 'GND')['pts'][0]]})
rc, out = variant('short', short); ok(rc != 0 and 'SHORT' in out.upper(), 'check_boards.py: a wire from a gate to ground is reported as a short')
with sync_playwright() as p:
    b = p.chromium.launch(executable_path=os.environ.get('CHROME', '/usr/bin/google-chrome'), args=['--no-sandbox'])
    ctx = b.new_context(viewport={'width': 1400, 'height': 900}, accept_downloads=True)
    pg = ctx.new_page(); errs = []; pg.on('pageerror', lambda e: errs.append(str(e))); pg.on('dialog', lambda d: d.accept())
    url = (tools / 'perfboard.html').as_uri() + '?board=coil_driver'
    pg.goto(url); pg.wait_for_timeout(400); J = pg.evaluate
    J("localStorage.clear()"); pg.reload(); pg.wait_for_timeout(400)
    ok(J("PB.L.parts.length") == 15 and J("PB.L.wires.length") == 23, 'coil_driver loads: %s parts, %s wires' % (J("PB.L.parts.length"), J("PB.L.wires.length")))
    ok(pg.locator('#gP [data-ref]').count() == 15 and pg.locator('#gW [data-wire]').count() == 23, 'all parts and wires are drawn on the grid')
    ok(J("document.getElementById('ck').dataset.err") == '0' and 'PASS' in pg.inner_text('#ck'), 'check chip: ' + pg.inner_text('#ck'))
    ok(pg.inner_text('#st').strip() == 'PROPOSED', 'layout is marked PROPOSED')
    ok('1N4007' in pg.inner_text('#offb') or 'flyback' in pg.inner_text('#offb').lower(), 'off-board flyback diode listed')
    ok('Sparky' in pg.inner_text('#notes'), "Sparky's guidance shown in the notes")
    ok(J("PB.check().E.length") == 0, 'in-page check agrees with check_boards.py (0 errors)')
    # ---- 3) in-page bogus id
    J("""(()=>{const s={saved:'t',base:'',L:JSON.parse(JSON.stringify(PB.L))};s.L.parts[0].part='bogus_part_xyz';localStorage.setItem('teryx-pb-coil_driver',JSON.stringify(s))})()""")
    pg.reload(); pg.wait_for_timeout(400)
    ok(J("+document.getElementById('ck').dataset.err") > 0 and 'bogus_part_xyz' in pg.inner_text('#chk'), 'in-page check catches a bogus PARTS id: ' + pg.inner_text('#ck'))
    ok(pg.is_visible('#lc'), '"local edits" chip shows for localStorage edits')
    pg.click('#rst'); pg.wait_for_timeout(300)
    ok(J("PB.L.parts[0].part") != 'bogus_part_xyz' and J("document.getElementById('ck').dataset.err") == '0' and J("localStorage.getItem('teryx-pb-coil_driver')") is None, 'Reset drops the local edits and reloads the committed layout')
    # ---- 4) export unedited == repo
    with pg.expect_download() as d: pg.click('#exp')
    dl = d.value; got = json.loads(open(dl.path()).read())
    ok(dl.suggested_filename == 'coil_driver.layout.json' and got == REPO, 'Export JSON of the unedited board equals docs/boards/coil_driver.layout.json')
    # drag Q-BP one hole right... find the part's anchor on screen
    ref = J("PB.L.parts.find(p=>p.fp==='TO220').ref"); at0 = J(f"PB.L.parts.find(p=>p.ref==='{ref}').at")
    def scr(h): return J(f"""(()=>{{const rc=PB.hole('{h}'),sv=document.getElementById('sv'),m=sv.getScreenCTM(),pt=new DOMPoint((rc[1]-1)*2.54,rc[0]*2.54).matrixTransform(m);return [pt.x,pt.y,m.a]}})()""")
    x, y, sc = scr(at0); pitch = 2.54 * sc
    pg.mouse.move(x, y); pg.mouse.down(); pg.mouse.move(x + pitch * .6, y, steps=4); pg.mouse.move(x + pitch * 3, y + pitch * 2, steps=8); pg.mouse.up(); pg.wait_for_timeout(200)
    r0, c0 = J(f"PB.hole('{at0}')"); at1 = J(f"PB.L.parts.find(p=>p.ref==='{ref}').at"); r1, c1 = J(f"PB.hole('{at1}')")
    ok((r1 - r0, c1 - c0) == (2, 3), f'drag {ref} snaps to holes: {at0} -> {at1}')
    pg.mouse.click(*scr(at1)[:2]); pg.wait_for_timeout(150); pg.click('#rot'); pg.wait_for_timeout(150)
    ok(J(f"PB.L.parts.find(p=>p.ref==='{ref}').rot") == 90, f'Rotate turns {ref} to 90°')
    ok(int(J("document.getElementById('ck').dataset.err")) > 0, 'moving a FET off its wires makes the check fail (open/dangling nets flagged)')
    pg.reload(); pg.wait_for_timeout(400)
    ok(J(f"PB.L.parts.find(p=>p.ref==='{ref}').at") == at1 and J(f"PB.L.parts.find(p=>p.ref==='{ref}').rot") == 90, 'edits persist across reload (localStorage)')
    pg.click('#rst'); pg.wait_for_timeout(300)
    ok(J(f"PB.L.parts.find(p=>p.ref==='{ref}').at") == at0, 'Reset restores the committed position')
    # ---- 5) wiring drv node
    w = ctx.new_page(); w.goto((tools / 'wiring.html').as_uri() + '#precharge'); w.wait_for_timeout(500)
    node = w.locator('[data-board="coil_driver"]').first
    ok(node.count() == 1, 'wiring precharge page: the drv node carries the coil_driver board link')
    node.locator('text=perf-board layout').click(); w.wait_for_timeout(600)
    ok(w.url.endswith('perfboard.html?board=coil_driver') and w.evaluate("PB.L.board") == 'coil_driver', 'clicking the drv node opens ' + w.url.rsplit('/', 1)[-1])
    shot = os.environ.get('SHOT')
    if shot: pg.reload(); pg.wait_for_timeout(500); pg.screenshot(path=shot, full_page=True); print('screenshot ' + shot)
    ok(not errs, 'no page errors ' + repr(errs[:3]))
    # ---- 6) iPhone
    UA = 'Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 Safari/604.1'
    ic = b.new_context(viewport={'width': 390, 'height': 844}, device_scale_factor=3, is_mobile=True, has_touch=True, user_agent=UA)
    ip = ic.new_page(); ie = []; ip.on('pageerror', lambda e: ie.append(str(e))); ip.goto(url); ip.wait_for_timeout(500)
    ok(ip.evaluate("document.documentElement.scrollWidth<=innerWidth+1"), 'iPhone portrait: no sideways scroll')
    ref2 = ip.evaluate("PB.L.parts.find(p=>p.fp.startsWith('TB')).ref")
    bx = ip.locator(f'#gP [data-ref="{ref2}"] rect').first.bounding_box()
    ip.touchscreen.tap(bx['x'] + bx['width'] / 2, bx['y'] + bx['height'] / 2); ip.wait_for_timeout(300)
    ok(ref2 in ip.inner_text('#selb'), f'iPhone touch tap selects {ref2} and shows its part card')
    ok(not ie, 'iPhone: no page errors')
    b.close()
print(f'{n - f}/{n} perfboard checks passed'); sys.exit(1 if f else 0)
