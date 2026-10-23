# python3 scripts/test_perfboard_edit.py — Build 22 perf-board designer edits (Playwright + Chrome), on a small synthetic board.
# 1) Window select: drag left->right = window (only items fully inside, solid blue box); right->left = crossing (anything touched,
#    dashed green box). Shift adds, Esc clears.
# 2) Stretch: dragging a picked part moves the picked parts; attached wire ends follow; wire points inside the window move; the rest stay.
# 3) Attachment: wire ends on pins follow a single-part drag and Rotate; dots mark attached ends; right-click -> Detach leaves the end
#    behind (check flags the open net); dropping the free end on a pin re-attaches it. Touch: long-press opens the same menu,
#    the Window toggle + a one-finger drag selects.
# 4) Auto-size: board shrinks to parts + a 2-hole margin, mounts go to the new corners; a layout with no grid starts auto-sized;
#    the committed boards are untouched until the button is clicked; bms_master is already auto-sized (no-op).
import os, sys, json, pathlib
from playwright.sync_api import sync_playwright
repo = pathlib.Path(__file__).resolve().parent.parent
url = (repo / 'tools' / 'perfboard.html').as_uri() + '?board=coil_driver'
REPO = json.loads((repo / 'docs' / 'boards' / 'coil_driver.layout.json').read_text())
n = f = 0
def ok(c, m):
    global n, f
    n += 1; f += (not c); print(('PASS ' if c else 'FAIL ') + 'pbedit ' + m, flush=True)
PID = next(q['part'] for q in REPO['parts'] if q['fp'] == 'AXIAL400')
T = {"board": "coil_driver", "title": "edit test", "status": "PROPOSED", "assembly": REPO['assembly'], "board_part": REPO.get('board_part'),
     "grid": {"pitch_mm": 2.54, "rows": 30, "cols": 40}, "mounts": ["A1", "A40", "AD1", "AD40"],
     "parts": [{"ref": "R1", "part": PID, "fp": "AXIAL400", "at": "B3", "rot": 0}, {"ref": "R2", "part": PID, "fp": "AXIAL400", "at": "J3", "rot": 0},
               {"ref": "R3", "part": PID, "fp": "AXIAL400", "at": "F12", "rot": 0}],
     "wires": [{"id": "w1", "net": "N1", "side": "top", "awg": "22 AWG", "pts": ["B7", "B10", "J10", "J7"]},
               {"id": "w2", "net": "N2", "side": "top", "awg": "22 AWG", "pts": ["J3", "L3"]}],
     "nets": {"N1": {"members": ["R1.2", "R2.2"]}, "N2": {"members": ["R2.1"]}}, "nc": ["R1.1", "R3.1", "R3.2"]}
