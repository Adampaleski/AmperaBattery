# python3 scripts/test_iphone.py — Build 15 iPhone checks (Playwright + Chrome, iPhone-sized touch emulation).
# 1) dash.html?src=sim with NO Sim tab: the sim engine starts inside the page (iOS freezes background tabs), and the
#    dash's SIMULATOR panel drives it: key, phone, throttle pop-up, PIN to Drive with the touch PIN pad, full controls.
# 2) Every page at 390x844 and 844x390: fits the width (no sideways scroll), controls >= 44 px on screen,
#    touch-action: manipulation on controls, safe-area insets used, no hover-only CSS.
# Cannot check headless (report, do not claim): real Safari tab freezing, notch insets on a device, tap feel.
import os, sys, pathlib
from playwright.sync_api import sync_playwright
root = pathlib.Path(__file__).resolve().parent.parent / 'tools'
n = f = 0
def ok(c, m):
    global n, f
    n += 1; f += (not c); print(('PASS ' if c else 'FAIL ') + 'iphone ' + m, flush=True)
UA = 'Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 Safari/604.1'
AUDIT = """(W)=>{const vv=window.visualViewport,eff=W/innerWidth;const SEL='button,a[href],input:not([type=hidden]),select,textarea,summary,[role=button],[data-sc],[data-sg],[data-g],[data-k]:not(i):not(tr)';
 const small=[],noTA=[];document.querySelectorAll(SEL).forEach(e=>{const r=e.getBoundingClientRect();if(!r.width||e.offsetParent===null&&getComputedStyle(e).position!=='fixed')return;
  if(e.closest('#simEngine'))return;
  if(e.tagName==='INPUT'&&/checkbox|radio/.test(e.type)&&e.closest('label')){const L=e.closest('label').getBoundingClientRect();if(L.width*eff>=43.5&&L.height*eff>=43.5)return;}
  if(e.tagName==='A'&&getComputedStyle(e).display==='inline'&&e.closest('p,li,.desc,dd,td'))return;   // links inside running text
  const w=r.width*eff,h=r.height*eff;if(w<43.5||h<43.5)small.push((e.id||e.dataset.f||e.dataset.sc||e.tagName)+' '+(e.textContent||'').trim().slice(0,16)+' '+Math.round(w)+'x'+Math.round(h));
  if(!/manipulation|none/.test(getComputedStyle(e).touchAction))noTA.push(e.tagName+'#'+e.id);});
 let safe=0,hover=0;for(const s of document.styleSheets){let rs;try{rs=s.cssRules;}catch(_){continue;}const walk=L=>{for(const r of L){if(r.cssRules)walk(r.cssRules);const t=r.cssText||'';if(/safe-area-inset/.test(t))safe++;if(/:hover[^{]*\\{[^}]*(display|visibility|opacity)/.test(t))hover++;}};walk(rs);}
 return {eff,innerWidth,scrollW:document.documentElement.scrollWidth,small,noTA,safe,hover};}"""
