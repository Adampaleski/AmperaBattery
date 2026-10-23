# python3 scripts/test_perfboard_b25.py — Build 25 perf-board designer requests (t312), Playwright + Chrome.
# 1) selection filter: a wire point on a pin of an UNpicked part never moves (that end stays attached and in place; the picked end stretches)
# 2) collision: a drop that puts a part on a part / a pin on a wire / a body over a wire is rejected (red ghost while dragging, snap back);
#    under-body wires the layout already has are only flagged (#flg)
# 3) board edges + corners: pick (click, window, crossing) and drag; the board grows/shrinks in whole holes and can't cut through content
# 4) row/column headers: right-click (long-press) -> insert N above/below/left/right or delete N; blocked deletes name the blocker;
#    undo/redo for every edit (Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z + the ↶ ↷ buttons)
# 5) install mode: layout.steps; current step highlighted, earlier muted, later ghosted; Prev/Next, step list, text, localStorage check-off;
#    ?mode=install&step=N; 44 px touch targets on a coarse pointer
import os, sys, json, pathlib
from playwright.sync_api import sync_playwright
repo = pathlib.Path(__file__).resolve().parent.parent
base = (repo / 'tools' / 'perfboard.html').as_uri()
url = base + '?board=coil_driver'
REPO = json.loads((repo / 'docs' / 'boards' / 'coil_driver.layout.json').read_text())
BMS = json.loads((repo / 'docs' / 'boards' / 'bms_master.layout.json').read_text())
n = f = 0
def ok(c, m):
    global n, f
    n += 1; f += (not c); print(('PASS ' if c else 'FAIL ') + 'pb25 ' + m, flush=True)
PID = next(q['part'] for q in REPO['parts'] if q['fp'] == 'AXIAL400')
T = {"board": "coil_driver", "title": "b25 test", "status": "PROPOSED", "assembly": REPO['assembly'], "board_part": REPO.get('board_part'),
     "grid": {"pitch_mm": 2.54, "rows": 30, "cols": 40}, "mounts": ["A1", "A40", "AD1", "AD40"],
     "parts": [{"ref": "R1", "part": PID, "fp": "AXIAL400", "at": "B3", "rot": 0}, {"ref": "R2", "part": PID, "fp": "AXIAL400", "at": "J3", "rot": 0},
               {"ref": "R3", "part": PID, "fp": "AXIAL400", "at": "F12", "rot": 0}],
     "wires": [{"id": "w1", "net": "N1", "side": "top", "awg": "22 AWG", "pts": ["B7", "B10", "J10", "J7"]},
               {"id": "w2", "net": "N2", "side": "top", "awg": "22 AWG", "pts": ["J3", "L3"]}],
     "nets": {"N1": {"members": ["R1.2", "R2.2"]}, "N2": {"members": ["R2.1"]}}, "nc": ["R1.1", "R3.1", "R3.2"]}
# every layout carries install steps, every part/wire is in exactly one step, PARTS ids are real (check_boards does the full check)
for nm, L in (('bms_master', BMS), ('coil_driver', REPO)):
    S = L.get('steps') or []
    P = [r for s in S for r in s.get('parts', [])]; Wd = [w for s in S for w in s.get('wires', [])]
    ok(len(S) >= 5 and sorted(P) == sorted(p['ref'] for p in L['parts']) and sorted(Wd) == sorted(w['id'] for w in L['wires'])
       and all(s.get('label') in ('SOURCED', 'ASSUMED', 'PROPOSED', 'UNVERIFIED', 'DECIDED', 'OPEN') and s.get('title') and s.get('text') for s in S),
       f'{nm}.layout.json has {len(S)} labelled install steps covering all {len(P)} parts and {len(Wd)} wires once')