with sync_playwright() as p:
    b = p.chromium.launch(executable_path=os.environ.get('CHROME', '/usr/bin/google-chrome'), args=['--no-sandbox'])
    ctx = b.new_context(viewport={'width': 1400, 'height': 900})
    pg = ctx.new_page(); errs = []; pg.on('pageerror', lambda e: errs.append(str(e))); pg.on('dialog', lambda d: d.accept()); J = pg.evaluate
    def put(L, page=None):
        (page or pg).evaluate("L=>localStorage.setItem('teryx-pb-coil_driver',JSON.stringify({saved:'t',base:'',L}))", L); (page or pg).reload(); (page or pg).wait_for_timeout(400)
    def S(x, y, page=None):   # board mm -> screen px
        return (page or pg).evaluate(f"(()=>{{const m=document.getElementById('sv').getScreenCTM(),q=new DOMPoint({x},{y}).matrixTransform(m);return [q.x,q.y]}})()")
    H = lambda h: J(f"PB.XY(PB.hole('{h}'))")
    def drag(a, b, mid=None, shift=False):
        if shift: pg.keyboard.down('Shift')
        pg.mouse.move(*a); pg.mouse.down(); pg.mouse.move(a[0] + (b[0] - a[0]) * .5, a[1] + (b[1] - a[1]) * .5, steps=5)
        if mid: mid()
        pg.mouse.move(*b, steps=5); pg.mouse.up(); pg.wait_for_timeout(120)
        if shift: pg.keyboard.up('Shift')
    W = lambda i: J(f"PB.L.wires.find(w=>w.id==='{i}')")
    at = lambda r: J(f"PB.L.parts.find(p=>p.ref==='{r}').at")
    pg.goto(url); pg.wait_for_timeout(300); J("localStorage.clear()"); put(T)
    ok(J("PB.L.title") == 'edit test' and J("PB.check().E.length") == 0, 'synthetic board loads and checks clean: ' + pg.inner_text('#ck'))
    # ---- 1) window vs crossing
    pg.click('#mBox'); ok(J("PB.mode") == 'box' and 'on' in pg.get_attribute('#mBox', 'class'), 'Window toggle switches to window-select mode')
    st = {}
    def look(): st['win'] = J("document.querySelectorAll('#gD rect.selwin').length"); st['cross'] = J("document.querySelectorAll('#gD rect.selcross').length"); st['dash'] = J("(()=>{const r=document.querySelector('#gD rect.selcross');return r?getComputedStyle(r).strokeDasharray:''})()")
    drag(S(4, .8), S(16, 4.3), look)
    ok(st['win'] == 1 and st['cross'] == 0, 'left->right drag draws a solid window box')
    ms = J("PB.ms"); ok(ms['parts'] == ['R1'] and ms['wires'] == [] and ms['kind'] == 'window', f'window: only R1 (fully inside) is picked, w1 (half out) is not: {ms}')
    drag(S(16, 4.3), S(4, .8), look)
    ok(st['cross'] == 1 and st['win'] == 0 and st['dash'] not in ('', 'none'), f'right->left drag draws a dashed crossing box ({st["dash"]})')
    ms = J("PB.ms"); ok(ms['parts'] == ['R1'] and ms['wires'] == ['w1'] and ms['verts'] == {'w1': [0]} and ms['kind'] == 'crossing', f'crossing: R1 and the touched wire w1 (point B7 inside) are picked: {ms}')
    drag(S(4, .8), S(10, 4.3)); ok(J("PB.ms") is None, 'window over half of R1 picks nothing')
    drag(S(10, 4.3), S(4, .8)); ms = J("PB.ms"); ok(ms and ms['parts'] == ['R1'] and ms['wires'] == [], f'crossing over half of R1 picks R1: {ms}')
    drag(S(4, 21.1), S(16, 24.6), shift=True); ms = J("PB.ms"); ok(ms and ms['parts'] == ['R1', 'R2'], f'Shift + window around R2 adds it: {ms and ms["parts"]}')
    ok('2' in pg.inner_text('#selb') and 'selection' in pg.inner_text('#selb').lower(), 'side panel shows the selection count')
    pg.keyboard.press('Escape'); pg.wait_for_timeout(80); ok(J("PB.ms") is None and J("document.querySelectorAll('#gP .pb.sel').length") == 0, 'Esc clears the selection')
    # ---- 2) stretch
    drag(S(24, 4.3), S(4, .8)); ms = J("PB.ms"); ok(ms and ms['parts'] == ['R1'] and ms['verts'] == {'w1': [0, 1]}, f'crossing picks R1 + w1 points B7, B10: {ms}')
    x, y = S(10.16, 2.54); dy = S(0, 5.08)[1] - S(0, 0)[1]
    drag((x, y), (x, y + dy))
    ok(at('R1') == 'D3', f'stretch: R1 moved 2 rows (B3 -> {at("R1")})')
    ok(W('w1')['pts'] == ['D7', 'D10', 'J10', 'J7'], f'stretch: w1 attached end + inside point moved, outside points stayed: {W("w1")["pts"]}')
    ok(W('w2')['pts'] == ['J3', 'L3'] and at('R2') == 'J3' and at('R3') == 'F12', 'stretch: unpicked parts and wires stay put')
    ok(J("PB.check().E.length") == 0, 'stretched board still checks clean')
    pg.keyboard.press('Escape'); pg.click('#mSel')
    # ---- 3) attachment
    x, y = S(10.16, 22.86); dx = S(2.54, 0)[0] - S(0, 0)[0]
    drag((x, y), (x + dx, y))
    ok(at('R2') == 'J4' and W('w1')['pts'][-1] == 'J8' and W('w2')['pts'][0] == 'J4', f'single drag: R2 J3 -> {at("R2")}, attached ends follow (w1 end {W("w1")["pts"][-1]}, w2 start {W("w2")["pts"][0]})')
    ok(J("document.querySelectorAll('#gO .att').length") == 3, 'attached ends show a dot (3: w1 x2, w2 x1; the free w2 end at L3 has none)')
    pg.mouse.click(x + dx, y); pg.wait_for_timeout(80); pg.click('#rot'); pg.wait_for_timeout(80)
    p2 = J("PB.hname(PB.pinsOf(PB.L.parts.find(p=>p.ref==='R2'))['2'])")
    ok(W('w1')['pts'][-1] == p2 and J("PB.L.parts.find(p=>p.ref==='R2').rot") == 90, f'Rotate: w1 end follows R2.2 to {p2}')
    for _ in range(3): pg.click('#rot'); pg.wait_for_timeout(50)
    ok(W('w1')['pts'][-1] == 'J8', 'rotate back to 0°: the end is back on J8')
    pg.keyboard.press('Escape')
    pg.mouse.click(*S(*H('J8')), button='right'); pg.wait_for_timeout(120)
    txt = pg.inner_text('#ctx') if pg.is_visible('#ctx') else ''
    ok('Detach w1 (N1) from R2.2' in txt, 'right-click a wire end on a pin opens the Detach menu: ' + repr(txt))
    pg.click('#ctx button'); pg.wait_for_timeout(100)
    ok(W('w1').get('free') == [False, True] and not pg.is_visible('#ctx'), 'Detach frees the end (w1.free = [false, true])')
    ok(J("document.querySelectorAll('#gO .att').length") == 2, 'the detached end loses its dot')
    drag((x + dx, y), (x + 2 * dx, y))
    ok(at('R2') == 'J5' and W('w1')['pts'][-1] == 'J8' and W('w2')['pts'][0] == 'J5', f'after Detach, moving R2 leaves w1 behind at J8 (w2 still follows to {W("w2")["pts"][0]})')
    E = J("PB.check().E"); ok(any('N1' in e and 'NOT connected' in e for e in E), 'the check flags N1 as open: ' + repr(E[:2]))
    pg.mouse.click(*S(22.86, 12.7)); pg.wait_for_timeout(100)   # select w1 on its D10-J10 run
    ok(J("document.querySelectorAll('#gD [data-vh]').length") == 4, 'select w1: its 4 solder-point handles show')
    drag(S(*H('J8')), S(*H('J9')))
    ok(W('w1')['pts'][-1] == 'J9' and 'free' not in W('w1') and J("PB.check().E.length") == 0, f'dropping the free end on R2.2 (J9) re-attaches it: free={W("w1").get("free")}, check {J("PB.check().E.length")} err')
    pg.keyboard.press('Escape'); drag((x + 2 * dx, y), (x + dx, y))
    ok(W('w1')['pts'][-1] == 'J8', 'and it follows R2 again (J9 -> J8)')
    # ---- 4) auto-size
    put(T); pg.click('#asz'); pg.wait_for_timeout(200); L = J("PB.L")
    ok(L['grid']['rows'] == 15 and L['grid']['cols'] == 18, f'Auto-size: 30x40 -> {L["grid"]["rows"]}x{L["grid"]["cols"]} (rows 1..11, cols 3..16 + 2 holes each side)')
    ok([q['at'] for q in L['parts']] == ['C3', 'K3', 'G12'] and L['wires'][0]['pts'] == ['C7', 'C10', 'K10', 'K7'], 'Auto-size shifts parts and wires together (2-hole margin)')
    ok(L['mounts'] == ['A1', 'A18', 'O1', 'O18'] and J("PB.check().E.length") == 0, f'corner mounts move to the new corners {L["mounts"]}, check clean')
    T2 = json.loads(json.dumps(T)); del T2['grid']; put(T2); L = J("PB.L")
    ok(L['grid']['rows'] == 15 and L['grid']['cols'] == 18 and L['parts'][0]['at'] == 'C3', 'a new layout with no grid starts auto-sized (15x18)')
    J("localStorage.clear()"); pg.reload(); pg.wait_for_timeout(400)
    ok(J("PB.L.grid") == REPO['grid'] and J("localStorage.getItem('teryx-pb-coil_driver')") is None, 'committed coil_driver is untouched until Auto-size is clicked')
    pg.click('#asz'); pg.wait_for_timeout(200); g = J("PB.L.grid")
    # coil_driver's terminal-block bodies sit on rows A and Z (the board edge), so the 2-hole margin adds 2 rows each end; columns shrink 36 -> 32
    ok((g['rows'], g['cols']) == (30, 32) and J("PB.check().E.length") == 0 and J("!!localStorage.getItem('teryx-pb-coil_driver')"),
       f'Auto-size on coil_driver: {REPO["grid"]["rows"]}x{REPO["grid"]["cols"]} -> {g["rows"]}x{g["cols"]}, check clean, saved as a local edit')
    pg.goto(url.replace('coil_driver', 'bms_master')); pg.wait_for_timeout(600)
    ok(J("(()=>{const c=JSON.parse(JSON.stringify(PB.L));PB.autosize(c);return JSON.stringify(c)===JSON.stringify(PB.L)})()"), 'bms_master is already auto-sized (Auto-size is a no-op, same 2-hole rule as gen_bms_master.py)')
    ok(not errs, 'no page errors ' + repr(errs[:3]))
    # ---- touch: Window toggle + one-finger drag, long-press menu
    UA = 'Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 Safari/604.1'
    ic = b.new_context(viewport={'width': 390, 'height': 844}, device_scale_factor=3, is_mobile=True, has_touch=True, user_agent=UA)
    ip = ic.new_page(); ie = []; ip.on('pageerror', lambda e: ie.append(str(e))); ip.goto(url); ip.wait_for_timeout(300); put(T, ip)
    bb = ip.locator('#mBox').bounding_box(); ip.touchscreen.tap(bb['x'] + bb['width'] / 2, bb['y'] + bb['height'] / 2); ip.wait_for_timeout(100)
    ok(ip.evaluate("PB.mode") == 'box' and bb['height'] >= 43.5, f'iPhone: tap the Window toggle (hit area {bb["width"]:.0f}x{bb["height"]:.0f} px)')
    vis = ip.evaluate("(()=>{const b=document.getElementById('mBox'),c=getComputedStyle(b);return [c.borderTopColor,c.borderTopWidth,c.backgroundClip]})()")
    ok(vis[1] == '7px' and 'rgba(0, 0, 0, 0)' in vis[0] and vis[2] == 'padding-box', f'iPhone: the 44 px target is padded by an invisible border, the visible pill stays slim {vis}')
    def touch(kind, x, y): ip.evaluate(f"document.getElementById('sv').dispatchEvent(new PointerEvent('{kind}',{{pointerId:11,pointerType:'touch',isPrimary:true,bubbles:true,clientX:{x},clientY:{y}}}))")
    a, z = S(16, 4.3, ip), S(4, .8, ip)
    touch('pointerdown', *a); [touch('pointermove', a[0] + (z[0] - a[0]) * k / 6, a[1] + (z[1] - a[1]) * k / 6) for k in range(1, 7)]; touch('pointerup', *z); ip.wait_for_timeout(100)
    ms = ip.evaluate("PB.ms"); ok(ms and ms['parts'] == ['R1'] and ms['wires'] == ['w1'], f'iPhone: one-finger right->left drag in Window mode = crossing selection {ms}')
    ip.evaluate("PB.L") and ip.keyboard.press('Escape')
    j = S(*ip.evaluate("PB.XY(PB.hole('J7'))"), ip); touch('pointerdown', *j); ip.wait_for_timeout(750)
    t = ip.inner_text('#ctx') if ip.is_visible('#ctx') else ''; touch('pointerup', *j)
    ok('Detach w1 (N1) from R2.2' in t, 'iPhone: long-press a wire end opens the Detach menu ' + repr(t))
    ok(ip.locator('#ctx button').first.bounding_box()['height'] >= 43.5, 'iPhone: the menu item is a 44 px target')
    ok(not ie, 'iPhone: no page errors ' + repr(ie[:3]))
    b.close()
print(f'{n - f}/{n} perfboard edit checks passed'); sys.exit(1 if f else 0)