with sync_playwright() as p:
    b = p.chromium.launch(executable_path=os.environ.get('CHROME', '/usr/bin/google-chrome'), args=['--no-sandbox'])
    # ---- 1) in-page engine, portrait iPhone, no Sim tab anywhere
    ctx = b.new_context(viewport={'width': 390, 'height': 844}, device_scale_factor=3, is_mobile=True, has_touch=True, user_agent=UA)
    pg = ctx.new_page(); errs = []; pg.on('pageerror', lambda e: errs.append(str(e)))
    pg.goto((root / 'dash.html').as_uri() + '?src=sim')
    J = pg.evaluate; F = lambda k: '.tdash [data-f="%s"]' % k
    pg.wait_for_function("(document.querySelector('[data-f=seng]')||{}).textContent.includes('running in this page')", timeout=15000)
    ok(J("!!document.getElementById('simEngine')&&!!last&&last.schema===1"), 'no Sim tab: the sim engine starts inside the dash page and feeds it (' + J("document.querySelector('[data-f=seng]').textContent") + ')')
    ok(J("getComputedStyle(document.querySelector('[data-f=sengb]')).display") == 'none', 'engine running: the "Run sim in this page" button hides')
    def I(k): return J(f"last&&last.simInfo?last.simInfo.{k}:null")
    k0 = I('key'); pg.tap(F('skey')); pg.wait_for_timeout(2500)
    ok(I('key') != k0 and I('key'), 'tap Key on the dash panel: the in-page sim turns the key on')
    ph0 = I('phoneKey'); pg.tap(F('sphone')); pg.wait_for_timeout(600)
    ok(I('phoneKey') != ph0, 'tap Phone nearby: the phone input flips in the in-page sim')
    if not I('phoneKey'): pg.tap(F('sphone')); pg.wait_for_timeout(600)
    # park on + throttle: plain-words pop-up on this page
    if not J("last.simInfo.block&&last.simInfo.block.drive.some(x=>/Parking brake/.test(x))"): pg.tap(F('spark')); pg.wait_for_timeout(600)
    J("(()=>{const e=document.querySelector('.tdash [data-sv=thr]');e.value=30;e.dispatchEvent(new Event('input'));})()"); pg.wait_for_timeout(900)
    t = J("(document.querySelector('.tlock-toast')||{}).textContent||''")
    ok('Throttle blocked' in t and 'Parking brake' in t, 'park set + throttle: the pop-up shows on the phone dash ' + repr(t))
    J("(()=>{const e=document.querySelector('.tdash [data-sv=thr]');e.value=0;e.dispatchEvent(new Event('input'));})()"); pg.wait_for_timeout(300)
    # PIN to Drive (Build 18, Adam DECIDED 2026-10-23): default PIN 0000 + "change it" reminder, no forced first-time setup.
    # With the phone nearby, turning it on needs no PIN.
    def pad(pin):
        for d in pin: pg.tap('.tpad [data-k="%s"]' % d)
        pg.tap('.tpad [data-a=enter]'); pg.wait_for_timeout(700)
    pg.tap(F('sdpin')); pg.wait_for_timeout(700)
    ok(pg.locator('.tpad').count() == 0 and I('pinToDrive'), 'PIN to Drive on with the phone nearby: no PIN asked, no forced "Set a PIN" pad')
    ok(I('pinDefault') is True, 'default PIN is 0000 and the in-page sim flags the "change your PIN" reminder')
    pg.tap(F('sphone')); pg.wait_for_timeout(800)      # phone away, key on, stopped -> the gate locks at once (live rule)
    ok(I('key') and not I('phoneKey') and I('driveOk') is False, 'phone away with the key on and stopped: drive locks until PIN (live, not latched)')
    if J("last.simInfo.block.drive.some(x=>/Parking brake/.test(x))"): pg.tap(F('spark')); pg.wait_for_timeout(500)
    if J("last.simInfo.block.drive.some(x=>/Neutral/.test(x))"): pg.tap(F('sneut')); pg.wait_for_timeout(500)
    J("(()=>{const e=document.querySelector('.tdash [data-sv=thr]');e.value=20;e.dispatchEvent(new Event('input'));})()"); pg.wait_for_timeout(900)
    t = J("(document.querySelector('.tlock-toast')||{}).textContent||''")
    ok('Enter PIN or bring phone to drive' in t and pg.locator('.tpad').count() == 1, 'throttle with PIN to Drive on: pop-up "Enter PIN or bring phone to drive" + PIN pad on the phone ' + repr(t))
    if pg.locator('.tpad').count():
        pad('1111'); ok(pg.locator('.tpad').count() == 1 and 'Wrong PIN' in pg.inner_text('.tpad .tpm'), 'wrong PIN: pad stays open and says "Wrong PIN — try again"')
        pad('0000'); ok(pg.locator('.tpad').count() == 0, 'right PIN (default 0000): pad closes')
    pg.wait_for_timeout(800)
    ok(I('driveOk') is True, 'after the PIN the in-page sim allows drive')
    J("(()=>{const e=document.querySelector('.tdash [data-sv=thr]');e.value=0;e.dispatchEvent(new Event('input'));})()")
    # key cycle with no phone: key-on itself asks for the PIN (Build 18 key-on gate); the key stays off until the PIN
    pg.tap(F('skey')); pg.wait_for_timeout(1200)
    if 'red' not in (pg.get_attribute(F('spark'), 'class') or ''): pg.tap(F('spark')); pg.wait_for_timeout(600)   # key-on needs the parking brake set
    pg.tap(F('skey')); pg.wait_for_timeout(1500)
    ok(pg.locator('.tpad').count() == 1 and not I('key'), 'key on without the phone: PIN pad opens and the key stays off until the PIN')
    if pg.locator('.tpad').count():
        pad('0000'); pg.wait_for_timeout(2500)
        ok(I('key') and I('driveOk') is True, 'PIN 0000 at key-on: key turns on, drive allowed this key cycle')
    # full controls overlay
    pg.tap(F('sengf')); pg.wait_for_timeout(500)
    ok(J("document.getElementById('simEngine').getBoundingClientRect().width>300") and pg.locator('[data-f=sengx]').is_visible(), '"Full sim controls" shows the whole simulator over the dash, with a Back button')
    pg.tap('[data-f=sengx]'); pg.wait_for_timeout(300)
    ok(J("document.getElementById('simEngine').getBoundingClientRect().width<5"), 'Back to dash hides it again (engine keeps running)')
    # tell-tale tap explains itself (no hover on phones)
    pg.tap('.tdash .tt i[data-k="phone"]'); pg.wait_for_timeout(200)
    ok('Phone nearby' in (J("(document.querySelector('.tdash [data-f=toast]')||{}).textContent") or ''), 'tapping a warning light shows its meaning (no hover needed)')
    ok(not errs, 'no page errors (engine mode) ' + '; '.join(errs[:3]))
    ctx.close()
    # ?engine=tab never starts the in-page engine
    c2 = b.new_context(viewport={'width': 390, 'height': 844}, is_mobile=True, has_touch=True); p2 = c2.new_page()
    p2.goto((root / 'dash.html').as_uri() + '?src=sim&engine=tab'); p2.wait_for_timeout(3500)
    ok(p2.evaluate("!document.getElementById('simEngine')"), '?engine=tab: no in-page engine (waits for a Sim tab)')
    c2.close()
    # ---- 2) layout audit, every page, both orientations
    pages = ['bms_sim.html', 'dash.html?src=sim&engine=tab', 'wiring.html', 'checklist.html', 'perfboard.html?board=coil_driver']
    for (w, h) in [(390, 844), (844, 390)]:
        c = b.new_context(viewport={'width': w, 'height': h}, device_scale_factor=3, is_mobile=True, has_touch=True, user_agent=UA)
        for f_ in pages:
            pg = c.new_page(); u = (root / f_.split('?')[0]).as_uri() + ('?' + f_.split('?')[1] if '?' in f_ else '')
            pg.goto(u); pg.wait_for_timeout(1200)
            views = [None]
            if f_.startswith('bms_sim'): views = pg.evaluate("[...document.querySelectorAll('.tab')].map(x=>x.dataset.t)")
            bad_s, bad_ta, wide = [], [], []
            for v in views:
                if v: pg.evaluate(f"document.querySelector('.tab[data-t=\"{v}\"]').click()"); pg.wait_for_timeout(300)
                r = pg.evaluate(AUDIT, w)
                bad_s += [(v or '') + ':' + x for x in r['small']]; bad_ta += r['noTA']
                if not f_.startswith('dash') and r['scrollW'] > w + 1: wide.append(f'{v or f_} {r["scrollW"]}px')
            nm = f_.split('?')[0] + f' {w}x{h}'
            ok(not wide, f'{nm}: fits the screen width, no sideways scroll ' + repr(wide[:4]))
            ok(not bad_s, f'{nm}: every control at least 44 px on screen ' + repr(bad_s[:6]))
            ok(not bad_ta, f'{nm}: touch-action manipulation on every control (no double-tap zoom) ' + repr(bad_ta[:4]))
            ok(r['safe'] > 0 and r['hover'] == 0, f'{nm}: safe-area insets used ({r["safe"]} rules), no hover-only controls')
            pg.close()
        c.close()
    b.close()
print(f'{n-f}/{n} iphone')
sys.exit(1 if f else 0)