ok([s['id'] for s in BMS['steps']][:3] == ['power', 'star', 'teensy'] and all(s['label'] == 'PROPOSED' for s in BMS['steps']), 'bms_master order is PROPOSED (power, star, Teensy first)')
with sync_playwright() as p:
    b = p.chromium.launch(executable_path=os.environ.get('CHROME', '/usr/bin/google-chrome'), args=['--no-sandbox'])
    ctx = b.new_context(viewport={'width': 1400, 'height': 900})
    pg = ctx.new_page(); errs = []; pg.on('pageerror', lambda e: errs.append(str(e))); pg.on('dialog', lambda d: d.accept()); J = pg.evaluate
    def put(L):
        J("L=>localStorage.setItem('teryx-pb-coil_driver',JSON.stringify({saved:'t',base:'',L}))", L); pg.reload(); pg.wait_for_timeout(400)
    def S(x, y):
        return J(f"(()=>{{const m=document.getElementById('sv').getScreenCTM(),q=new DOMPoint({x},{y}).matrixTransform(m);return [q.x,q.y]}})()")
    H = lambda h: S(*J(f"PB.XY(PB.hole('{h}'))"))
    def drag(a, b, mid=None, key=None):
        if key: pg.keyboard.down(key)
        pg.mouse.move(*a); pg.mouse.down(); pg.mouse.move(a[0] + (b[0] - a[0]) * .5, a[1] + (b[1] - a[1]) * .5, steps=5)
        pg.mouse.move(*b, steps=5)
        if mid: mid()
        pg.mouse.up(); pg.wait_for_timeout(150)
        if key: pg.keyboard.up(key)
    W = lambda i: J(f"PB.L.wires.find(w=>w.id==='{i}').pts")
    at = lambda r: J(f"PB.L.parts.find(p=>p.ref==='{r}').at")
    def MM(h, d):   # screen point d mm diagonally off hole h
        x, y = J(f"PB.XY(PB.hole('{h}'))"); return S(x + d, y + d)
    def zoomout():
        for _ in range(8):
            v = J("(()=>{const r=document.getElementById('view').getBoundingClientRect();return [r.x,r.y,r.right,r.bottom]})()")
            a, c = H('A1'), H(J("PB.hname([PB.L.grid.rows-1,PB.L.grid.cols])"))
            if a[0] > v[0] + 60 and a[1] > v[1] + 60 and c[0] < v[2] - 60 and c[1] < v[3] - 60: return True
            pg.mouse.move((v[0] + v[2]) / 2, (v[1] + v[3]) / 2); pg.mouse.wheel(0, 250); pg.wait_for_timeout(80)
        return False
    pg.goto(url); pg.wait_for_timeout(300); J("localStorage.clear()"); put(T)
    ok(J("PB.check().E.length") == 0 and J("PB.hist") == [0, 0], 'synthetic board loads clean, empty undo history')
    # ---- 1) selection filter
    pg.click('#mBox')
    a, c = MM('A9', -.8), MM('K11', .8); drag(c, a)     # crossing over w1's B10-J10 leg only
    ms = J("PB.ms"); ok(ms and ms['wires'] == ['w1'] and ms['verts'] == {'w1': [1, 2]} and ms['parts'] == [], f'crossing over the B10–J10 leg picks its two bends: {ms}')
    a, c = MM('A6', -.8), MM('K11', .8); drag(a, c)     # now B7 (R1 pin 2) and J7 (R2 pin 2) are inside too, but R1/R2 are not picked
    ms = J("PB.ms"); ok(ms and ms['verts'] == {'w1': [1, 2]} and ms['parts'] == [], f'window that also covers the pin ends of w1 still leaves them out (R1, R2 not picked): {ms}')
    drag(H('F10'), H('F8')); w = W('w1')
    ok(w == ['B7', 'B8', 'J8', 'J7'], f'stretch: the leg moves 2 columns left, both ends stay attached at B7/J7: {w}')
    pg.keyboard.press('Control+z'); pg.wait_for_timeout(100); ok(W('w1') == T['wires'][0]['pts'], 'Ctrl+Z undoes the stretch')
    a, c = MM('A2', -.8), MM('C8', .8); drag(c, a)     # crossing around R1 only (w1's far end is on unpicked R2)
    ms = J("PB.ms"); ok(ms and ms['parts'] == ['R1'] and ms['verts'] == {'w1': [0]}, f'crossing picks R1 and only the w1 end on R1: {ms}')
    drag(H('B5'), H('C5')); w = W('w1')
    ok(at('R1') == 'C3' and w[0] == 'C7' and w[-1] == 'J7', f'moving R1 stretches w1: the R1 end follows (C7), the R2 end stays attached in place (J7): {w}')
    pg.keyboard.press('Escape'); pg.click('#mBox'); pg.keyboard.press('Control+z'); pg.wait_for_timeout(100)
    ok(at('R1') == 'B3' and W('w1')[0] == 'B7', 'Ctrl+Z puts R1 and w1 back')
    # ---- 2) collision
    st = {}
    def look(): st['g'] = J("PB.ghost"); st['bad'] = J("document.querySelectorAll('#gP .pb.bad').length"); st['x'] = J("document.querySelectorAll('.colx').length")
    h0 = J("PB.hist")[0]
    drag(H('F14'), H('F10'), look)   # R3 F12 -> F8: body F9..F11 lies over w1 at F10
    ok(st['g'] and 'R3' in st['g']['refs'] and st['bad'] >= 1 and st['x'] >= 1, f'dragging R3 body over w1 shows a red ghost + red hole marks: {st}')
    ok(at('R3') == 'F12' and 'Blocked' in J("PB.msg") and 'w1' in J("PB.msg") and J("PB.hist")[0] == h0 and J("PB.ghost") is None,
       f'drop rejected, R3 snapped back to F12, nothing recorded: {J("PB.msg")!r}')
    ok(J("getComputedStyle(document.getElementById('msg')).display") != 'none' and 'Blocked' in pg.inner_text('#msg'), 'the reason shows in the on-board message')
    drag(H('F14'), H('B5'))         # R3 onto R1 (part on part)
    ok(at('R3') == 'F12' and 'R1' in J("PB.msg") and 'R3' in J("PB.msg"), f'R3 onto R1 is rejected too: {J("PB.msg")!r}')
    drag(H('F14'), H('M14')); ok(at('R3') == 'M12' and J("PB.ghost") is None, 'a free spot still accepts the drop (R3 -> M12)')
    pg.keyboard.press('Control+z'); pg.wait_for_timeout(100); ok(at('R3') == 'F12', 'undo')
    pg.click('#gP [data-ref="R2"]'); pg.click('#rot'); pg.wait_for_timeout(100)   # 90° would lay R2's body over w2 (J3-L3)
    r = J("PB.L.parts.find(p=>p.ref==='R2').rot"); ok(r == 0 and 'Not rotated' in J("PB.msg") and 'w2' in J("PB.msg"), f'a Rotate that would collide is refused (stays at 0°): {J("PB.msg")!r}')
    pg.keyboard.press('Escape'); pg.wait_for_timeout(100)
    # existing overlaps are only flagged
    pg.goto(url); pg.wait_for_timeout(300); J("localStorage.clear()"); pg.reload(); pg.wait_for_timeout(400)
    fl = J("PB.flags()"); ok(len(fl) == 6 and J("PB.check().E.length") == 0 and pg.locator('#flg').count() == 1 and '6 under-body' in pg.inner_text('#flg summary'),
       f'committed coil_driver: 0 errors, its 6 existing under-body wires are listed as flags: {fl[:1]}')
    put(T)
    # ---- 3) board edges + corners
    ok(zoomout(), 'wheel zooms out until the whole board is in view')
    ok(J("document.querySelectorAll('#gE line[data-edge]').length") == 4, '4 pickable board edges are drawn')
    ex = lambda e: J(f"(()=>{{const r=document.querySelector('#gE [data-edge={e}]').getBoundingClientRect();return [r.x+r.width/2,r.y+r.height/2]}})()")
    e = ex('r'); drag(e, [e[0] + (H('A3')[0] - H('A1')[0]), e[1]])
    ok(J("PB.L.grid.cols") == 42 and 'AD42' in J("PB.L.mounts") and '30×40' in J("PB.msg").replace('x', '×') , f'dragging the right edge 2 holes out grows the board to 42 columns, corner mounts follow: {J("PB.msg")!r} {J("PB.L.mounts")}')
    e = ex('r'); drag(e, [e[0] - (H('A30')[0] - H('A1')[0]), e[1]])
    ok(J("PB.L.grid.cols") == 42 and 'Blocked' in J("PB.msg") and 'R3' in J("PB.msg"), f'dragging it in through R3 (cols 12–16) is blocked and names R3: {J("PB.msg")!r}')
    e = ex('t'); drag(e, [e[0], e[1] + (H('B1')[1] - H('A1')[1])])
    ok(J("PB.L.grid.rows") == 29 and at('R1') == 'A3' and W('w1')[0] == 'A7' and at('R2') == 'I3' and 'A1' in J("PB.L.mounts"),
       f'top edge down 1 hole: board 29 rows, everything renamed one row up (R1 A3, R2 I3): {J("PB.L.mounts")}')
    e = ex('t'); drag(e, [e[0], e[1] + (H('C1')[1] - H('A1')[1])])
    ok(J("PB.L.grid.rows") == 29 and 'Blocked' in J("PB.msg") and 'R1' in J("PB.msg"), f'the next step down would cut R1: blocked: {J("PB.msg")!r}')
    pg.keyboard.press('Control+z'); pg.keyboard.press('Control+z'); pg.wait_for_timeout(100)
    ok(J("PB.L.grid") == T['grid'] and at('R1') == 'B3' and J("PB.L.mounts") == T['mounts'], 'two undos restore the 30×40 board')
    pg.click('#mBox'); drag(MM('AD40', 1), MM('AD40', 3.5)); c0 = MM('AD40', 1.905)   # the corner sits 0.75 pitch past the last hole
    ms = J("PB.ms"); ok(ms and ms['edges'] == ['b', 'r'], f'a window around the bottom-right corner picks both edges: {ms}')
    drag(c0, [c0[0] + (H('A2')[0] - H('A1')[0]), c0[1] + (H('B1')[1] - H('A1')[1])])
    ok(J("PB.L.grid") == {**T['grid'], 'rows': 31, 'cols': 41} and 'AE41' in J("PB.L.mounts"), f'dragging the corner grows both ways (31×41): {J("PB.L.grid")}')
    pg.keyboard.press('Escape'); pg.click('#mBox'); pg.keyboard.press('Control+z'); pg.wait_for_timeout(100)
    # ---- 4) row/column headers + undo/redo
    ok(J("document.querySelectorAll('#gG [data-hdr]').length") == 70, 'one header per row (30) and column (40)')
    def hdr(h, act, k=1):
        bb = pg.locator(f'#gG [data-hdr="{h}"]').bounding_box(); pg.mouse.click(bb['x'] + bb['width'] / 2, bb['y'] + bb['height'] / 2, button='right'); pg.wait_for_timeout(80)
        vis = pg.is_visible('#ctx'); pg.fill('#hN', str(k)); pg.click(f'#ctx button[data-a="{act}"]'); pg.wait_for_timeout(120); return vis
    v = hdr('r:9', 'i0', 2)   # 2 rows above J
    ok(v and J("PB.L.grid.rows") == 32 and at('R2') == 'L3' and W('w2') == ['L3', 'N3'] and W('w1')[-1] == 'L7' and at('R1') == 'B3' and 'AF40' in J("PB.L.mounts"),
       f'right-click row J -> Insert 2 above: R2 J3->L3, w1/w2 follow, rows renumbered (32): {J("PB.msg")!r}')
    ok(J("document.querySelectorAll('#gG [data-hdr^=\"r:\"]').length") == 32, 'row headers renumbered (32)')
    v = hdr('c:5', 'i1', 1)   # column right of 5 -> splits R1 (pins 3 and 7)
    ok(J("PB.L.grid.cols") == 40 and "R1" in J("PB.msg") and "Can't insert" in J("PB.msg"), f'inserting a column through R1 is refused: {J("PB.msg")!r}')
    hdr('r:11', 'd')
    ok(J("PB.L.grid.rows") == 32 and "Can't delete row L" in J("PB.msg") and 'R2' in J("PB.msg"), f'deleting row L is blocked and names R2: {J("PB.msg")!r}')
    hdr('r:5', 'd', 3)
    ok(J("PB.L.grid.rows") == 32 and 'w1' in J("PB.msg"), f'deleting rows F–H is blocked by the w1 leg: {J("PB.msg")!r}')
    hdr('c:38', 'd', 3)
    ok(J("PB.L.grid.cols") == 37 and 'A37' in J("PB.L.mounts") and J("PB.check().E.length") == 0, f'deleting 3 empty columns 38–40 works, corner mounts follow: {J("PB.msg")!r}')
    hist = J("PB.hist"); pg.keyboard.press('Control+z'); pg.wait_for_timeout(80)
    ok(J("PB.L.grid.cols") == 40 and J("PB.hist") == [hist[0] - 1, 1], 'Ctrl+Z undoes the delete')
    pg.keyboard.press('Control+y'); pg.wait_for_timeout(80); ok(J("PB.L.grid.cols") == 37, 'Ctrl+Y redoes it')
    pg.click('#undo'); pg.click('#undo'); pg.wait_for_timeout(80); ok(J("PB.L.grid.rows") == 30 and at('R2') == 'J3', '↶ button undoes twice (back to 30 rows)')
    pg.click('#redo'); pg.wait_for_timeout(80); ok(J("PB.L.grid.rows") == 32, '↷ button redoes')
    pg.keyboard.press('Control+Shift+z'); pg.wait_for_timeout(80); ok(J("PB.L.grid.cols") == 37, 'Ctrl+Shift+Z redoes too')
    pg.reload(); pg.wait_for_timeout(400); ok(J("PB.L.grid") == {**T['grid'], 'rows': 32, 'cols': 37}, 'header edits persist across reload')
    ok(J("document.getElementById('undo').disabled") and J("document.getElementById('redo').disabled"), 'undo/redo buttons start disabled after a load')
    ok(not errs, f'no page errors (editor): {errs[:2]}')
    # ---- 5) install mode
    ip = ctx.new_page(); errs2 = []; ip.on('pageerror', lambda e: errs2.append(str(e))); K = ip.evaluate
    ip.goto(base + '?board=bms_master&mode=install&step=3'); ip.wait_for_timeout(500); K("localStorage.removeItem('teryx-pb-steps-bms_master')")
    ip.goto(base + '?board=bms_master&mode=install&step=3'); ip.wait_for_timeout(500)
    ok(K("PB.mode") == 'inst' and K("PB.step") == 2 and ip.inner_text('#sNum') == 'Step 3 / 9' and ip.is_visible('#sText') and ip.is_visible('#sList'),
       f'?mode=install&step=3 opens Install mode at step 3: {ip.inner_text("#sTitle")!r}')
    cls = lambda r: K(f"document.querySelector('#gP [data-ref=\"{r}\"]').getAttribute('class')")
    S3 = BMS['steps']; cur, prev, fut = S3[2]['parts'][0], S3[0]['parts'][0], S3[4]['parts'][0]
    ok('st-cur' in cls(cur) and 'st-prev' in cls(prev) and 'st-fut' in cls(fut), f'{cur} highlighted, {prev} (earlier) muted, {fut} (later) ghosted')
    op = lambda r: float(K(f"getComputedStyle(document.querySelector('#gP [data-ref=\"{r}\"]')).opacity"))
    ok(op(cur) == 1 and 0.3 < op(prev) < 0.8 and op(fut) < 0.25, f'opacity cur/prev/fut = {op(cur)}/{op(prev)}/{op(fut)}')
    ok(K("document.querySelectorAll('#gE line').length") == 0 and K("document.getElementById('undo').closest('[hidden]')") is None, 'install mode hides the board-edge handles')
    ip.click('#sNext'); ok(K("PB.step") == 3 and K("PB.stState().cur") == 3 and 'st-cur' in cls(S3[3]['parts'][0]), 'Next ▶ goes to step 4 and remembers it')
    ip.click('#sPrev'); ip.click('#sPrev'); ok(K("PB.step") == 1, '◀ Prev x2 -> step 2')
    ip.keyboard.press('ArrowRight'); ok(K("PB.step") == 2, '→ key steps forward')
    ip.click('#sList li[data-i="5"] span'); ok(K("PB.step") == 5 and 'clamp' in ip.inner_text('#sTitle').lower(), f'clicking a list entry jumps to it: {ip.inner_text("#sTitle")!r}')
    ip.check('#sDone'); d = K("PB.stState().done"); ok(d == {S3[5]['id']: True} and 'done' in (ip.get_attribute('#sList li[data-i="5"]', 'class') or ''), f'Step done check-off is stored in localStorage: {d}')
    ip.check('#sList li[data-i="0"] input'); ok(set(K("PB.stState().done")) == {S3[5]['id'], S3[0]['id']}, 'list check-boxes tick steps off too')
    bb = ip.locator(f'#gP [data-ref="{S3[7]["parts"][0]}"]').bounding_box(); ip.mouse.click(bb['x'] + bb['width'] / 2, bb['y'] + bb['height'] / 2); ip.wait_for_timeout(100)
    ok(K("PB.step") == 7 and K(f"PB.L.parts.find(p=>p.ref==='{S3[7]['parts'][0]}').at") == next(q['at'] for q in BMS['parts'] if q['ref'] == S3[7]['parts'][0]),
       'tapping a part jumps to its step and does not move it')
    ip.goto(base + '?board=bms_master'); ip.wait_for_timeout(400); ip.click('#mInst'); ip.wait_for_timeout(100)
    ok(K("PB.mode") == 'inst' and K("PB.step") == 7 and ip.is_checked('#sList li[data-i="5"] input') and ip.is_checked('#sList li[data-i="0"] input'), 'after a reload the Install button resumes at the saved step with the check-offs')
    ip.keyboard.press('Escape'); ok(K("PB.mode") == 'sel' and not ip.is_visible('#sNum') and 'st-' not in cls(cur), 'Esc leaves Install mode, classes cleared')
    K("localStorage.removeItem('teryx-pb-coil_driver')"); ip.goto(base + '?board=coil_driver&mode=install'); ip.wait_for_timeout(400)
    ok(ip.inner_text('#sNum').endswith('/ 12') and K("PB.L.steps.some(s=>s.label==='DECIDED')"), 'coil_driver has its 12 guide steps')
    ok(not errs2, f'no page errors (install): {errs2[:2]}')
    # 44 px touch targets on a phone
    mc = b.new_context(viewport={'width': 390, 'height': 844}, is_mobile=True, has_touch=True, device_scale_factor=2)
    mp = mc.new_page(); mp.goto(base + '?board=bms_master&mode=install&step=2'); mp.wait_for_timeout(500)
    small = mp.evaluate("""[...document.querySelectorAll('#inst button,#inst label.chk,#sList li,#undo,#redo,#rot,#mInst')].filter(e=>e.offsetParent)
       .map(e=>[e.id||e.tagName,e.getBoundingClientRect().height,e.getBoundingClientRect().width]).filter(([,h,w])=>h<44||w<44)""")
    ok(mp.locator('#sNext').count() == 1 and not small, f'phone: Install, undo/redo and step controls are >= 44 px: {small[:4]}')
    mp.screenshot(path='/tmp/pb25_phone.png')
    b.close()
print(f'{n - f}/{n} perfboard Build 25 checks passed'); sys.exit(1 if f else 0)
