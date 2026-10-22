"""Shared top tab bar for the 4 published pages (Sim, Dash, Wiring, Checklist). Relative links (gist-safe).
Sim <-> Dash open in named tabs so the sim keeps running while the dash shows it (BroadcastChannel)."""
import html
TABS = [("sim", "Sim", "index.html", "teryx_sim"), ("dash", "Dash", "dash.html?src=sim", "teryx_dash"),
        ("wiring", "Wiring", "wiring.html", ""), ("checklist", "Checklist", "checklist.html", "")]
CSS = """<style id="tnav-css">
#tnav{--k:var(--tnk,1);position:sticky;top:0;z-index:15;display:flex;align-items:center;gap:calc(6px*var(--k));
 height:calc(48px*var(--k));padding:0 calc(8px*var(--k));padding-top:env(safe-area-inset-top);box-sizing:content-box;
 background:rgba(0,0,0,.92);-webkit-backdrop-filter:blur(14px);backdrop-filter:blur(14px);border-bottom:1px solid #1f1f22;
 font:500 calc(15px*var(--k))/1 system-ui,-apple-system,"Inter","Segoe UI",Roboto,sans-serif;overflow-x:auto;white-space:nowrap}
#tnav a{display:flex;align-items:center;min-height:calc(44px*var(--k));padding:0 calc(14px*var(--k));border-radius:calc(10px*var(--k));
 color:#a1a1a8;text-decoration:none;-webkit-tap-highlight-color:transparent}
#tnav a.on{color:#f5f5f7;background:#1c1c1e;box-shadow:inset 0 calc(-2px*var(--k)) 0 #3e6ae1}
#tnav .lbl{margin-left:auto;padding-left:calc(10px*var(--k));color:#5f6368;font-size:calc(11px*var(--k));font-weight:400}
body>header{top:calc(48px*var(--tnk,1) + env(safe-area-inset-top))!important}
@media print{#tnav{display:none}}
</style>"""
JS = """<script>/* keep the bar thumb-sized when the browser shows the page zoomed out (dash fit-to-screen); measured at load/rotate only, not on pinch */
(function(){function k(){const v=window.visualViewport,sc=v&&v.scale>0?v.scale:1;document.documentElement.style.setProperty('--tnk',String(Math.max(1,Math.min(4,1/sc))));}
 k();addEventListener('load',()=>{k();setTimeout(k,600);});addEventListener('orientationchange',()=>setTimeout(k,100));})();</script>"""
def topnav(cur, label):
    a = "".join('<a href="%s"%s%s>%s</a>' % (h, ' class="on" aria-current="page"' if k == cur else "",
                 ' target="%s"' % t if t and k != cur else "", n) for k, n, h, t in TABS)
    return CSS + '<div id="tnav" role="navigation" aria-label="Pages">%s<span class="lbl">%s</span></div>' % (a, html.escape(label)) + JS
