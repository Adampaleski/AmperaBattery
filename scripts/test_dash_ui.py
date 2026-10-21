# python3 scripts/test_dash_ui.py [shots_dir] — browser UI checks for tools/bms_sim.html + tools/dash.html (Playwright + Chrome)
import sys, os, pathlib
from playwright.sync_api import sync_playwright
root = pathlib.Path(__file__).resolve().parent.parent / 'tools'
shots = sys.argv[1] if len(sys.argv) > 1 else None
n = f = 0
def ok(c, m):
    global n, f
    n += 1
    if not c: f += 1
    print(('PASS' if c else 'FAIL'), 'ui', m)
with sync_playwright() as p:
    b = p.chromium.launch(executable_path=os.environ.get('CHROME', '/usr/bin/google-chrome'), args=['--no-sandbox'])
    pg = b.new_page(viewport={'width': 1366, 'height': 1024})
    errs = []; pg.on('pageerror', lambda e: errs.append(str(e)))
    pg.on('dialog', lambda d: d.accept())
    pg.goto((root / 'bms_sim.html').as_uri()); pg.wait_for_selector('#dashHost .tdash'); pg.wait_for_timeout(600)
    D = '#dashHost '
    ok(pg.is_visible(D + '.simp') and pg.is_visible(D + '.dock'), 'sim: SIMULATOR panel and quick-button dock visible on Dash tab')
    if shots: pg.screenshot(path=f'{shots}/home.png', full_page=True)
    # quick buttons toggle
    pg.click(D + '[data-q="lights"]'); pg.wait_for_timeout(300)
    ok('on' in pg.get_attribute(D + '[data-q="lights"]', 'class'), 'quick Lights toggles on')
    pg.click(D + '[data-q="lights"]'); pg.wait_for_timeout(300)
    ok('on' not in pg.get_attribute(D + '[data-q="lights"]', 'class'), 'quick Lights toggles off')
    pg.click(D + '[data-seat="driver"]'); pg.wait_for_timeout(200)
    ok(pg.inner_text(D + '[data-f="lv_driver"]').startswith('Low'), 'quick seat heater: Off → Low')
    pg.click(D + '[data-q="fan"]'); pg.wait_for_timeout(500)
    ok(pg.inner_text(D + '[data-f="q_fan"]') in ('override', 'on'), 'quick Fan override turns the fan on')
    pg.click(D + '[data-q="fan"]')
    pg.click(D + '[data-q="diff"]'); pg.wait_for_timeout(300)
    ok('in 4WD' in pg.inner_text(D + '[data-f="toast"]'), 'quick Diff lock refused in 2WD with the reason shown')
    pg.click(D + '[data-q="4wd"]'); pg.wait_for_timeout(3500)
    ok(pg.inner_text(D + '[data-f="q_4wd"]') == '4WD', 'quick 4WD engages when stopped')
    pg.click(D + '[data-q="4wd"]'); pg.wait_for_timeout(3500)
    # hard launch: speed / rpm rising
    pg.click(D + '[data-sc="launch"]'); pg.wait_for_timeout(3000)
    s1 = float(pg.inner_text(D + '[data-f="spd"]') or 0); r1 = pg.inner_text(D + '[data-f="rpm"]')
    pg.wait_for_timeout(2500)
    s2 = float(pg.inner_text(D + '[data-f="spd"]') or 0); r2 = pg.inner_text(D + '[data-f="rpm"]')
    ok(s2 > s1 > 0 and float(r2) > float(r1) > 0, f'hard launch: speed {s1}→{s2} mph, RPM {r1}→{r2} rising')
    q4 = pg.get_attribute(D + '[data-q="4wd"]', 'class')
    ok('dis' in q4, '4WD quick button greyed out while moving')
    # regen stop: negative amps
    pg.click(D + '[data-sc="regen"]'); amps = []
    for i in range(8):
        pg.wait_for_timeout(250); amps.append(pg.evaluate('S.packA'))
    ok(min(amps) < -5, f'regen stop: pack amps go negative (min {min(amps):.1f} A)')
    ok('regen' in pg.inner_text(D + '[data-f="sro"]'), 'sim readout shows regen A')
    pg.click(D + '[data-sc="coast"]')
    # menu sections
    pg.click(D + '[data-q="menu"]')
    for k in ['batt', 'chg', 'drive', 'lights', 'wd', 'flt', 'cool', 'veh', 'disp', 'diag', 'about']:
        pg.click(D + f'[data-sec="{k}"]'); pg.wait_for_timeout(250)
        ok(pg.is_visible(D + f'[data-s2="{k}"]'), f'menu opens section {k}')
    pg.click(D + '[data-sec="batt"]'); pg.wait_for_timeout(400)
    ok(pg.locator(D + '.cg i').count() == 144, 'Battery: 144 cells in the grid')
    if shots: pg.screenshot(path=f'{shots}/battery.png')
    pg.click(D + '[data-sec="drive"]'); pg.wait_for_timeout(400)
    ok(pg.locator(D + '.crv svg').count() == 5, 'Drive & Kelly: 5 cutback curves')
    pg.click(D + '[data-sec="veh"]'); pg.wait_for_timeout(300)
    ok('confirm total ratio' in pg.inner_text(D + '[data-s2="veh"]'), 'Vehicle: ratio flagged confirm total ratio')
    if shots: pg.screenshot(path=f'{shots}/menu_vehicle.png')
    small = pg.evaluate("""[...document.querySelectorAll('#dashHost .mnu *')].filter(e=>e.offsetParent&&e.childNodes.length&&[...e.childNodes].some(n=>n.nodeType===3&&n.textContent.trim())&&parseFloat(getComputedStyle(e).fontSize)<18).length""")
    ok(small == 0, f'menu: no visible text under 18 px ({small})')
    pg.click(D + '[data-f="mclose"]')
    ok(not errs, 'no page errors ' + '; '.join(errs[:3]))
    # dash.html real mode: no sim panel; ?src=sim: panel present
    d = b.new_page(viewport={'width': 1366, 'height': 1024}); d.goto((root / 'dash.html').as_uri() + '?src=none'); d.wait_for_timeout(500)
    ok(d.locator('.simp').count() == 0, 'dash.html real mode: no SIMULATOR panel in the DOM')
    d.goto((root / 'dash.html').as_uri() + '?src=sim'); d.wait_for_timeout(500)
    ok(d.locator('.simp').count() == 1, 'dash.html?src=sim: SIMULATOR panel present')
    b.close()
print(f'{n-f}/{n} ui')
sys.exit(1 if f else 0)
