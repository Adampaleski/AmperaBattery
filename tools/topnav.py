"""Shared top tab bar for the 6 published pages (Sim, Dash, Wiring, Boards, Layout, Checklist). Relative links (gist-safe).
Sim <-> Dash open in named tabs so the sim keeps running while the dash shows it (BroadcastChannel)."""
import html
TABS = [("sim", "Sim", "index.html", "teryx_sim"), ("dash", "Dash", "dash.html?src=sim", "teryx_dash"),
        ("wiring", "Wiring", "wiring.html", ""), ("perfboard", "Boards", "perfboard.html", ""), ("layout", "Layout", "layout.html", ""),
        ("checklist", "Checklist", "checklist.html", "")]
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
/* Build 15 phone rules (all 4 pages): no double-tap zoom on controls, thumb-sized controls on touch screens,
   room for the iPhone notch / home bar (safe-area insets; needs viewport-fit=cover). */
a,button,input,select,textarea,label,summary,[role=button],[data-sc],[data-sv]{touch-action:manipulation}
@media (pointer:coarse){
 body button,body select,body summary,body [role=button]{min-height:calc(44px*var(--tnk,1))!important;min-width:calc(44px*var(--tnk,1))!important}
 body input[type=range]{min-height:calc(44px*var(--tnk,1))!important}
 body input[type=number],body input[type=text],body input[type=password],body input[type=search],body textarea{min-height:calc(44px*var(--tnk,1))!important;font-size:max(16px,1em)} /* 16px stops iOS zooming in when a box is tapped */
 body input[type=checkbox],body input[type=radio]{min-width:22px;min-height:22px}
}
body{padding-left:env(safe-area-inset-left);padding-right:env(safe-area-inset-right);padding-bottom:env(safe-area-inset-bottom)}
@media (max-width:560px){#tnav{flex-wrap:wrap;height:auto;min-height:calc(48px*var(--k));white-space:normal;overflow-x:visible}
 #tnav .lbl{flex-basis:100%;margin-left:0;padding:0 0 calc(4px*var(--k)) calc(4px*var(--k))}}
</style>"""
JS = """<script>/* keep the bar thumb-sized when the browser shows the page zoomed out (dash fit-to-screen); measured at load/rotate only, not on pinch */
(function(){function k(){const v=window.visualViewport,sc=v&&v.scale>0?v.scale:1;document.documentElement.style.setProperty('--tnk',String(Math.max(1,Math.min(4,1/sc))));}
 k();addEventListener('load',()=>{k();setTimeout(k,600);});addEventListener('orientationchange',()=>setTimeout(k,100));})();</script>"""
def topnav(cur, label):
    a = "".join('<a href="%s"%s%s>%s</a>' % (h, ' class="on" aria-current="page"' if k == cur else "",
                 ' target="%s"' % t if t and k != cur else "", n) for k, n, h, t in TABS)
    return CSS + '<div id="tnav" role="navigation" aria-label="Pages">%s<span class="lbl">%s</span></div>' % (a, html.escape(label)) + JS
